#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Utf8.h"
#include "utils/MiniJson.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

void registerCSVBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("parse_csv", Value::makeNativeFunction("parse_csv", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isString()) { interp.runtimeError("parse_csv() expects string", 0, ""); return Value(); }
                const std::string& str = args[0].asString();
                std::vector<std::vector<std::string>> rows;
                std::vector<std::string> currentRow;
                std::string currentField;
                bool inQuotes = false;
                
                for (size_t i = 0; i < str.length(); ++i) {
                    char c = str[i];
                    if (inQuotes) {
                        if (c == '"') {
                            if (i + 1 < str.length() && str[i + 1] == '"') {
                                currentField += '"';
                                i++;
                            } else {
                                inQuotes = false;
                            }
                        } else {
                            currentField += c;
                        }
                    } else {
                        if (c == '"') {
                            inQuotes = true;
                        } else if (c == ',') {
                            currentRow.push_back(currentField);
                            currentField = "";
                        } else if (c == '\r') {
                            // ignore
                        } else if (c == '\n') {
                            currentRow.push_back(currentField);
                            rows.push_back(currentRow);
                            currentRow.clear();
                            currentField = "";
                        } else {
                            currentField += c;
                        }
                    }
                }
                if (!currentField.empty() || !currentRow.empty()) {
                    currentRow.push_back(currentField);
                    rows.push_back(currentRow);
                }
                
                if (rows.empty()) return Value::makeArray(std::vector<Value>());
                
                std::vector<std::string> headers = rows[0];
                std::vector<Value> result;
                
                for (size_t i = 1; i < rows.size(); ++i) {
                    Value dictVal = Value::makeDictionary();
                    auto dict = dictVal.asDictionaryPtr();
                    dict->modifyMap([&](auto& m) {
                        for (size_t j = 0; j < rows[i].size() && j < headers.size(); ++j) {
                            m[headers[j]] = Value(rows[i][j]);
                        }
                    });
                    result.push_back(dictVal);
                }
                return Value::makeArray(result);
            }));

    

    

    

    interp.defineGlobal("to_csv", Value::makeNativeFunction("to_csv", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("to_csv() expects an array of dictionaries", 0, ""); return Value(); }
                const auto& arr = args[0].asArray().getElementsCopy();
                if (arr.empty()) return Value("");
                
                std::vector<std::string> headers;
                if (arr[0].isDictionary()) {
                    auto m = arr[0].asDictionaryPtr()->getMapCopy();
                    for (auto& kv : m) headers.push_back(kv.first);
                } else {
                    return Value("");
                }
                
                std::string res;
                auto escape = [](const std::string& s) {
                    if (s.find(',') != std::string::npos || s.find('"') != std::string::npos || s.find('\n') != std::string::npos || s.find('\r') != std::string::npos) {
                        std::string esc = "\"";
                        for (char c : s) {
                            if (c == '"') esc += "\"\"";
                            else esc += c;
                        }
                        esc += "\"";
                        return esc;
                    }
                    return s;
                };
                
                for (size_t i = 0; i < headers.size(); i++) {
                    res += escape(headers[i]);
                    if (i < headers.size() - 1) res += ",";
                }
                res += "\n";
                
                for (const auto& rowVal : arr) {
                    if (rowVal.isDictionary()) {
                        auto dictPtr = rowVal.asDictionaryPtr();
                        for (size_t i = 0; i < headers.size(); i++) {
                            Value v = dictPtr->get(headers[i]);
                            std::string sVal = v.isString() ? v.asString() : (v.isNil() ? "" : v.toString());
                            res += escape(sVal);
                            if (i < headers.size() - 1) res += ",";
                        }
                        res += "\n";
                    }
                }
                return Value(res);
            }));

}
