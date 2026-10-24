#pragma once
// ============================================================================
// ВАРИАНТ 8: Система автоматического расчета отпускных выплат.
//
// Вход:  средний месячный заработок (0..100000 руб.) и число дней отпуска
//        (целое, 1..90). Принимаются две формы JSON-тела:
//          {"average_earnings": 60000, "vacation_days": 14}
//          [60000, 14]                      (как в примерах задания)
// Выход: три строки «Сумма отпускных: X руб.», «Средний дневной заработок:
//        Y руб.», «Дней отпуска: Z» и те же величины числами.
//
// Алгоритм (по заданию):
//   1) расчет: отпускные = заработок_дневной * дней_отпуска,
//      где заработок_дневной = средний месячный заработок / 30;
//   2) проверка корректности входных данных (выполняется ДО расчета);
//   3) округление до целых рублей;
//   4) форматирование результата.
//
// Имя namespace TaskVariant менять нельзя: на него опирается task.hpp.
// Зависимости: только стандартная библиотека C++ и json.hpp проекта.
// ============================================================================

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include "../../json.hpp"

namespace TaskVariant {

    // ------------------------------------------------------------------------
    // Константы задачи.
    // ------------------------------------------------------------------------
    inline constexpr double MIN_EARNINGS  = 0.0;       // нижняя граница заработка, руб.
    inline constexpr double MAX_EARNINGS  = 100000.0;  // верхняя граница заработка, руб.
    inline constexpr int    MIN_DAYS      = 1;         // минимальная длительность отпуска
    inline constexpr int    MAX_DAYS      = 90;        // максимальная длительность отпуска
    inline constexpr double DAYS_IN_MONTH = 30.0;      // делитель из контрольных примеров
    inline constexpr double ROUND_EPS     = 1e-6;      // защита от погрешности double при .5

    // ------------------------------------------------------------------------
    // Одно числовое поле запроса: было ли оно передано и является ли числом.
    // Отдельные флаги нужны, чтобы отличить «поле не передано» от «передан 0».
    // ------------------------------------------------------------------------
    struct Field {
        bool   present   = false;  // ключ (или элемент массива) найден
        bool   is_number = false;  // значение является числом JSON
        double value     = 0.0;    // значение, если is_number == true
    };

    // ------------------------------------------------------------------------
    // Структура входных данных. Соответствует полям JSON-запроса.
    // ------------------------------------------------------------------------
    struct Input {
        Field       earnings;      // средний месячный заработок, руб.
        Field       days;          // количество дней отпуска
        std::string format_error;  // ошибка структуры тела (не JSON, лишние символы и т.п.)
    };

    // ------------------------------------------------------------------------
    // Структура выходных данных. Соответствует полям JSON-ответа.
    // ------------------------------------------------------------------------
    struct Output {
        long long   vacation_pay;     // сумма отпускных, руб. (округлена)
        long long   daily_earnings;   // средний дневной заработок, руб. (округлен)
        int         vacation_days;    // продолжительность отпуска, дней
        std::string vacation_pay_text;
        std::string daily_earnings_text;
        std::string duration_text;
    };

    // ========================================================================
    // Разбор JSON. Поддерживается плоский объект или массив чисел.
    // ========================================================================
    namespace detail {

        inline void skip_spaces(const std::string& s, size_t& pos) {
            while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
        }

        // Число по грамматике JSON: -?(0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?[0-9]+)?
        // Строгая проверка отсекает то, что принял бы strtod: inf, nan, 0x10, +5.
        inline bool read_number(const std::string& s, size_t& pos, double& out) {
            size_t start = pos;
            if (pos < s.size() && s[pos] == '-') ++pos;
            if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos]))) {
                pos = start;
                return false;
            }
            if (s[pos] == '0') {
                ++pos;
            } else {
                while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
            }
            if (pos < s.size() && s[pos] == '.') {
                ++pos;
                if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos]))) {
                    pos = start;
                    return false;
                }
                while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
            }
            if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
                ++pos;
                if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
                if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos]))) {
                    pos = start;
                    return false;
                }
                while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
            }
            out = std::strtod(s.substr(start, pos - start).c_str(), nullptr);
            return true;
        }

        // Строка JSON в двойных кавычках с учетом экранирования \" и \\.
        inline bool read_string(const std::string& s, size_t& pos, std::string& out) {
            if (pos >= s.size() || s[pos] != '"') return false;
            ++pos;
            out.clear();
            while (pos < s.size() && s[pos] != '"') {
                if (s[pos] == '\\' && pos + 1 < s.size()) ++pos;
                out += s[pos++];
            }
            if (pos >= s.size()) return false;
            ++pos;  // закрывающая кавычка
            return true;
        }

        // Значение поля: число, строка, true/false/null. Вложенные объекты и
        // массивы в значениях не нужны по заданию и считаются ошибкой формата.
        inline bool read_value(const std::string& s, size_t& pos, Field& field) {
            field.present = true;
            double number = 0.0;
            if (read_number(s, pos, number)) {
                field.is_number = true;
                field.value = number;
                return true;
            }
            std::string text;
            if (read_string(s, pos, text)) return true;
            for (const char* word : {"true", "false", "null"}) {
                std::string w(word);
                if (s.compare(pos, w.size(), w) == 0) {
                    pos += w.size();
                    return true;
                }
            }
            return false;
        }

        // Ключи, под которыми принимаются поля (английские и из текста задания).
        inline bool is_earnings_key(const std::string& key) {
            return key == "average_earnings" || key == "средний_заработок" ||
                   key == "средний_дневной_заработок";
        }

        inline bool is_days_key(const std::string& key) {
            return key == "vacation_days" || key == "дней_отпуска";
        }

        inline bool parse_object(const std::string& s, size_t& pos, Input& in) {
            ++pos;  // '{'
            skip_spaces(s, pos);
            if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
            while (true) {
                skip_spaces(s, pos);
                std::string key;
                if (!read_string(s, pos, key)) return false;
                skip_spaces(s, pos);
                if (pos >= s.size() || s[pos] != ':') return false;
                ++pos;
                skip_spaces(s, pos);
                Field value;
                if (!read_value(s, pos, value)) return false;
                if (is_earnings_key(key))  in.earnings = value;
                else if (is_days_key(key)) in.days = value;
                skip_spaces(s, pos);
                if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
                if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
                return false;
            }
        }

        inline bool parse_array(const std::string& s, size_t& pos, Input& in) {
            ++pos;  // '['
            std::vector<Field> items;
            skip_spaces(s, pos);
            if (pos < s.size() && s[pos] == ']') { ++pos; }
            else {
                while (true) {
                    skip_spaces(s, pos);
                    Field item;
                    if (!read_value(s, pos, item)) return false;
                    items.push_back(item);
                    skip_spaces(s, pos);
                    if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
                    if (pos < s.size() && s[pos] == ']') { ++pos; break; }
                    return false;
                }
            }
            if (items.size() != 2) {
                in.format_error = "Массив должен содержать ровно два числа: "
                                  "[средний заработок, дней отпуска]";
                return true;
            }
            in.earnings = items[0];
            in.days = items[1];
            return true;
        }

        // Округление до целых рублей по правилу «половина вверх».
        inline long long round_rubles(double amount) {
            return static_cast<long long>(std::floor(amount + 0.5 + ROUND_EPS));
        }

    } // namespace detail

    // ------------------------------------------------------------------------
    // Разбор JSON-тела запроса в Input.
    // Ошибки структуры записываются в format_error, проверка значений
    // выполняется отдельно в validate().
    // ------------------------------------------------------------------------
    inline Input parse(const std::string& body) {
        Input in;
        size_t pos = 0;
        detail::skip_spaces(body, pos);

        bool ok = false;
        if (pos < body.size() && body[pos] == '{') ok = detail::parse_object(body, pos, in);
        else if (pos < body.size() && body[pos] == '[') ok = detail::parse_array(body, pos, in);

        detail::skip_spaces(body, pos);
        if (!ok || pos != body.size()) {
            in = Input{};
            in.format_error = "Тело запроса должно быть JSON-объектом "
                              "{\"average_earnings\": число, \"vacation_days\": число} "
                              "или массивом [число, число]";
        }
        return in;
    }

    // ------------------------------------------------------------------------
    // Проверка входных данных (шаг 2 алгоритма, выполняется ДО расчета).
    // Возвращает "" если всё ок, иначе текст ошибки.
    // ------------------------------------------------------------------------
    inline std::string validate(const Input& in) {
        if (!in.format_error.empty())
            return in.format_error;
        if (!in.earnings.present)
            return "Не задан средний заработок (average_earnings)";
        if (!in.days.present)
            return "Не задано количество дней отпуска (vacation_days)";
        if (!in.earnings.is_number)
            return "Средний заработок должен быть числом";
        if (!in.days.is_number)
            return "Количество дней отпуска должно быть числом";
        if (!std::isfinite(in.earnings.value) ||
            in.earnings.value < MIN_EARNINGS || in.earnings.value > MAX_EARNINGS)
            return "Средний заработок должен быть в диапазоне от 0 до 100000 руб.";
        if (!std::isfinite(in.days.value) || in.days.value != std::floor(in.days.value))
            return "Количество дней отпуска должно быть целым числом";
        if (in.days.value < MIN_DAYS || in.days.value > MAX_DAYS)
            return "Количество дней отпуска должно быть в диапазоне от 1 до 90";
        return "";
    }

    // ------------------------------------------------------------------------
    // Основные расчеты: шаги 1, 3 и 4 алгоритма. Вызывается только для
    // данных, прошедших validate().
    // ------------------------------------------------------------------------
    inline Output calculate(const Input& in) {
        Output out{};
        double earnings = in.earnings.value;
        int days = static_cast<int>(in.days.value);

        // Шаг 1. Расчет без промежуточного округления.
        double daily_exact = earnings / DAYS_IN_MONTH;
        double pay_exact   = earnings * days / DAYS_IN_MONTH;  // = daily_exact * days

        // Шаг 3. Округление до целых рублей.
        out.daily_earnings = detail::round_rubles(daily_exact);
        out.vacation_pay   = detail::round_rubles(pay_exact);
        out.vacation_days  = days;

        // Шаг 4. Форматирование результата.
        out.vacation_pay_text   = "Сумма отпускных: " + std::to_string(out.vacation_pay) + " руб.";
        out.daily_earnings_text = "Средний дневной заработок: " +
                                  std::to_string(out.daily_earnings) + " руб.";
        out.duration_text       = "Дней отпуска: " + std::to_string(out.vacation_days);
        return out;
    }

    // ------------------------------------------------------------------------
    // Формирование JSON-ответа из Output.
    // ------------------------------------------------------------------------
    inline std::string to_json(const Output& out) {
        JArr result;
        for (const std::string* text : {&out.vacation_pay_text, &out.daily_earnings_text,
                                        &out.duration_text})
            result.add("\"" + jesc(*text) + "\"");

        JObj o;
        o.str("vacation_pay", out.vacation_pay_text);
        o.str("daily_earnings", out.daily_earnings_text);
        o.str("duration", out.duration_text);
        o.raw("result", result.str());
        o.int_("vacation_pay_rub", out.vacation_pay);
        o.int_("daily_earnings_rub", out.daily_earnings);
        o.int_("vacation_days", out.vacation_days);
        return o.str();
    }

    // ========================================================================
    // Автотесты модуля (20 проверок): контрольные примеры, граничные значения,
    // неполные данные, неверные типы и формат.
    // ========================================================================
    struct TestCase {
        std::string name;
        bool        passed;
    };

    // Прогон тела запроса через parse -> validate -> calculate.
    // Возвращает текст ошибки ("" при успехе) и заполняет out.
    inline std::string process(const std::string& body, Output& out) {
        Input in = parse(body);
        std::string err = validate(in);
        if (err.empty()) out = calculate(in);
        return err;
    }

    inline bool expect_ok(const std::string& body, long long pay, long long daily, int days) {
        Output out{};
        return process(body, out).empty() && out.vacation_pay == pay &&
               out.daily_earnings == daily && out.vacation_days == days;
    }

    inline bool expect_error(const std::string& body, const std::string& fragment) {
        Output out{};
        std::string err = process(body, out);
        return !err.empty() && err.find(fragment) != std::string::npos;
    }

    inline std::vector<TestCase> run_self_tests() {
        Output example{};
        process("[60000, 14]", example);

        return {
            // Корректные данные и контрольные примеры задания
            {"Пример 1: [60000, 14] -> 28000 / 2000 / 14", expect_ok("[60000, 14]", 28000, 2000, 14)},
            {"Пример 2: [90000, 28] -> 84000 / 3000 / 28", expect_ok("[90000, 28]", 84000, 3000, 28)},
            {"Объект JSON эквивалентен массиву",
             expect_ok("{\"average_earnings\": 60000, \"vacation_days\": 14}", 28000, 2000, 14)},
            {"Ключи из текста задания",
             expect_ok("{\"средний_дневной_заработок\": 90000, \"дней_отпуска\": 28}", 84000, 3000, 28)},
            {"Округление: 50000 руб., 14 дн. -> 23333 / 1667",
             expect_ok("[50000, 14]", 23333, 1667, 14)},
            {"Округление половины вверх: 45015 руб., 1 дн. -> 1501",
             expect_ok("[45015, 1]", 1501, 1501, 1)},
            {"Формат строки результата",
             example.vacation_pay_text == "Сумма отпускных: 28000 руб." &&
             example.daily_earnings_text == "Средний дневной заработок: 2000 руб." &&
             example.duration_text == "Дней отпуска: 14"},

            // Граничные значения
            {"Граница: заработок 0 допустим", expect_ok("[0, 10]", 0, 0, 10)},
            {"Граница: заработок 100000 допустим", expect_ok("[100000, 90]", 300000, 3333, 90)},
            {"Граница: 1 день допустим", expect_ok("[30000, 1]", 1000, 1000, 1)},
            {"Граница: 0 дней отклоняется", expect_error("[30000, 0]", "от 1 до 90")},
            {"Граница: 91 день отклоняется", expect_error("[30000, 91]", "от 1 до 90")},
            {"Граница: заработок -0.01 отклоняется", expect_error("[-0.01, 10]", "от 0 до 100000")},
            {"Граница: заработок 100000.01 отклоняется", expect_error("[100000.01, 10]", "от 0 до 100000")},

            // Неполные данные
            {"Пустое тело запроса", expect_error("", "JSON")},
            {"Нет поля vacation_days", expect_error("{\"average_earnings\": 60000}", "дней отпуска")},
            {"Массив из одного числа", expect_error("[60000]", "ровно два числа")},

            // Неверные данные
            {"Заработок строкой", expect_error("{\"average_earnings\": \"60000\", \"vacation_days\": 14}",
                                              "должен быть числом")},
            {"Дробное число дней", expect_error("[60000, 14.5]", "целым числом")},
            {"Не JSON", expect_error("average_earnings=60000", "JSON")},
        };
    }

} // namespace TaskVariant
