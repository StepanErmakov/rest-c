# Описание проекта (REST-API C++ RBAC)

Проект собирается **без внешних зависимостей** (кроме SQLite) и работает
на Windows, Linux и macOS

[ТЗ для индивидуального выполнения](https://github.com/mal-corp/rest-c/blob/main/src/modules/individual/README.md)

---

## Возможности

- **Многопоточность** — `httplib` обрабатывает каждый запрос в отдельном
  потоке из пула.
- **Авторизация через Bearer-токены** — с ограниченным временем жизни
  (TTL Time to Live настраивается).
- **RBAC** — Role-Based Access Control, три роли: `user`, `admin`, `owner`. Доступ к защищённым
  эндпоинтам проверяется.
- **Безопасные пароли** — соль + Secure Hash Algorithm SHA-256. В БД пароли в открытом виде
  не хранятся.
- **SQL-защита** — Structured Query Language, все запросы параметризованы через `?` (нет
  SQL-инъекций).
- **JSON** — JavaScript Object Notation все ответы возвращаются в формате JSON.
- **Единый формат ошибок** — middleware перехватывает исключения и
  стандартизирует ответы.
- **CORS** — Cross-Origin Resource Sharing, браузерные клиенты (fetch/AJAX) работают из коробки.
- **Healthcheck** — `GET /health` для мониторинга.
- **Логирование** — каждый запрос пишется в консоль и в таблицу `logs`.

---

## Структура проекта

```
REST-API/
├── CMakeLists.txt                          Сборка
├── main.cpp                                Точка входа + middleware
├── README.md                               Этот файл
└── src/
    ├── json.hpp                            Сборка JSON-ответов
    ├── crypto.hpp                          SHA-256, пароли, токены
    ├── database.hpp                        Работа с SQLite + конфиг
    │
    ├── routes/
    │   ├── auth.hpp                        8 эндпоинтов авторизации
    │   └── public.hpp                      Счётчики, история, admin-эндпоинты
    │
    ├── modules/
    │   ├── example/
    │   │   └── stats.hpp                   Демо: статистика (как пример)
    │   └── individual/
    │       ├── task.hpp                    Каркас индивидуального задания (не трогать)
    │       ├── variant_roi.hpp             Пример: ROMI/ROAS
    │       └── README.md                   ТЗ-Инструкция к выполнению
    │
    └── lib/
        ├── httplib.h                       Сторонний HTTP-движок
        └── sqlite_loader.hpp               Загрузчик SQLite
```


---

## Быстрый старт

### Windows (w64devkit)

Проект протестирован с портативным GCC-дистрибутивом
[w64devkit](https://github.com/skeeto/w64devkit/releases).

```powershell
# 1. Убедитесь, что sqlite3.dll лежит рядом с .exe, ИЛИ
#    используется встроенный winsqlite3.dll (Windows 10+).

# 2. Сборка
cd C:\wp\cpp\REST-API
Remove-Item -Recurse -Force CMakeCache.txt, CMakeFiles, build.ninja, .ninja_deps, .ninja_log, cmake_install.cmake -ErrorAction SilentlyContinue
cmake .
cmake --build .

# 3. Запуск
.\rest_server.exe
```

### Linux (Ubuntu/Debian) 
[cmake](https://cmake.org/download/).

```bash
sudo apt update
sudo apt install -y build-essential cmake sqlite3 libsqlite3-dev

cd REST-API
cmake .
cmake --build .
./rest_server
```

### macOS

```bash
xcode-select --install
brew install cmake sqlite

cd REST-API
cmake .
cmake --build .
./rest_server
```

Успешный запуск:

```
=============================================
  Server running on http://localhost:8080
  Healthcheck: http://localhost:8080/health
=============================================
```

### Стартовые пользователи

При первом запуске создаются три аккаунта:

| Логин    | Пароль   | Роль    |
|----------|----------|---------|
| `owner1` | `owner1` | owner   |
| `admin1` | `admin1` | admin   |
| `user1`  | `user1`  | user    |


### Web-коллекция для тестирования и работы: `frontend\index.html`

---

## Безопасность

| Механизм | Реализация |
|----------|-----------|
| Хеширование паролей | Соль + SHA-256 (`<salt>$<hash>`) |
| Генерация токенов | `std::mt19937_64` + `std::random_device` |
| SQL-инъекции | Все запросы параметризованы через `?` |
| JSON-инъекции | Все строки проходят через `jesc()` |
| RBAC | Проверка роли в защищённых эндпоинтах |
| TTL токена | Сессии истекают через `TOKEN_TTL_SECONDS` |
| Многопоточность | Мьютекс `db_lock` на всех запросах к БД |
| Размер запроса | Ограничение 1 МБ в `main.cpp` |

**Для продакшена дополнительно:**
- Использовать PBKDF2/bcrypt/argon2 вместо SHA-256.
- Настроить HTTPS (TLS).
- Разрешить CORS не для `*`, а для конкретных доменов.
- Ограничить rate-limit по IP.

---


## Тестирование

### Проверка SQL-инъекций

```bash
curl -X POST http://localhost:8080/api/v1.1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"'"'"' OR 1=1 --","password":"x"}'
# → 401 Invalid credentials (инъекция не проходит)
```

### Проверка RBAC

```bash
# Логин как обычный user
curl -X POST http://localhost:8080/api/v1.1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"user1","password":"user1"}'

# Попытка получить список пользователей — 403
curl http://localhost:8080/api/v1.1/auth/users \
  -H "Authorization: Bearer <USER_TOKEN>"
```


## Решение проблем

### `sqlite3.dll not found`

**Причина:** Windows не нашла SQLite.
**Решение:**
- Скачайте `sqlite3.dll` с https://www.sqlite.org/download.html
  (раздел **Precompiled Binaries for Windows**).
- Положите рядом с `rest_server.exe`.
- **Или** запустите на Windows 10/11 — подхватится встроенный
  `winsqlite3.dll` из `C:\Windows\System32`.

### `[ERROR] bind: Address already in use`

**Причина:** порт 8080 занят другим процессом.
**Решение:**

```powershell
Get-Process | Where-Object { $_.ProcessName -match "rest_server" }
Stop-Process -Name rest_server -Force
```

Или измените порт в `main.cpp`:

```cpp
svr.listen("0.0.0.0", 8081);
```

### 401 Unauthorized после логина

**Причины:**
1. Токен просрочен (по умолчанию 5 минут).
2. Сервер был перезапущен — таблица `sessions` очищается при старте.
3. В заголовке пропущен `Bearer `.

**Решение:** залогиньтесь заново, скопируйте свежий токен.

### 403 Forbidden на `/auth/users`

**Причина:** у текущего пользователя роль `user`, а не `admin`/`owner`.
**Решение:** залогиньтесь как `admin1` / `admin1`.


### Очистить базу

```powershell
# Остановите сервер, удалите файл, перезапустите
Remove-Item data\rest.db
.\rest_server.exe
```

При следующем запуске схема пересоздастся, стартовые пользователи
появятся снова.

---

## Лицензия

Учебный проект. Сторонние библиотеки:
- [`cpp-httplib`](https://github.com/yhirose/cpp-httplib) — MIT.
- [`SQLite`](https://sqlite.org) — Public Domain.
---

## Индивидуальный вариант 8. Система автоматического расчета отпускных выплат

Модуль: `src/modules/individual/variant_mine.hpp`, подключен в `task.hpp`.

**Вход** (`POST /api/v1.1/works/calc-individual`, токен обязателен), две равноценные формы:

```json
{"average_earnings": 60000, "vacation_days": 14}
[60000, 14]
```

| Поле | Тип | Ограничение |
|------|-----|-------------|
| `average_earnings` | число | средний месячный заработок, от 0 до 100000 руб. |
| `vacation_days` | целое | от 1 до 90 |

**Расчет:** дневной заработок = заработок / 30; отпускные = дневной заработок × дни;
результаты округляются до целых рублей (половина вверх) после расчета.

**Выход:**

```json
{"vacation_pay":"Сумма отпускных: 28000 руб.",
 "daily_earnings":"Средний дневной заработок: 2000 руб.",
 "duration":"Дней отпуска: 14",
 "result":["Сумма отпускных: 28000 руб.","Средний дневной заработок: 2000 руб.","Дней отпуска: 14"],
 "vacation_pay_rub":28000,"daily_earnings_rub":2000,"vacation_days":14}
```

При ошибке входных данных: `400 {"error": "<описание>"}`.

**Автотесты модуля** (20 проверок): `GET /api/v1.1/admin/run-tests-individual`, роль admin или owner.

### Исправленные ошибки исходного проекта

| Ошибка | Исправление |
|--------|-------------|
| Linux, macOS: CMake младше 4.3 не находит цель `SQLite3::SQLite3` | выбор `SQLite3::SQLite3` или `SQLite::SQLite3` (`CMakeLists.txt`) |
| Обработчик ошибок затирал тело ответа `{"error": ...}` | тело заполняется, только если оно пустое (`main.cpp`) |
| Без токена `/auth/users`, `/admin/stats` отвечали 403 | 401 без токена, 403 при нехватке прав |
| admin менял пароль и удалял admin/owner | `Auth::can_manage`: admin управляет собой и ролью user, owner всеми |
| Смена пароля несуществующему пользователю давала 200 | 404 User not found |
| `?limit=-1` снимал ограничение истории | `parse_history_limit`: limit в диапазоне [1, 20] |
| Пустая дата в журнале `logs` | `write_log` записывает `datetime` |
| `failed_validations` всегда 0 | считается по записям `VALIDATION_ERROR` журнала |
| История своего варианта показывала расчеты других модулей | столбец `works.module` |
