#include "../Builtins.h"
#include "../Interpreter.h"

#include <sqlite3.h>
#include <unordered_map>
#include <string>
#include <vector>

static std::unordered_map<int, sqlite3*> dbConnections;
static int nextDbHandle = 1;

void registerDBBuiltins(Interpreter& interp) {
    interp.defineGlobal("dbOpen", Value::makeNativeFunction("dbOpen", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("dbOpen() expects string path", 0, ""); return Value(); }
            std::string path = args[0].asString();
            
            sqlite3* db;
            int rc = sqlite3_open(path.c_str(), &db);
            if (rc != SQLITE_OK) {
                std::string err = sqlite3_errmsg(db);
                sqlite3_close(db);
                interp.runtimeError("sqlite3_open failed: " + err, 0, ""); return Value();
            }
            
            int handle = nextDbHandle++;
            dbConnections[handle] = db;
            return Value((double)handle);
        }));

    interp.defineGlobal("dbExec", Value::makeNativeFunction("dbExec", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) { interp.runtimeError("dbExec() expects at least 2 arguments", 0, ""); return Value(); }
            if (!args[0].isNumber()) { interp.runtimeError("dbExec() expects number handle", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.runtimeError("dbExec() expects string SQL", 0, ""); return Value(); }
            
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) {
                interp.runtimeError("Invalid database handle", 0, ""); return Value();
            }
            
            sqlite3* db = dbConnections[handle];
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, args[1].asString().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                interp.runtimeError("sqlite3_prepare_v2 failed: " + std::string(sqlite3_errmsg(db)), 0, ""); return Value();
            }
            
            if (args.size() > 2 && args[2].isArray()) {
                const auto& params = args[2].asArray();
                for (int i = 0; i < (int)params.size(); i++) {
                    const auto& p = params[i];
                    int idx = i + 1;
                    if (p.isNil()) sqlite3_bind_null(stmt, idx);
                    else if (p.isBool()) sqlite3_bind_int(stmt, idx, p.asBool() ? 1 : 0);
                    else if (p.isNumber()) sqlite3_bind_double(stmt, idx, p.asNumber());
                    else if (p.isString()) sqlite3_bind_text(stmt, idx, p.asString().c_str(), -1, SQLITE_TRANSIENT);
                    else sqlite3_bind_text(stmt, idx, p.toString().c_str(), -1, SQLITE_TRANSIENT);
                }
            }
            
            int rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
                interp.runtimeError("sqlite3_step failed: " + std::string(sqlite3_errmsg(db)), 0, ""); return Value();
            }
            
            return Value(true);
        }));

    interp.defineGlobal("dbQuery", Value::makeNativeFunction("dbQuery", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) { interp.runtimeError("dbQuery() expects at least 2 arguments", 0, ""); return Value(); }
            if (!args[0].isNumber()) { interp.runtimeError("dbQuery() expects number handle", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.runtimeError("dbQuery() expects string SQL", 0, ""); return Value(); }
            
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) {
                interp.runtimeError("Invalid database handle", 0, ""); return Value();
            }
            
            sqlite3* db = dbConnections[handle];
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, args[1].asString().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                interp.runtimeError("sqlite3_prepare_v2 failed: " + std::string(sqlite3_errmsg(db)), 0, ""); return Value();
            }
            
            if (args.size() > 2 && args[2].isArray()) {
                const auto& params = args[2].asArray();
                for (int i = 0; i < (int)params.size(); i++) {
                    const auto& p = params[i];
                    int idx = i + 1;
                    if (p.isNil()) sqlite3_bind_null(stmt, idx);
                    else if (p.isBool()) sqlite3_bind_int(stmt, idx, p.asBool() ? 1 : 0);
                    else if (p.isNumber()) sqlite3_bind_double(stmt, idx, p.asNumber());
                    else if (p.isString()) sqlite3_bind_text(stmt, idx, p.asString().c_str(), -1, SQLITE_TRANSIENT);
                    else sqlite3_bind_text(stmt, idx, p.toString().c_str(), -1, SQLITE_TRANSIENT);
                }
            }
            
            std::vector<Value> results;
            int colCount = sqlite3_column_count(stmt);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Value row = Value::makeDictionary();
                auto& rowMap = row.asDictionary().map;
                
                for (int i = 0; i < colCount; i++) {
                    const char* name = sqlite3_column_name(stmt, i);
                    std::string colName = name ? name : "col_" + std::to_string(i);
                    int type = sqlite3_column_type(stmt, i);
                    
                    if (type == SQLITE_INTEGER) {
                        rowMap[colName] = Value((double)sqlite3_column_int64(stmt, i));
                    } else if (type == SQLITE_FLOAT) {
                        rowMap[colName] = Value(sqlite3_column_double(stmt, i));
                    } else if (type == SQLITE_TEXT) {
                        const char* text = (const char*)sqlite3_column_text(stmt, i);
                        rowMap[colName] = Value(text ? text : "");
                    } else if (type == SQLITE_NULL) {
                        rowMap[colName] = Value();
                    } else {
                        const char* text = (const char*)sqlite3_column_text(stmt, i);
                        rowMap[colName] = Value(text ? text : "");
                    }
                }
                results.push_back(row);
            }
            
            sqlite3_finalize(stmt);
            return Value::makeArray(results);
        }));

    interp.defineGlobal("dbClose", Value::makeNativeFunction("dbClose", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("dbClose() expects number handle", 0, ""); return Value(); }
            int handle = (int)args[0].asNumber();
            auto it = dbConnections.find(handle);
            if (it != dbConnections.end()) {
                sqlite3_close(it->second);
                dbConnections.erase(it);
            }
            return Value(true);
        }));

    interp.defineGlobal("dbLastInsertId", Value::makeNativeFunction("dbLastInsertId", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("dbLastInsertId() expects number handle", 0, ""); return Value(); }
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) { interp.runtimeError("Invalid database handle", 0, ""); return Value(); }
            return Value((double)sqlite3_last_insert_rowid(dbConnections[handle]));
        }));

    interp.defineGlobal("dbBegin", Value::makeNativeFunction("dbBegin", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("dbBegin() expects number handle", 0, ""); return Value(); }
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) { interp.runtimeError("Invalid database handle", 0, ""); return Value(); }
            sqlite3_exec(dbConnections[handle], "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
            return Value(true);
        }));

    interp.defineGlobal("dbCommit", Value::makeNativeFunction("dbCommit", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("dbCommit() expects number handle", 0, ""); return Value(); }
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) { interp.runtimeError("Invalid database handle", 0, ""); return Value(); }
            sqlite3_exec(dbConnections[handle], "COMMIT", nullptr, nullptr, nullptr);
            return Value(true);
        }));

    interp.defineGlobal("dbRollback", Value::makeNativeFunction("dbRollback", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("dbRollback() expects number handle", 0, ""); return Value(); }
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) { interp.runtimeError("Invalid database handle", 0, ""); return Value(); }
            sqlite3_exec(dbConnections[handle], "ROLLBACK", nullptr, nullptr, nullptr);
            return Value(true);
        }));

    auto globalEnv = interp.getGlobalEnv();
    interp.defineGlobal("db_open", globalEnv->get("dbOpen", 0));
    interp.defineGlobal("db_execute", globalEnv->get("dbExec", 0));
    interp.defineGlobal("db_query", globalEnv->get("dbQuery", 0));
    interp.defineGlobal("db_close", globalEnv->get("dbClose", 0));
    interp.defineGlobal("db_last_insert_id", globalEnv->get("dbLastInsertId", 0));
    interp.defineGlobal("db_begin", globalEnv->get("dbBegin", 0));
    interp.defineGlobal("db_commit", globalEnv->get("dbCommit", 0));
    interp.defineGlobal("db_rollback", globalEnv->get("dbRollback", 0));
}
