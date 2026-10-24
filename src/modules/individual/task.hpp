#pragma once
// ============================================================================
// МОДУЛЬ INDIVIDUAL: каркас роута.
// Не содержит математики, только вызовы функций варианта.
// ============================================================================

#include <string>
#include "../../lib/httplib.h"
#include "../../database.hpp"
#include "../../json.hpp"
#include "../../routes/auth.hpp"

// ────────────────────────────────────────────────────────────────────────────
// ПОДКЛЮЧИТЕ СВОЙ ВАРИАНТ ЗДЕСЬ (замените одну строку).
// ────────────────────────────────────────────────────────────────────────────
// #include "variant_roi.hpp"
#include "variant_mine.hpp"

namespace Individual {
    using namespace std;
    using namespace httplib;

    inline void init_routes(Server& svr) {

        // ─── POST /works/calc-individual ────────────────────────────────────
        svr.Post("/api/v1.1/works/calc-individual", [](const Request& req, Response& res) {
            string role, name;
            int uid = Auth::auth(req, role, name);
            if (!uid) { res.status = 401; return; }

            auto in = TaskVariant::parse(req.body);

            string err = TaskVariant::validate(in);
            if (!err.empty()) {
                write_log("VALIDATION_ERROR", "calc-individual: " + err);
                JObj o; o.str("error", err);
                res.status = 400;
                res.set_content(o.str(), "application/json");
                return;
            }

            auto out = TaskVariant::calculate(in);
            string result = TaskVariant::to_json(out);

            db_run("INSERT INTO works (user_id,datetime,input_json,result_json,module) "
                   "VALUES (?,?,?,?,?);",
                   {to_string(uid), now_iso(), req.body, result, MODULE_INDIVIDUAL});
            res.set_content(result, "application/json");
        });

        // ─── GET /works/history-individual ──────────────────────────────────
        svr.Get("/api/v1.1/works/history-individual", [](const Request& req, Response& res) {
            string role, name;
            int uid = Auth::auth(req, role, name);
            if (!uid) { res.status = 401; return; }

            int limit = parse_history_limit(req.get_param_value("limit"));

            JArr arr;
            for (auto& r : db_all(
                "SELECT id,datetime,input_json,result_json FROM works "
                "WHERE user_id=? AND module=? ORDER BY id DESC LIMIT ?;",
                {to_string(uid), MODULE_INDIVIDUAL, to_string(limit)})) {
                JObj o;
                o.int_("id", stoll(r[0]));
                o.str("datetime", r[1]);
                o.raw("input", r[2]);
                o.raw("result", r[3]);
                arr.add(o.str());
            }
            res.set_content(arr.str(), "application/json");
        });

        // ─── GET /admin/run-tests-individual ────────────────────────────────
        // Автотесты модуля варианта: 20 проверок. Доступ: admin / owner.
        svr.Get("/api/v1.1/admin/run-tests-individual", [](const Request& req, Response& res) {
            string role, name;
            if (!Auth::auth(req, role, name)) { res.status = 401; return; }
            if (!Auth::admin(role))           { res.status = 403; return; }

            auto tests = TaskVariant::run_self_tests();
            int passed = 0;
            JArr cases;
            for (size_t i = 0; i < tests.size(); ++i) {
                if (tests[i].passed) passed++;
                JObj o;
                o.int_("tc_id", (long long)(i + 1));
                o.str("name", tests[i].name);
                o.str("status", tests[i].passed ? "PASSED" : "FAILED");
                cases.add(o.str());
            }

            int total = (int)tests.size();
            JObj rep;
            rep.str("status", passed == total ? "success" : "failed");
            rep.int_("total_tests", total);
            rep.int_("passed", passed);
            rep.int_("failed", total - passed);
            rep.raw("test_cases", cases.str());
            res.set_content(rep.str(), "application/json");
        });
    }
}