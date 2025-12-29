#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <fstream>
#include <cctype>
#include <memory>
#include <climits>
#include <functional>
#include <chrono>
#include <thread>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")



using namespace std;
namespace fs = std::filesystem;



enum Token_Type
{
    NUMBER,
    STRING,
    IDENT,
    T_BOOL,
    PLUS,
    MINUS,
    MUL,
    T_DIV,
    INTT_DIV,
    MOD,
    ASSIGN,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COMMA,
    IF,
    ELSE,
    FT_OR,
    TO,
    UNTIL,
    FUNC,
    RETURN,
    PRINT,
    T_INPUT,
    EQ,
    LT,
    LTE,
    GT,
    GTE,
    NEQ,
    T_AND,
    T_OR,
    T_NOT,
    BREAK,
    CONTINUE,
    INC,
    DEC,
    PLUSEQ,
    MINUSEQ,
    MULEQ,
    T_DIVEQ,
    MODEQ,
    GET,
    USE,
    END,
    POWER
};

struct Token
{
    Token_Type type;
    string text;
    int line;
};





class InterpreterException : public exception
{
    string msg;

public:
    InterpreterException(const string &message, int line)
    {
        msg = "Error on line " + to_string(line) + ": " + message;
    }
    const char *what() const noexcept override
    {
        return msg.c_str();
    }
};

void error(const string &msg, int line)
{
    throw InterpreterException(msg, line);
}


string readFile(const string &name)
{
    ifstream f(name);
    if (!f)
    {
        cerr << "Error: File not found: " << name << endl;
        exit(1);
    }
    stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}


vector<Token> tokenize(const string &src)
{
    vector<Token> tokens;
    int line = 1;

    for (size_t i = 0; i < src.size();)
    {
        if (src[i] == '\n')
        {
            line++;
            i++;
            continue;
        }
        if (isspace(src[i]))
        {
            i++;
            continue;
        }

        // Comment handling
        if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/')
        {
            // Check if it's integer division (//) vs comment (// )
            if (i + 2 < src.size() && src[i + 2] != '/' && !isspace(src[i + 2]))
            {
                tokens.push_back({INTT_DIV, "//", line});
                i += 2;
                continue;
            }
            // It's a comment, skip to end of line
            while (i < src.size() && src[i] != '\n')
                i++;
            continue;
        }

        // Number tokenization - FIXED VERSION
        // Only treat '-' as part of number if it appears after operators or at start
        bool canBeNegative = tokens.empty() || 
                            tokens.back().type == LPAREN ||
                            tokens.back().type == LBRACKET ||
                            tokens.back().type == COMMA ||
                            tokens.back().type == ASSIGN ||
                            tokens.back().type == PLUS ||
                            tokens.back().type == MINUS ||
                            tokens.back().type == MUL ||
                            tokens.back().type == T_DIV ||
                            tokens.back().type == INTT_DIV ||
                            tokens.back().type == MOD ||
                            tokens.back().type == POWER ||
                            tokens.back().type == EQ ||
                            tokens.back().type == NEQ ||
                            tokens.back().type == LT ||
                            tokens.back().type == GT ||
                            tokens.back().type == LTE ||
                            tokens.back().type == GTE ||
                            tokens.back().type == T_AND ||
                            tokens.back().type == T_OR ||
                            tokens.back().type == T_NOT ||
                            tokens.back().type == RETURN;

        if (isdigit(src[i]) || (src[i] == '-' && canBeNegative && i + 1 < src.size() && isdigit(src[i + 1])))
        {
            string n;
            bool hasDot = false;
            if (src[i] == '-')
                n += src[i++];
            while (i < src.size() && (isdigit(src[i]) || src[i] == '.'))
            {
                if (src[i] == '.')
                {
                    if (hasDot)
                        error("Invalid number format", line);
                    hasDot = true;
                }
                n += src[i++];
            }
            tokens.push_back({NUMBER, n, line});
            continue;
        }

        // String tokenization
        if (src[i] == '"')
        {
            i++;
            string s;
            while (i < src.size() && src[i] != '"')
            {
                if (src[i] == '\\' && i + 1 < src.size())
                {
                    i++;
                    if (src[i] == 'n')
                        s += '\n';
                    else if (src[i] == 't')
                        s += '\t';
                    else if (src[i] == '\\')
                        s += '\\';
                    else if (src[i] == '"')
                        s += '"';
                    else
                        s += src[i];
                    i++;
                }
                else
                {
                    s += src[i++];
                }
            }
            if (i >= src.size())
                error("Unterminated string", line);
            i++;
            tokens.push_back({STRING, s, line});
            continue;
        }

        // Comparison keyword operators
        if (i + 11 <= src.size() && src.substr(i, 11) == "greaterthan")
        {
            tokens.push_back({GTE, ">=", line});
            i += 11;
            continue;
        }
        if (i + 11 <= src.size() && src.substr(i, 11) == "greaterthen")
        {
            tokens.push_back({GTE, ">=", line});
            i += 11;
            continue;
        }
        if (i + 8 <= src.size() && src.substr(i, 8) == "lessthan")
        {
            tokens.push_back({LTE, "<=", line});
            i += 8;
            continue;
        }
        if (i + 8 <= src.size() && src.substr(i, 8) == "lessthen")
        {
            tokens.push_back({LTE, "<=", line});
            i += 8;
            continue;
        }
        if (i + 8 <= src.size() && src.substr(i, 8) == "notequal")
        {
            tokens.push_back({NEQ, "!=", line});
            i += 8;
            continue;
        }
        if (i + 7 <= src.size() && src.substr(i, 7) == "greater")
        {
            tokens.push_back({GT, ">", line});
            i += 7;
            continue;
        }
        if (i + 5 <= src.size() && src.substr(i, 5) == "equal")
        {
            tokens.push_back({EQ, "==", line});
            i += 5;
            continue;
        }
        if (i + 4 <= src.size() && src.substr(i, 4) == "less")
        {
            tokens.push_back({LT, "<", line});
            i += 4;
            continue;
        }

        // Keywords and identifiers
        if (isalpha(src[i]))
        {
            string id;
            while (i < src.size() && (isalnum(src[i]) || src[i] == '_'))
                id += src[i++];

            if (id == "when")
                tokens.push_back({IF, id, line});
            else if (id == "other")
                tokens.push_back({ELSE, id, line});
            else if (id == "repeat")
                tokens.push_back({FT_OR, id, line});
            else if (id == "to")
                tokens.push_back({TO, id, line});
            else if (id == "until")
                tokens.push_back({UNTIL, id, line});
            else if (id == "task")
                tokens.push_back({FUNC, id, line});
            else if (id == "give")
                tokens.push_back({RETURN, id, line});
            else if (id == "out")
                tokens.push_back({PRINT, id, line});
            else if (id == "in")
                tokens.push_back({T_INPUT, id, line});
            else if (id == "yes")
                tokens.push_back({T_BOOL, "1", line});
            else if (id == "no")
                tokens.push_back({T_BOOL, "0", line});
            else if (id == "escape")
                tokens.push_back({BREAK, id, line});
            else if (id == "skip")
                tokens.push_back({CONTINUE, id, line});
            else if (id == "and")
                tokens.push_back({T_AND, id, line});
            else if (id == "or")
                tokens.push_back({T_OR, id, line});
            else if (id == "not")
                tokens.push_back({T_NOT, id, line});
            else if (id == "get")
                tokens.push_back({GET, id, line});
            else if (id == "use")
                tokens.push_back({USE, id, line});
            else
                tokens.push_back({IDENT, id, line});
            continue;
        }

        // Operators - rest remains the same
        if (src[i] == '+')
        {
            if (i + 1 < src.size() && src[i + 1] == '+')
            {
                tokens.push_back({INC, "++", line});
                i += 2;
                continue;
            }
            if (i + 1 < src.size() && src[i + 1] == '=')
            {
                tokens.push_back({PLUSEQ, "+=", line});
                i += 2;
                continue;
            }
            tokens.push_back({PLUS, "+", line});
            i++;
            continue;
        }
        if (src[i] == '-')
        {
            if (i + 1 < src.size() && src[i + 1] == '-')
            {
                tokens.push_back({DEC, "--", line});
                i += 2;
                continue;
            }
            if (i + 1 < src.size() && src[i + 1] == '=')
            {
                tokens.push_back({MINUSEQ, "-=", line});
                i += 2;
                continue;
            }
            tokens.push_back({MINUS, "-", line});
            i++;
            continue;
        }
        if (src[i] == '*')
        {
            if (i + 1 < src.size() && src[i + 1] == '*')
            {
                tokens.push_back({POWER, "**", line});
                i += 2;
                continue;
            }
            if (i + 1 < src.size() && src[i + 1] == '=')
            {
                tokens.push_back({MULEQ, "*=", line});
                i += 2;
                continue;
            }
            tokens.push_back({MUL, "*", line});
            i++;
            continue;
        }
        if (src[i] == '/')
        {
            if (i + 1 < src.size() && src[i + 1] == '=')
            {
                tokens.push_back({T_DIVEQ, "/=", line});
                i += 2;
                continue;
            }
            tokens.push_back({T_DIV, "/", line});
            i++;
            continue;
        }
        if (src[i] == '%')
        {
            if (i + 1 < src.size() && src[i + 1] == '=')
            {
                tokens.push_back({MODEQ, "%=", line});
                i += 2;
                continue;
            }
            tokens.push_back({MOD, "%", line});
            i++;
            continue;
        }
        if (src[i] == '=')
        {
            if (i + 1 < src.size() && src[i + 1] == '=')
            {
                tokens.push_back({EQ, "==", line});
                i += 2;
                continue;
            }
            tokens.push_back({ASSIGN, "=", line});
            i++;
            continue;
        }
        if (src[i] == '!')
        {
            if (i + 1 < src.size() && src[i + 1] == '=')
            {
                tokens.push_back({NEQ, "!=", line});
                i += 2;
                continue;
            }
            error("Unexpected '!' character", line);
        }
        if (src[i] == '<')
        {
            if (i + 1 < src.size() && src[i + 1] == '=')
            {
                tokens.push_back({LTE, "<=", line});
                i += 2;
                continue;
            }
            tokens.push_back({LT, "<", line});
            i++;
            continue;
        }
        if (src[i] == '>')
        {
            if (i + 1 < src.size() && src[i + 1] == '=')
            {
                tokens.push_back({GTE, ">=", line});
                i += 2;
                continue;
            }
            tokens.push_back({GT, ">", line});
            i++;
            continue;
        }
        if (src[i] == '(')
        {
            tokens.push_back({LPAREN, "(", line});
            i++;
            continue;
        }
        if (src[i] == ')')
        {
            tokens.push_back({RPAREN, ")", line});
            i++;
            continue;
        }
        if (src[i] == '{')
        {
            tokens.push_back({LBRACE, "{", line});
            i++;
            continue;
        }
        if (src[i] == '}')
        {
            tokens.push_back({RBRACE, "}", line});
            i++;
            continue;
        }
        if (src[i] == '[')
        {
            tokens.push_back({LBRACKET, "[", line});
            i++;
            continue;
        }
        if (src[i] == ']')
        {
            tokens.push_back({RBRACKET, "]", line});
            i++;
            continue;
        }
        if (src[i] == ',')
        {
            tokens.push_back({COMMA, ",", line});
            i++;
            continue;
        }

        error("Unknown character: " + string(1, src[i]), line);
    }

    tokens.push_back({END, "", line});
    return tokens;
}

struct Value;
using Closure = shared_ptr<unordered_map<string, Value>>;

struct Value
{
    enum Type
    {
        NUM,
        STR,
        T_BOOL,
        ARR,
        FUNC,
        FUNC_REF
    } type;
    double num = 0;
    string str = "";
    bool boolean = false;
    vector<Value> arr;
    
    
    vector<string> funcParams;
    size_t funcBodyStart = 0;
    Closure closure;
    string funcRefName = "";

    Value() : type(NUM), num(0) {}

    static Value Number(double v)
    {
        Value val;
        val.type = NUM;
        val.num = v;
        return val;
    }
    static Value String(const string &s)
    {
        Value val;
        val.type = STR;
        val.str = s;
        return val;
    }
    static Value Bool(bool b)
    {
        Value val;
        val.type = T_BOOL;
        val.boolean = b;
        return val;
    }
    static Value Array(const vector<Value> &a)
    {
        Value val;
        val.type = ARR;
        val.arr = a;
        return val;
    }
    static Value Func(const vector<string> &params, size_t bodyStart, Closure capturedScope)
    {
        Value val;
        val.type = FUNC;
        val.funcParams = params;
        val.funcBodyStart = bodyStart;
        val.closure = capturedScope;
        return val;
    }
    static Value FuncRef(const string& name)
    {
        Value val;
        val.type = FUNC_REF;
        val.funcRefName = name;
        return val;
    }

    bool toBool() const
    {
        if (type == T_BOOL)
            return boolean;
        if (type == NUM)
            return num != 0;
        if (type == STR)
            return !str.empty();
        if (type == ARR)
            return !arr.empty();
        if (type == FUNC)
            return true;
        if (type == FUNC_REF)
            return true;
        return false;
    }

    double toNumber() const
    {
        if (type == NUM)
            return num;
        if (type == T_BOOL)
            return boolean ? 1.0 : 0.0;
        if (type == STR)
        {
            try
            {
                return stod(str);
            }
            catch (...)
            {
                return 0.0;
            }
        }
        return 0.0;
    }

    string toString() const
    {
        if (type == STR)
            return str;
        if (type == NUM)
        {
            if (num == (int)num)
            {
                return to_string((int)num);
            }
            string s = to_string(num);
            s.erase(s.find_last_not_of('0') + 1, string::npos);
            if (s.back() == '.')
                s.pop_back();
            return s;
        }
        if (type == T_BOOL)
            return boolean ? "yes" : "no";
        if (type == ARR)
        {
            string s = "[";
            for (size_t i = 0; i < arr.size(); i++)
            {
                s += arr[i].toString();
                if (i < arr.size() - 1)
                    s += ", ";
            }
            s += "]";
            return s;
        }
        if (type == FUNC)
            return "<function>";
        if (type == FUNC_REF)
            return "<function " + funcRefName + ">";
        return "";
    }

    bool equals(const Value &other) const
    {
        if (type != other.type)
        {
            return toString() == other.toString();
        }
        if (type == NUM)
            return num == other.num;
        if (type == STR)
            return str == other.str;
        if (type == T_BOOL)
            return boolean == other.boolean;
        if (type == FUNC)
            return funcBodyStart == other.funcBodyStart;
        if (type == FUNC_REF)
            return funcRefName == other.funcRefName;
        return false;
    }
};


struct Function
{
    vector<string> params;
    size_t bodyStart;
    vector<Token> tokens;  // Add this line
};


enum ControlFlow
{
    NONE,
    BREAK_FLAG,
    CONTINUE_FLAG,
    RETURN_FLAG
};


class EZ
{
    vector<Token> t;
    size_t p = 0;
    vector<unordered_map<string, Value>> scopes;
    unordered_map<string, Function> functions;
    unordered_map<string, function<Value(vector<Value> &, int)>> builtins;
    ControlFlow flow = NONE;
    Value returnValue;
    int recursionDepth = 0;
    static const int MAX_RECURSION = 1000;
    
    unordered_set<string> loadedLibraries;
    string currentDirectory;
    unordered_map<string, Value> routes;

    Token cur()
    {
        if (p >= t.size())
            return t[t.size() - 1];
        return t[p];
    }

    void next()
    {
        if (p < t.size())
            p++;
    }

    Token peek(int offset = 1)
    {
        if (p + offset < t.size())
            return t[p + offset];
        return t[t.size() - 1];
    }

    void pushScope() { scopes.push_back({}); }
    void popScope()
    {
        if (!scopes.empty())
            scopes.pop_back();
    }

    void setVar(const string &name, const Value &val)
    {
        for (int i = scopes.size() - 1; i >= 0; i--)
        {
            if (scopes[i].count(name))
            {
                scopes[i][name] = val;
                return;
            }
        }
        scopes.back()[name] = val;
    }

    Value getVar(const string &name)
    {
        for (int i = scopes.size() - 1; i >= 0; i--)
        {
            if (scopes[i].count(name))
                return scopes[i][name];
        }
        error("Undefined variable: " + name, cur().line);
        return Value::Number(0);
    }

    bool varExists(const string &name)
    {
        for (int i = scopes.size() - 1; i >= 0; i--)
        {
            if (scopes[i].count(name))
                return true;
        }
        return false;
    }

  
    Closure captureScope()
    {
        auto captured = make_shared<unordered_map<string, Value>>();
        for (const auto &scope : scopes)
        {
            for (const auto &pair : scope)
            {
                (*captured)[pair.first] = pair.second;
            }
        }
        return captured;
    }

    string resolveLibraryPath(const string &libName)
    {
        // Try relative to current directory
        fs::path relPath = fs::path(currentDirectory) / libName;
        if (fs::exists(relPath))
            return relPath.string();
        
        // Try with .ez extension if not provided
        if (relPath.extension() != ".ez")
        {
            relPath += ".ez";
            if (fs::exists(relPath))
                return relPath.string();
        }
        
        // Try absolute path
        if (fs::exists(libName))
            return libName;
        
        // Try with .ez extension
        string withExt = libName;
        if (fs::path(libName).extension() != ".ez")
            withExt += ".ez";
        if (fs::exists(withExt))
            return withExt;
        
        return "";
    }

    void loadLibrary(const string &libName)
    {
        // Resolve the full path
        string libPath = resolveLibraryPath(libName);
        
        if (libPath.empty())
        {
            error("Library not found: " + libName, cur().line);
        }
        
        // Get canonical path to avoid loading same library twice
        string canonicalPath = fs::canonical(libPath).string();
        
        // Check if already loaded
        if (loadedLibraries.count(canonicalPath))
        {
            return; // Already loaded, skip
        }
        
        // Mark as loaded
        loadedLibraries.insert(canonicalPath);
        
        // Save current state
        vector<Token> savedTokens = t;
        size_t savedPos = p;
        string savedDir = currentDirectory;
        
        // Update current directory for nested imports
        currentDirectory = fs::path(libPath).parent_path().string();
        
        try
        {
            // Read and tokenize library
            string libCode = readFile(libPath);
            vector<Token> libTokens = tokenize(libCode);
            
            // Execute library in current scope
            t = libTokens;
            p = 0;
            
            while (cur().type != END)
            {
                statement();
            }
            
            // Restore state
            t = savedTokens;
            p = savedPos;
            currentDirectory = savedDir;
        }
        catch (...)
        {
            // Restore state on error
            t = savedTokens;
            p = savedPos;
            currentDirectory = savedDir;
            throw;
        }
    }
    string generate_session_id() {
    static int counter = 0;
    counter++;

    auto now = chrono::steady_clock::now().time_since_epoch().count();
    string time_part = to_string(now);

    // Proper string concatenation using +
    return "sess_" + to_string(counter) + "_" + time_part;
}
    void initBuiltins()
    {
        
        builtins["len"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("len expects 1 argument", line);
            if (args[0].type == Value::ARR)
            {
                return Value::Number(args[0].arr.size());
            }
            else if (args[0].type == Value::STR)
            {
                return Value::Number(args[0].str.length());
            }
            error("len expects array or string", line);
            return Value::Number(0);
        };

        
        builtins["add"] = [this](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("add expects 2 arguments (array, value)", line);
            if (args[0].type != Value::ARR)
                error("First argument to add must be array", line);
            args[0].arr.push_back(args[1]);
            return args[0];
        };

       
        builtins["remove"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("remove expects 1 argument", line);
            if (args[0].type != Value::ARR)
                error("remove expects array", line);
            if (args[0].arr.empty())
                error("Cannot remove from empty array", line);
            Value result = args[0];
            result.arr.pop_back();
            return result;
        };

        
        builtins["clock"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 0)
                error("clock expects no arguments", line);
            auto now = chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();
            return Value::Number(static_cast<double>(millis));
        };

        
        builtins["stop"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("stop expects 1 argument (milliseconds)", line);
            int ms = static_cast<int>(args[0].toNumber());
            if (ms < 0)
                error("stop duration must be non-negative", line);
            this_thread::sleep_for(chrono::milliseconds(ms));
            return Value::Number(0);
        };
        // int() - convert to integer
        builtins["int"] = [](vector<Value> &args, int line) -> Value
        {
        if (args.size() != 1)
        error("int expects 1 argument", line);
        return Value::Number((int)args[0].toNumber());
        };

        // sqrt() - square root
        builtins["sqrt"] = [](vector<Value> &args, int line) -> Value
        {
        if (args.size() != 1)
        error("sqrt expects 1 argument", line);
        double val = args[0].toNumber();
        if (val < 0)
        error("Cannot take square root of negative number", line);
        return Value::Number(sqrt(val));
        };

        // pow() - power function (alternative to ** operator)
        builtins["pow"] = [](vector<Value> &args, int line) -> Value
        {
        if (args.size() != 2)
        error("pow expects 2 arguments (base, exponent)", line);
        return Value::Number(pow(args[0].toNumber(), args[1].toNumber()));
        };
                // readFile() - read entire file as string
        builtins["readFile"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("readFile expects 1 argument (filename)", line);
            
            string filename = args[0].toString();
            ifstream file(filename);
            
            if (!file)
                error("Cannot open file: " + filename, line);
            
            stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            
            return Value::String(buffer.str());
        };

        // writeFile() - write string to file (overwrites)
        builtins["writeFile"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("writeFile expects 2 arguments (filename, content)", line);
            
            string filename = args[0].toString();
            string content = args[1].toString();
            
            ofstream file(filename);
            
            if (!file)
                error("Cannot write to file: " + filename, line);
            
            file << content;
            file.close();
            
            return Value::Number(1);
        };

        // appendFile() - append string to file
        builtins["appendFile"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("appendFile expects 2 arguments (filename, content)", line);
            
            string filename = args[0].toString();
            string content = args[1].toString();
            
            ofstream file(filename, ios::app);
            
            if (!file)
                error("Cannot append to file: " + filename, line);
            
            file << content;
            file.close();
            
            return Value::Number(1);
        };

        // fileExists() - check if file exists
        builtins["fileExists"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("fileExists expects 1 argument (filename)", line);
            
            string filename = args[0].toString();
            ifstream file(filename);
            bool exists = file.good();
            file.close();
            
            return Value::Bool(exists);
        };

        // deleteFile() - delete a file
        builtins["deleteFile"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("deleteFile expects 1 argument (filename)", line);
            
            string filename = args[0].toString();
            
            if (remove(filename.c_str()) == 0)
                return Value::Bool(true);
            else
                return Value::Bool(false);
        };

        // readLines() - read file as array of lines
        builtins["readLines"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("readLines expects 1 argument (filename)", line);
            
            string filename = args[0].toString();
            ifstream file(filename);
            
            if (!file)
                error("Cannot open file: " + filename, line);
            
            vector<Value> lines;
            string fileLine;
            
            while (getline(file, fileLine))
            {
                lines.push_back(Value::String(fileLine));
            }
            
            file.close();
            return Value::Array(lines);
        };

        // writeLines() - write array of strings to file (one per line)
        builtins["writeLines"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("writeLines expects 2 arguments (filename, array)", line);
            
            string filename = args[0].toString();
            
            if (args[1].type != Value::ARR)
                error("Second argument to writeLines must be an array", line);
            
            ofstream file(filename);
            
            if (!file)
                error("Cannot write to file: " + filename, line);
            
            for (size_t i = 0; i < args[1].arr.size(); i++)
            {
                file << args[1].arr[i].toString();
                if (i < args[1].arr.size() - 1)
                    file << "\n";
            }
            
            file.close();
            return Value::Number(1);
        };
        builtins["register_route"] = [this](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("register_route expects 2 arguments (path, handler)", line);
            string path = args[0].toString();
            Value handler = args[1];
            if (handler.type != Value::FUNC && handler.type != Value::FUNC_REF)
                error("Second argument must be a function", line);
            routes[path] = handler;
            return Value::Number(1);
        };

        builtins["run_server"] = [this](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("run_server expects 1 argument (port)", line);
            int port = static_cast<int>(args[0].toNumber());

            #ifdef _WIN32
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
                error("Failed to initialize Winsock", line);

            SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listenSocket == INVALID_SOCKET)
            {
                WSACleanup();
                error("Failed to create socket", line);
            }

            sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(listenSocket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
            {
                closesocket(listenSocket);
                WSACleanup();
                error("Failed to bind socket", line);
            }

            if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
            {
                closesocket(listenSocket);
                WSACleanup();
                error("Failed to listen on socket", line);
            }

            cout << "Server listening on port " << port << endl;

            while (true)
            {
                SOCKET clientSocket = accept(listenSocket, NULL, NULL);
                if (clientSocket == INVALID_SOCKET)
                    continue;

                char buf[4096] = {0};
                int bytesReceived = recv(clientSocket, buf, sizeof(buf) - 1, 0);
                if (bytesReceived > 0)
                {
                    string request(buf, bytesReceived);

                    // Parse request
                    size_t firstEol = request.find("\r\n");
                    if (firstEol == string::npos) {
                        closesocket(clientSocket);
                        continue;
                    }
                    string firstLine = request.substr(0, firstEol);
                    stringstream flss(firstLine);
                    string method, path, version;
                    flss >> method >> path >> version;

                    // Extract query
                    size_t queryPos = path.find('?');
                    string query = "";
                    if (queryPos != string::npos) {
                        query = path.substr(queryPos + 1);
                        path = path.substr(0, queryPos);
                    }

                    // Find end of headers
                    size_t headerEnd = request.find("\r\n\r\n");
                    string body = "";
                    if (headerEnd != string::npos) {
                        body = request.substr(headerEnd + 4);
                    }

                    // Find handler
                    auto it = routes.find(path);
                    if (it != routes.end())
                    {
                        Value handler = it->second;
                        vector<Value> callArgs = {
                            Value::String(method),
                            Value::String(path),
                            Value::String(query),
                            Value::String(body)
                        };
                        Value response = callFunctionValue(handler, callArgs, line);
                        string respBody = response.toString();

                        string httpResp = "HTTP/1.1 200 OK\r\n"
                                          "Content-Type: text/html\r\n"
                                          "Content-Length: " + to_string(respBody.size()) + "\r\n"
                                          "\r\n" + respBody;
                        send(clientSocket, httpResp.c_str(), httpResp.size(), 0);
                    }
                    else
                    {
                        string notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
                        send(clientSocket, notFound.c_str(), notFound.size(), 0);
                    }
                }
                closesocket(clientSocket);
            }

            closesocket(listenSocket);
            WSACleanup();
            #else
            error("Server functionality is only supported on Windows", line);
            #endif

            return Value::Number(0);
        };
                // Add this to the initBuiltins() function in your EZ interpreter

// Inside the initBuiltins() method, add:

// split() - split string by delimiter
        builtins["split"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("split expects 2 arguments (string, delimiter)", line);
            
            string str = args[0].toString();
            string delimiter = args[1].toString();
            
            vector<Value> result;
            
            if (delimiter.empty())
            {
                // Split into individual characters
                for (char c : str)
                {
                    result.push_back(Value::String(string(1, c)));
                }
                return Value::Array(result);
            }
            
            size_t start = 0;
            size_t end = str.find(delimiter);
            
            while (end != string::npos)
            {
                result.push_back(Value::String(str.substr(start, end - start)));
                start = end + delimiter.length();
                end = str.find(delimiter, start);
            }
            
            // Add the last part
            result.push_back(Value::String(str.substr(start)));
            
            return Value::Array(result);
        };

        // charAt() - get character at index
        builtins["charAt"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("charAt expects 2 arguments (string, index)", line);
            
            string str = args[0].toString();
            int index = static_cast<int>(args[1].toNumber());
            
            if (index < 0 || index >= static_cast<int>(str.length()))
                error("String index out of bounds: " + to_string(index), line);
            
            return Value::String(string(1, str[index]));
        };

        // indexOf() - find substring position
        builtins["indexOf"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("indexOf expects 2 arguments (string, substring)", line);
            
            string str = args[0].toString();
            string substr = args[1].toString();
            
            size_t pos = str.find(substr);
            
            if (pos == string::npos)
                return Value::Number(-1);
            
            return Value::Number(static_cast<double>(pos));
        };

        // substring() - extract substring
        builtins["substring"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() < 2 || args.size() > 3)
                error("substring expects 2 or 3 arguments (string, start, [end])", line);
            
            string str = args[0].toString();
            int start = static_cast<int>(args[1].toNumber());
            
            if (start < 0 || start > static_cast<int>(str.length()))
                error("Invalid start index", line);
            
            if (args.size() == 3)
            {
                int end = static_cast<int>(args[2].toNumber());
                if (end < start || end > static_cast<int>(str.length()))
                    error("Invalid end index", line);
                return Value::String(str.substr(start, end - start));
            }
            
            return Value::String(str.substr(start));
        };

        // trim() - remove leading/trailing whitespace
        builtins["trim"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("trim expects 1 argument", line);
            
            string str = args[0].toString();
            
            // Trim leading whitespace
            size_t start = str.find_first_not_of(" \t\n\r");
            if (start == string::npos)
                return Value::String("");
            
            // Trim trailing whitespace
            size_t end = str.find_last_not_of(" \t\n\r");
            
            return Value::String(str.substr(start, end - start + 1));
        };

        // replace() - replace all occurrences
        builtins["replace"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 3)
                error("replace expects 3 arguments (string, find, replace)", line);
            
            string str = args[0].toString();
            string find = args[1].toString();
            string replace = args[2].toString();
            
            if (find.empty())
                return Value::String(str);
            
            string result = str;
            size_t pos = 0;
            
            while ((pos = result.find(find, pos)) != string::npos)
            {
                result.replace(pos, find.length(), replace);
                pos += replace.length();
            }
            
            return Value::String(result);
        };

// toLowerCase() - convert to lowercase
         builtins["toLowerCase"] = [](vector<Value> &args, int line) -> Value
            {
                if (args.size() != 1)
                    error("toLowerCase expects 1 argument", line);
                
                string str = args[0].toString();
                transform(str.begin(), str.end(), str.begin(), ::tolower);
                return Value::String(str);
            };

        // toUpperCase() - convert to uppercase
        builtins["toUpperCase"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("toUpperCase expects 1 argument", line);
            
            string str = args[0].toString();
            transform(str.begin(), str.end(), str.begin(), ::toupper);
            return Value::String(str);
        };

// startsWith() - check if string starts with prefix
builtins["startsWith"] = [](vector<Value> &args, int line) -> Value
{
    if (args.size() != 2)
        error("startsWith expects 2 arguments (string, prefix)", line);
    
    string str = args[0].toString();
    string prefix = args[1].toString();
    
    if (prefix.length() > str.length())
        return Value::Bool(false);
    
    return Value::Bool(str.substr(0, prefix.length()) == prefix);
};

// endsWith() - check if string ends with suffix
builtins["endsWith"] = [](vector<Value> &args, int line) -> Value
{
    if (args.size() != 2)
        error("endsWith expects 2 arguments (string, suffix)", line);
    
    string str = args[0].toString();
    string suffix = args[1].toString();
    
    if (suffix.length() > str.length())
        return Value::Bool(false);
    
    return Value::Bool(str.substr(str.length() - suffix.length()) == suffix);
};

        // join() - join array elements with delimiter
        builtins["join"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("join expects 2 arguments (array, delimiter)", line);
            
            if (args[0].type != Value::ARR)
                error("First argument to join must be array", line);
            
            string delimiter = args[1].toString();
            string result = "";
            
            for (size_t i = 0; i < args[0].arr.size(); i++)
            {
                result += args[0].arr[i].toString();
                if (i < args[0].arr.size() - 1)
                    result += delimiter;
            }
            
            return Value::String(result);
        };
                builtins["join"] = [](vector<Value>& args, int line) -> Value {
            if (args.size() != 2 || args[0].type != Value::STR || args[1].type != Value::ARR)
                error("join expects delimiter and array", line);
            string delim = args[0].str;
            vector<Value>& arr = args[1].arr;
            string result;
            for (size_t i = 0; i < arr.size(); ++i) {
                result += arr[i].toString();
                if (i < arr.size() - 1) result += delim;
            }
            return Value::String(result);
        };

        builtins["reverse"] = [](vector<Value>& args, int line) -> Value {
            if (args.size() != 1 || args[0].type != Value::ARR) error("reverse expects array", line);
            vector<Value> rev = args[0].arr;
            std::reverse(rev.begin(), rev.end());
            return Value::Array(rev);
        };

        builtins["sort"] = [](vector<Value>& args, int line) -> Value {
            if (args.size() != 1 || args[0].type != Value::ARR) error("sort expects array", line);
            vector<Value> sorted = args[0].arr;
            std::sort(sorted.begin(), sorted.end(), [](const Value& a, const Value& b) {
                return a.toNumber() < b.toNumber();
            });
            return Value::Array(sorted);
        };
        // Get header value from request (e.g., Content-Type, Authorization)
        builtins["request_header"] = [this](vector<Value>& args, int line) -> Value {
            if (args.size() != 1 || args[0].type != Value::STR) error("request_header expects header name", line);
            // Note: You need to store headers in the server loop first
            // For now, return empty — we'll add header parsing next
            return Value::String("");
        };

        // Set response header (e.g., Content-Type, Location for redirect)
        

        // Redirect (302)
        // === ADD THESE INCLUDES AT THE TOP IF NOT ALREADY PRESENT ===


// === ADD THESE BUILT-INS TO initBuiltins() ===

        builtins["parse_form"] = [](vector<Value>& args, int line) -> Value {
            if (args.size() != 1 || args[0].type != Value::STR)
                error("parse_form expects 1 string (request body)", line);

            string body = args[0].str;
            vector<Value> result;

            if (body.empty())
                return Value::Array(result);

            stringstream ss(body);
            string pair;
            while (getline(ss, pair, '&')) {
                size_t eq_pos = pair.find('=');
                string key, val;

                if (eq_pos != string::npos) {
                    key = pair.substr(0, eq_pos);
                    val = pair.substr(eq_pos + 1);
                } else {
                    key = pair;
                    val = "";
                }

                // URL decode
                string decoded_key, decoded_val;
                for (size_t i = 0; i < key.length(); ++i) {
                    if (key[i] == '+') decoded_key += ' ';
                    else if (key[i] == '%' && i + 2 < key.length()) {
                        int hex = stoi(key.substr(i + 1, 2), nullptr, 16);
                        decoded_key += static_cast<char>(hex);
                        i += 2;
                    } else {
                        decoded_key += key[i];
                    }
                }
                for (size_t i = 0; i < val.length(); ++i) {
                    if (val[i] == '+') decoded_val += ' ';
                    else if (val[i] == '%' && i + 2 < val.length()) {
                        int hex = stoi(val.substr(i + 1, 2), nullptr, 16);
                        decoded_val += static_cast<char>(hex);
                        i += 2;
                    } else {
                        decoded_val += val[i];
                    }
                }

                vector<Value> kv = { Value::String(decoded_key), Value::String(decoded_val) };
                result.push_back(Value::Array(kv));
            }

            return Value::Array(result);
        };

        builtins["mime_type"] = [](vector<Value>& args, int line) -> Value {
            if (args.size() != 1 || args[0].type != Value::STR)
                error("mime_type expects filename string", line);

            string filename = args[0].str;
            string ext = fs::path(filename).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".html" || ext == ".htm") return Value::String("text/html");
            if (ext == ".css") return Value::String("text/css");
            if (ext == ".js") return Value::String("text/javascript");
            if (ext == ".json") return Value::String("application/json");
            if (ext == ".png") return Value::String("image/png");
            if (ext == ".jpg" || ext == ".jpeg") return Value::String("image/jpeg");
            if (ext == ".gif") return Value::String("image/gif");
            if (ext == ".svg") return Value::String("image/svg+xml");
            if (ext == ".ico") return Value::String("image/x-icon");
            if (ext == ".txt") return Value::String("text/plain");
            if (ext == ".pdf") return Value::String("application/pdf");

            return Value::String("application/octet-stream");
        };

        builtins["redirect"] = [](vector<Value>& args, int line) -> Value {
            if (args.size() != 1 || args[0].type != Value::STR)
                error("redirect expects URL string", line);

            string url = args[0].str;
            string response = "HTTP/1.1 302 Found\r\n"
                            "Location: " + url + "\r\n"
                            "Content-Length: 0\r\n"
                            "\r\n";
            return Value::String(response);
        };
          
        builtins["set_cookie"] = [](vector<Value>& args, int line) -> Value {
        if (args.size() < 2) error("set_cookie needs name and value", line);
        string name = args[0].toString();
        string value = args[1].toString();
        string cookie = "Set-Cookie: " + name + "=" + value + "; Path=/; HttpOnly";
        // You can collect these and add to response headers
        return Value::String(cookie);
    };

        builtins["get_cookie"] = [](vector<Value>& args, int line) -> Value {
            // Would parse from request headers — placeholder
            return Value::String("");
    }; 
            
             
        builtins["env"] = [](vector<Value>& args, int line) -> Value {
        if (args.size() != 1 || args[0].type != Value::STR) error("env expects key", line);
        const char* val = getenv(args[0].str.c_str());
        return Value::String(val ? val : "");
    }; 
    
    // === ADD THESE BUILT-INS TO initBuiltins() ===

// 1. Simple templating: replace {{key}} with value
        builtins["template_replace"] = [](vector<Value>& args, int line) -> Value {
            if (args.size() != 3 || args[0].type != Value::STR || args[1].type != Value::STR || args[2].type != Value::STR)
                error("template_replace expects template_string, key, value", line);

            string tmpl = args[0].str;
            string placeholder = "{{" + args[1].str + "}}";
            string replacement = args[2].str;

            size_t pos = 0;
            while ((pos = tmpl.find(placeholder, pos)) != string::npos) {
                tmpl.replace(pos, placeholder.length(), replacement);
                pos += replacement.length();
            }

            return Value::String(tmpl);
        };

        // 2. Set a response header (collect them for later use)
        builtins["set_header"] = [this](vector<Value>& args, int line) -> Value {
            if (args.size() != 2 || args[0].type != Value::STR || args[1].type != Value::STR)
                error("set_header expects name and value strings", line);

            string name = args[0].str;
            string value = args[1].str;

            // We'll store headers in a thread-local or per-request way later
            // For now, just return success — you can extend the server to collect them
            // Suggestion: add a map<string,string> response_headers in the server loop
            cout << "[HEADER] " << name << ": " << value << endl;  // Debug output

            return Value::Number(1);
        };

        // 3. Return JSON with proper Content-Type header
        builtins["json_response"] = [this](vector<Value>& args, int line) -> Value {
            if (args.size() != 1 || args[0].type != Value::ARR)
                error("json_response expects array of [key, value] pairs", line);

            vector<Value>& pairs = args[0].arr;
            string json_str = "{";

            bool first = true;
            for (const Value& pair : pairs) {
                if (pair.type != Value::ARR || pair.arr.size() != 2) continue;

                string key = pair.arr[0].toString();
                string val = pair.arr[1].toString();

                if (!first) json_str += ",";
                first = false;

                json_str += "\"" + key + "\":";
                if (pair.arr[1].type == Value::STR) {
                    // Escape quotes
                    string escaped = val;
                    size_t pos = 0;
                    while ((pos = escaped.find('"', pos)) != string::npos) {
                        escaped.replace(pos, 1, "\\\"");
                        pos += 2;
                    }
                    json_str += "\"" + escaped + "\"";
                } else {
                    json_str += val;
                }
            }
            json_str += "}";

            // Simulate setting header (in real use, collect this)
            cout << "[HEADER] Content-Type: application/json" << endl;

            return Value::String(json_str);
        };
        // === ADD THESE TO initBuiltins() ===

            // 1. Include another template file (for header/footer reuse)
            builtins["include_template"] = [](vector<Value>& args, int line) -> Value {
                if (args.size() != 1 || args[0].type != Value::STR)
                    error("include_template expects filename string", line);

                string filename = args[0].str;

                ifstream file(filename);
                if (!file) {
                    return Value::String("<!-- include_template: file not found: " + filename + " -->");
                }

                stringstream buffer;
                buffer << file.rdbuf();
                return Value::String(buffer.str());
            };

            // 2. Basic session support using a static map (in-memory only)
            // Note: Not persistent across restarts, but perfect for demos/login

            // We'll use a global map for sessions: session_id -> map<string, Value>
            static unordered_map<string, unordered_map<string, Value>> sessions;

            // Generate simple session ID (you can improve with random)
            

            builtins["session_start"] = [this](vector<Value>& args, int line) -> Value {
                if (args.size() != 0) error("session_start takes no arguments", line);
                string sess_id = generate_session_id();
                sessions[sess_id] = {};
                // In real use, you'd set a cookie: Set-Cookie: session=sess_id
                cout << "[SESSION] Started: " <<sess_id << endl;
                return Value::String(sess_id);
            };

            builtins["session_set"] = [](vector<Value>& args, int line) -> Value {
                if (args.size() != 3 || args[0].type != Value::STR)
                    error("session_set expects session_id, key, value", line);

                string sess_id = args[0].str;
                string key = args[1].toString();

                if (sessions.find(sess_id) == sessions.end()) {
                    return Value::Bool(false); // invalid session
                }

                sessions[sess_id][key] = args[2];
                return Value::Bool(true);
            };

            builtins["session_get"] = [](vector<Value>& args, int line) -> Value {
                if (args.size() != 2 || args[0].type != Value::STR)
                    error("session_get expects session_id, key", line);

                string sess_id = args[0].str;
                string key = args[1].toString();

                auto sess_it = sessions.find(sess_id);
                if (sess_it == sessions.end()) {
                    return Value::String(""); // no session
                }

                auto& data = sess_it->second;
                auto it = data.find(key);
                if (it == data.end()) {
                    return Value::String("");
                }

                return it->second;
            };

            // 3. Abort with HTTP status code (404, 500, etc.)
            builtins["abort"] = [](vector<Value>& args, int line) -> Value {
                int status = 404;
                string message = "Not Found";

                if (args.size() >= 1) {
                    status = static_cast<int>(args[0].toNumber());
                }
                if (args.size() >= 2) {
                    message = args[1].toString();
                }

                string status_line;
                switch (status) {
                    case 400: status_line = "400 Bad Request"; break;
                    case 401: status_line = "401 Unauthorized"; break;
                    case 403: status_line = "403 Forbidden"; break;
                    case 404: status_line = "404 Not Found"; break;
                    case 500: status_line = "500 Internal Server Error"; break;
                    default: status_line = to_string(status) + " Error";
                }

                string body = "<h1>" + to_string(status) + " " + message + "</h1>";
                string response = "HTTP/1.1 " + to_string(status) + " " + status_line + "\r\n"
                                "Content-Type: text/html\r\n"
                                "Content-Length: " + to_string(body.length()) + "\r\n"
                                "\r\n" + body;

                return Value::String(response);
            };

        // ==================== NEW IMPROVEMENTS ====================
        
        // abs() - absolute value
        builtins["abs"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("abs expects 1 argument", line);
            return Value::Number(fabs(args[0].toNumber()));
        };

        // floor() - round down to integer
        builtins["floor"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("floor expects 1 argument", line);
            return Value::Number(floor(args[0].toNumber()));
        };

        // ceil() - round up to integer
        builtins["ceil"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("ceil expects 1 argument", line);
            return Value::Number(ceil(args[0].toNumber()));
        };

        // round() - round to nearest integer
        builtins["round"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("round expects 1 argument", line);
            return Value::Number(round(args[0].toNumber()));
        };

        // min() - minimum of two numbers
        builtins["min"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() < 2)
                error("min expects at least 2 arguments", line);
            double minVal = args[0].toNumber();
            for (size_t i = 1; i < args.size(); i++)
            {
                double val = args[i].toNumber();
                if (val < minVal) minVal = val;
            }
            return Value::Number(minVal);
        };

        // max() - maximum of two or more numbers
        builtins["max"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() < 2)
                error("max expects at least 2 arguments", line);
            double maxVal = args[0].toNumber();
            for (size_t i = 1; i < args.size(); i++)
            {
                double val = args[i].toNumber();
                if (val > maxVal) maxVal = val;
            }
            return Value::Number(maxVal);
        };

        // random() - random number between 0 and 1
        builtins["random"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 0)
                error("random expects no arguments", line);
            return Value::Number((double)rand() / RAND_MAX);
        };

        // randomRange() - random integer in range [min, max]
        builtins["randomRange"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("randomRange expects 2 arguments (min, max)", line);
            int minVal = (int)args[0].toNumber();
            int maxVal = (int)args[1].toNumber();
            if (minVal > maxVal)
                error("randomRange: min must be <= max", line);
            return Value::Number(minVal + rand() % (maxVal - minVal + 1));
        };

        // typeof() - get type of value as string
        builtins["typeof"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("typeof expects 1 argument", line);
            switch (args[0].type)
            {
                case Value::NUM: return Value::String("number");
                case Value::STR: return Value::String("string");
                case Value::T_BOOL: return Value::String("boolean");
                case Value::ARR: return Value::String("array");
                case Value::FUNC: return Value::String("function");
                case Value::FUNC_REF: return Value::String("function");
                default: return Value::String("unknown");
            }
        };

        // isNumber() - check if value is a number
        builtins["isNumber"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("isNumber expects 1 argument", line);
            return Value::Bool(args[0].type == Value::NUM);
        };

        // isString() - check if value is a string
        builtins["isString"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("isString expects 1 argument", line);
            return Value::Bool(args[0].type == Value::STR);
        };

        // isArray() - check if value is an array
        builtins["isArray"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("isArray expects 1 argument", line);
            return Value::Bool(args[0].type == Value::ARR);
        };

        // isBool() - check if value is a boolean
        builtins["isBool"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("isBool expects 1 argument", line);
            return Value::Bool(args[0].type == Value::T_BOOL);
        };

        // toNumber() - convert to number
        builtins["toNumber"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("toNumber expects 1 argument", line);
            return Value::Number(args[0].toNumber());
        };

        // toStr() - convert to string (explicit)
        builtins["toStr"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 1)
                error("toStr expects 1 argument", line);
            return Value::String(args[0].toString());
        };

        // contains() - check if array contains value or string contains substring
        builtins["contains"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("contains expects 2 arguments", line);
            
            if (args[0].type == Value::ARR)
            {
                for (const Value &v : args[0].arr)
                {
                    if (v.equals(args[1]))
                        return Value::Bool(true);
                }
                return Value::Bool(false);
            }
            else if (args[0].type == Value::STR)
            {
                return Value::Bool(args[0].str.find(args[1].toString()) != string::npos);
            }
            error("contains expects array or string as first argument", line);
            return Value::Bool(false);
        };

        // slice() - get subset of array or string
        builtins["slice"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() < 2 || args.size() > 3)
                error("slice expects 2 or 3 arguments (value, start, [end])", line);
            
            int start = (int)args[1].toNumber();
            
            if (args[0].type == Value::ARR)
            {
                const vector<Value> &arr = args[0].arr;
                int end = args.size() == 3 ? (int)args[2].toNumber() : (int)arr.size();
                
                // Handle negative indices
                if (start < 0) start = max(0, (int)arr.size() + start);
                if (end < 0) end = max(0, (int)arr.size() + end);
                if (start > (int)arr.size()) start = (int)arr.size();
                if (end > (int)arr.size()) end = (int)arr.size();
                
                if (start >= end)
                    return Value::Array({});
                
                vector<Value> result(arr.begin() + start, arr.begin() + end);
                return Value::Array(result);
            }
            else if (args[0].type == Value::STR)
            {
                const string &str = args[0].str;
                int end = args.size() == 3 ? (int)args[2].toNumber() : (int)str.length();
                
                // Handle negative indices
                if (start < 0) start = max(0, (int)str.length() + start);
                if (end < 0) end = max(0, (int)str.length() + end);
                if (start > (int)str.length()) start = (int)str.length();
                if (end > (int)str.length()) end = (int)str.length();
                
                if (start >= end)
                    return Value::String("");
                
                return Value::String(str.substr(start, end - start));
            }
            
            error("slice expects array or string as first argument", line);
            return Value::Array({});
        };

        // range() - create array of numbers from start to end
        builtins["range"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() < 1 || args.size() > 3)
                error("range expects 1 to 3 arguments (end) or (start, end) or (start, end, step)", line);
            
            int start = 0, end, step = 1;
            
            if (args.size() == 1)
            {
                end = (int)args[0].toNumber();
            }
            else if (args.size() == 2)
            {
                start = (int)args[0].toNumber();
                end = (int)args[1].toNumber();
            }
            else
            {
                start = (int)args[0].toNumber();
                end = (int)args[1].toNumber();
                step = (int)args[2].toNumber();
            }
            
            if (step == 0)
                error("range step cannot be zero", line);
            
            vector<Value> result;
            if (step > 0)
            {
                for (int i = start; i < end; i += step)
                    result.push_back(Value::Number(i));
            }
            else
            {
                for (int i = start; i > end; i += step)
                    result.push_back(Value::Number(i));
            }
            
            return Value::Array(result);
        };

        // insert() - insert value at index in array
        builtins["insert"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 3)
                error("insert expects 3 arguments (array, index, value)", line);
            if (args[0].type != Value::ARR)
                error("First argument to insert must be array", line);
            
            int index = (int)args[1].toNumber();
            vector<Value> result = args[0].arr;
            
            if (index < 0) index = 0;
            if (index > (int)result.size()) index = (int)result.size();
            
            result.insert(result.begin() + index, args[2]);
            return Value::Array(result);
        };

        // removeAt() - remove value at index from array
        builtins["removeAt"] = [](vector<Value> &args, int line) -> Value
        {
            if (args.size() != 2)
                error("removeAt expects 2 arguments (array, index)", line);
            if (args[0].type != Value::ARR)
                error("First argument to removeAt must be array", line);
            
            int index = (int)args[1].toNumber();
            vector<Value> result = args[0].arr;
            
            if (index < 0 || index >= (int)result.size())
                error("removeAt index out of bounds", line);
            
            result.erase(result.begin() + index);
            return Value::Array(result);
        };
    }

    Value factor()
{
if (cur().type == LPAREN)
{
next();
Value v = logicalOr();
if (cur().type != RPAREN)
error("Expected ')'", cur().line);
next();
return v;
}
if (cur().type == T_NOT)
{
    next();
    Value v = factor();
    return Value::Bool(!v.toBool());
}

if (cur().type == NUMBER)
{
    double v = stod(cur().text);
    next();
    return Value::Number(v);
}

if (cur().type == STRING)
{
    string v = cur().text;
    next();
    return Value::String(v);
}

if (cur().type == T_BOOL)
{
    bool v = (cur().text == "1");
    next();
    return Value::Bool(v);
}

if (cur().type == LBRACKET)
{
    next();
    vector<Value> arr;
    while (cur().type != RBRACKET && cur().type != END)
    {
        arr.push_back(logicalOr());
        if (cur().type == COMMA)
        {
            next();
        }
        else if (cur().type != RBRACKET)
        {
            error("Expected ',' or ']' in array literal", cur().line);
        }
    }
    if (cur().type != RBRACKET)
        error("Expected ']'", cur().line);
    next();
    return Value::Array(arr);
}

if (cur().type == FUNC)
{
    next();
    
    if (cur().type != LPAREN)
        error("Expected '(' after 'task' in lambda", cur().line);
    next();
    
    vector<string> params;
    while (cur().type != RPAREN && cur().type != END)
    {
        if (cur().type != IDENT)
            error("Expected parameter name", cur().line);
        params.push_back(cur().text);
        next();
        if (cur().type == COMMA)
            next();
    }
    if (cur().type != RPAREN)
        error("Expected ')' after parameters", cur().line);
    next();
    
    Closure captured = captureScope();
    size_t bodyStart = p;
    skipBlock();
    
    return Value::Func(params, bodyStart, captured);
}

if (cur().type == IDENT)
{
    string n = cur().text;
    int callLine = cur().line;
    next();

    if (cur().type == LPAREN)
    {
        next();
        vector<Value> args;
        while (cur().type != RPAREN && cur().type != END)
        {
            args.push_back(logicalOr());
            if (cur().type == COMMA)
                next();
        }
        if (cur().type != RPAREN)
            error("Expected ')'", cur().line);
        next();
        
        Value result;
        
        // Check if it's a variable containing a function
        if (varExists(n))
        {
            Value v = getVar(n);
            if (v.type == Value::FUNC || v.type == Value::FUNC_REF)
            {
                result = callFunctionValue(v, args, callLine);
                
                // Handle chained function calls
                while (cur().type == LPAREN)
                {
                    next();
                    vector<Value> chainArgs;
                    while (cur().type != RPAREN && cur().type != END)
                    {
                        chainArgs.push_back(logicalOr());
                        if (cur().type == COMMA)
                            next();
                    }
                    if (cur().type != RPAREN)
                        error("Expected ')'", cur().line);
                    next();
                    result = callFunctionValue(result, chainArgs, callLine);
                }
                return result; // RETURN HERE instead of continuing
            }
        }
        
        // Check if it's a defined function
        if (functions.count(n))
        {
            result = callFunction(n, args);
            return result; // RETURN HERE - this was missing!
        }
        
        // Check if it's a builtin
        if (builtins.count(n))
        {
            result = builtins[n](args, callLine);
            return result; // RETURN HERE - this was missing!
        }
        
        error("Undefined function: " + n, callLine);
    }

    if (cur().type == LBRACKET)
        {
            next();
            Value idx = logicalOr();
            if (cur().type != RBRACKET)
                error("Expected ']'", cur().line);
            next();

            Value variable = getVar(n);
            
            // Handle array indexing (existing)
            if (variable.type == Value::ARR)
            {
                int i = (int)idx.toNumber();
                if (i < 0 || i >= (int)variable.arr.size())
                    error("Array index out of bounds: " + to_string(i), cur().line);
                return variable.arr[i];
            }
            // NEW: Handle string indexing
            else if (variable.type == Value::STR)
            {
                int i = (int)idx.toNumber();
                if (i < 0 || i >= (int)variable.str.length())
                    error("String index out of bounds: " + to_string(i), cur().line);
                // Return single character as a string
                return Value::String(string(1, variable.str[i]));
            }
            else
            {
                error("Cannot index type, only arrays and strings can be indexed", callLine);
            }
        }

        if (functions.count(n))
        {
            return Value::FuncRef(n);
        }

        return getVar(n);
    }


error("Unexpected token: " + cur().text, cur().line);
return Value::Number(0);
}

    Value term()
{
    Value v = factor();
    while (cur().type == MUL || cur().type == T_DIV || cur().type == INTT_DIV || cur().type == MOD || cur().type == POWER)
    {
        Token_Type op = cur().type;
        int line = cur().line;
        next();
        Value r = factor();

        if (op == MUL)
        {
            v.num = v.toNumber() * r.toNumber();
            v.type = Value::NUM;
        }
        else if (op == T_DIV)
        {
            double divisor = r.toNumber();
            if (divisor == 0)
                error("Division by zero", line);
            v.num = v.toNumber() / divisor;
            v.type = Value::NUM;
        }
        else if (op == INTT_DIV)
        {
            int divisor = (int)r.toNumber();
            if (divisor == 0)
                error("Integer division by zero", line);
            v.num = (int)v.toNumber() / divisor;
            v.type = Value::NUM;
        }
        else if (op == MOD)
        {
            long long dividend = (long long)v.toNumber();
            long long divisor  = (long long)r.toNumber();

            if (divisor == 0)
                error("Modulo by zero", line);

            long long result = ((dividend % divisor) + divisor) % divisor;

            v.num = (double)result;
            v.type = Value::NUM;
        }
        else if (op == POWER)
        {
            v.num = pow(v.toNumber(), r.toNumber());
            v.type = Value::NUM;
        }
    }
    return v;
}

    Value expr()
    {
        Value v = term();
        while (cur().type == PLUS || cur().type == MINUS)
        {
            Token_Type op = cur().type;
            next();
            Value r = term();

            if (op == PLUS)
            {
                if (v.type == Value::STR || r.type == Value::STR)
                {
                    v = Value::String(v.toString() + r.toString());
                }
                else
                {
                    v.num = v.toNumber() + r.toNumber();
                    v.type = Value::NUM;
                }
            }
            else
            {
                v.num = v.toNumber() - r.toNumber();
                v.type = Value::NUM;
            }
        }
        return v;
    }

    Value comparison()
    {
        Value v = expr();

        while (cur().type == EQ || cur().type == NEQ ||
               cur().type == LT || cur().type == GT ||
               cur().type == LTE || cur().type == GTE)
        {
            Token_Type op = cur().type;
            next();
            Value r = expr();

            bool result;
            if (op == EQ)
            {
                result = v.equals(r);
            }
            else if (op == NEQ)
            {
                result = !v.equals(r);
            }
            else
            {
                double lhs = v.toNumber();
                double rhs = r.toNumber();
                if (op == LT)
                    result = (lhs < rhs);
                else if (op == GT)
                    result = (lhs > rhs);
                else if (op == LTE)
                    result = (lhs <= rhs);
                else
                    result = (lhs >= rhs);
            }

            v = Value::Bool(result);
        }
        return v;
    }

    Value logicalAnd()
    {
        Value v = comparison();
        while (cur().type == T_AND)
        {
            next();
            if (!v.toBool())
            {
                comparison();
                return Value::Bool(false);
            }
            Value r = comparison();
            v = Value::Bool(r.toBool());
        }
        return v;
    }

    Value logicalOr()
    {
        Value v = logicalAnd();
        while (cur().type == T_OR)
        {
            next();
            if (v.toBool())
            {
                logicalAnd();
                return Value::Bool(true);
            }
            Value r = logicalAnd();
            v = Value::Bool(r.toBool());
        }
        return v;
    }

    void skipBlock()
    {
        if (cur().type == LBRACE)
            next();

        int depth = 1;
        while (depth > 0 && cur().type != END)
        {
            if (cur().type == LBRACE)
                depth++;
            if (cur().type == RBRACE)
                depth--;
            next();
        }
    }

    Value callFunction(const string &name, vector<Value> args)
{
    // Check builtins first
    if (builtins.count(name))
    {
        return builtins[name](args, cur().line);
    }

    if (!functions.count(name))
        error("Undefined function: " + name, cur().line);

    if (recursionDepth >= MAX_RECURSION)
        error("Maximum recursion depth exceeded", cur().line);

    Function &func = functions[name];
    if (args.size() != func.params.size())
        error("Function " + name + " expects " + to_string(func.params.size()) +
                  " arguments, got " + to_string(args.size()),
              cur().line);

    size_t savedPos = p;
    vector<Token> savedTokens = t;  // Add this line
    ControlFlow savedFlow = flow;
    recursionDepth++;

    t = func.tokens;  // Add this line - switch to function's token stream
    pushScope();
    for (size_t i = 0; i < args.size(); i++)
    {
        setVar(func.params[i], args[i]);
    }

    p = func.bodyStart;
    flow = NONE;
    returnValue = Value::Number(0);

    executeBlock();

    Value result = returnValue;

    popScope();
    recursionDepth--;
    t = savedTokens;  // Add this line - restore token stream
    p = savedPos;
    flow = savedFlow;

    return result;
}

    
    Value callFunctionValue(const Value &funcVal, vector<Value> args, int callLine)
    {
        if (funcVal.type == Value::FUNC_REF)
        {
            string name = funcVal.funcRefName;
            if (!functions.count(name))
                error("Undefined function: " + name, callLine);

            const Function& f = functions[name];

            if (args.size() != f.params.size())
                error("Wrong number of arguments for " + name, callLine);

            // Save state
            size_t savedPos = p;
            vector<Token> savedTokens = t;
            ControlFlow savedFlow = flow;
            recursionDepth++;

            // Switch to function's token stream
            t = f.tokens;
            p = f.bodyStart;

            pushScope();
            for (size_t i = 0; i < args.size(); ++i)
            {
                setVar(f.params[i], args[i]);
            }

            flow = NONE;
            returnValue = Value::Number(0);

            executeBlock();

            Value result = returnValue;

            popScope();
            recursionDepth--;
            t = savedTokens;
            p = savedPos;
            flow = savedFlow;

            return result;
        }

        if (funcVal.type != Value::FUNC)
            error("Not callable", callLine);

        if (args.size() != funcVal.funcParams.size())
            error("Function expects " + to_string(funcVal.funcParams.size()) +
                      " arguments, got " + to_string(args.size()),
                  callLine);

        if (recursionDepth >= MAX_RECURSION)
            error("Maximum recursion depth exceeded", callLine);

        size_t savedPos = p;
        ControlFlow savedFlow = flow;
        recursionDepth++;

        
        pushScope();
        
       
        if (funcVal.closure)
        {
            for (const auto &pair : *funcVal.closure)
            {
                scopes.back()[pair.first] = pair.second;
            }
        }
        
        
        for (size_t i = 0; i < args.size(); i++)
        {
            scopes.back()[funcVal.funcParams[i]] = args[i];
        }

        p = funcVal.funcBodyStart;
        flow = NONE;
        returnValue = Value::Number(0);

        executeBlock();

        Value result = returnValue;

        popScope();
        recursionDepth--;
        p = savedPos;
        flow = savedFlow;

        return result;
    }

    void executeBlock()
    {
        if (cur().type != LBRACE)
            error("Expected '{'", cur().line);
        next();

        while (cur().type != RBRACE && cur().type != END && flow == NONE)
        {
            statement();
        }

        if (cur().type == RBRACE)
            next();
    }

    void statement()
    {
        if (cur().type == USE)
        {
            next();
            if (cur().type != STRING && cur().type != IDENT)
                error("Expected library name after 'use'", cur().line);
            
            string libName = cur().text;
            if (cur().type == IDENT)
            {
                libName += ".ez";
            }
            int line = cur().line;
            next();
            
            loadLibrary(libName);
            return;
        }

        if (cur().type == PRINT)
        {
            next();

            bool first = true;
            do
            {
                Value v = logicalOr();

                if (!first)
                {
                    cout << " ";
                }
                cout << v.toString();
                first = false;

                
                if (cur().type == COMMA)
                {
                    next();
                }
                else
                {
                    break;
                }
            } while (true);

            cout << endl;
            return;
        }

        if (cur().type == T_INPUT)
        {
            next();
            if (cur().type != IDENT)
                error("Expected variable name after 'in'", cur().line);
            string varName = cur().text;
            next();

            string input;
            if (!getline(cin, input))
            {
                error("Failed to read input", cur().line);
            }

            try
            {
                size_t pos;
                double num = stod(input, &pos);
                if (pos == input.length())
                {
                    setVar(varName, Value::Number(num));
                }
                else
                {
                    setVar(varName, Value::String(input));
                }
            }
            catch (...)
            {
                setVar(varName, Value::String(input));
            }
            return;
        }

        if (cur().type == IF)
        {
            next();
            Value cond = logicalOr();

            if (cond.toBool())
            {
                executeBlock();
               
                while (cur().type == IF || cur().type == ELSE)
                {
                    if (cur().type == IF)
                    {
                        next();
                        logicalOr();
                    }
                    else
                    {
                        next();
                    }
                    skipBlock();
                }
            }
            else
            {
                skipBlock();
               
                while (cur().type == IF)
                {
                    next();
                    Value elseCond = logicalOr();
                    if (elseCond.toBool())
                    {
                        executeBlock();
                        
                        while (cur().type == IF || cur().type == ELSE)
                        {
                            if (cur().type == IF)
                            {
                                next();
                                logicalOr();
                            }
                            else
                            {
                                next();
                            }
                            skipBlock();
                        }
                        return;
                    }
                    else
                    {
                        skipBlock();
                    }
                }
                
                if (cur().type == ELSE)
                {
                    next();
                    executeBlock();
                }
            }
            return;
        }

        
        if (cur().type == GET)
        {
            next();
            if (cur().type != IDENT)
                error("Expected variable name after 'get'", cur().line);
            string varName = cur().text;
            next();

            if (cur().type != T_INPUT)
                error("Expected 'in' after variable name in foreach loop", cur().line);
            next();

            if (cur().type != IDENT)
                error("Expected array name after 'in'", cur().line);
            string arrayName = cur().text;
            next();

            Value arr = getVar(arrayName);
            if (arr.type != Value::ARR)
                error("Expected array in foreach loop: " + arrayName, cur().line);

            size_t loopStart = p;

            for (size_t i = 0; i < arr.arr.size(); i++)
            {
                setVar(varName, arr.arr[i]);
                p = loopStart;
                executeBlock();

                if (flow == BREAK_FLAG)
                {
                    flow = NONE;
                    break;
                }
                if (flow == CONTINUE_FLAG)
                {
                    flow = NONE;
                    continue;
                }
                if (flow == RETURN_FLAG)
                    break;
            }
            return;
        }

        if (cur().type == FT_OR)
        {
            next();
            if (cur().type != IDENT)
                error("Expected variable name after 'repeat'", cur().line);
            string varName = cur().text;
            next();

            if (cur().type != ASSIGN)
                error("Expected '=' in for loop", cur().line);
            next();
            Value start = logicalOr();

            if (cur().type != TO)
                error("Expected 'to' in for loop", cur().line);
            next();
            Value end = logicalOr();

            size_t loopStart = p;
            int startVal = (int)start.toNumber();
            int endVal = (int)end.toNumber();

            
            if (startVal <= endVal)
            {
                for (int i = startVal; i <= endVal; i++)
                {
                    setVar(varName, Value::Number(i));
                    p = loopStart;
                    executeBlock();

                    if (flow == BREAK_FLAG)
                    {
                        flow = NONE;
                        break;
                    }
                    if (flow == CONTINUE_FLAG)
                    {
                        flow = NONE;
                        continue;
                    }
                    if (flow == RETURN_FLAG)
                        break;
                }
            }
            else
            {
                for (int i = startVal; i >= endVal; i--)
                {
                    setVar(varName, Value::Number(i));
                    p = loopStart;
                    executeBlock();

                    if (flow == BREAK_FLAG)
                    {
                        flow = NONE;
                        break;
                    }
                    if (flow == CONTINUE_FLAG)
                    {
                        flow = NONE;
                        continue;
                    }
                    if (flow == RETURN_FLAG)
                        break;
                }
            }
            return;
        }
        if (cur().type == UNTIL)
        {
            next();
            size_t condStart = p;
            Value cond = logicalOr();
            size_t loopStart = p;

            
            while (cond.toBool())
            {
                p = loopStart;
                executeBlock();

                if (flow == BREAK_FLAG)
                {
                    flow = NONE;
                    break;
                }
                if (flow == CONTINUE_FLAG)
                {
                    flow = NONE;
                }
                if (flow == RETURN_FLAG)
                    break;

                
                p = condStart;
                cond = logicalOr();
                loopStart = p;
            }

            
            if (cur().type == LBRACE)
            {
                skipBlock();
            }
            return;
        }
        if (cur().type == BREAK)
        {
            flow = BREAK_FLAG;
            next();
            return;
        }

        if (cur().type == CONTINUE)
        {
            flow = CONTINUE_FLAG;
            next();
            return;
        }

        if (cur().type == RETURN)
        {
            next();
            if (cur().type != RBRACE && cur().type != END)
            {
                returnValue = logicalOr();
            }
            flow = RETURN_FLAG;
            return;
        }

        if (cur().type == FUNC)
        {
            next();
            if (cur().type != IDENT)
                error("Expected function name", cur().line);
            string fname = cur().text;
            next();

            if (cur().type != LPAREN)
                error("Expected '(' after function name", cur().line);
            next();

            vector<string> params;
            while (cur().type != RPAREN && cur().type != END)
            {
                if (cur().type != IDENT)
                    error("Expected parameter name", cur().line);
                params.push_back(cur().text);
                next();
                if (cur().type == COMMA)
                    next();
            }
            if (cur().type != RPAREN)
                error("Expected ')' after parameters", cur().line);
            next();

            functions[fname] = {params, p, t};  // Store current token stream
            skipBlock();
            return;
        }

        
        if (cur().type == IDENT)
    {
        string varName = cur().text;
        int line = cur().line;
        next();

        // ===== MODIFIED: Handle string[index] = value =====
        if (cur().type == LBRACKET)
        {
            next();
            Value idx = logicalOr();
            if (cur().type != RBRACKET)
                error("Expected ']'", cur().line);
            next();

            if (cur().type == ASSIGN)
            {
                next();
                Value newVal = logicalOr();
                Value variable = getVar(varName);
                
                // Handle array assignment (existing)
                if (variable.type == Value::ARR)
                {
                    int i = (int)idx.toNumber();
                    if (i < 0 || i >= (int)variable.arr.size())
                        error("Array index out of bounds: " + to_string(i), line);
                    variable.arr[i] = newVal;
                    setVar(varName, variable);
                    return;
                }
                // NEW: Handle string assignment
                else if (variable.type == Value::STR)
                {
                    int i = (int)idx.toNumber();
                    if (i < 0 || i >= (int)variable.str.length())
                        error("String index out of bounds: " + to_string(i), line);
                    
                    // Convert newVal to string and get first character
                    string newChar = newVal.toString();
                    if (newChar.empty())
                        error("Cannot assign empty string to character position", line);
                    
                    // Modify the string at position i
                    variable.str[i] = newChar[0];
                    setVar(varName, variable);
                    return;
                }
                else
                {
                    error("Not an array or string: " + varName, line);
                }
            }
            error("Expected '=' after array/string index", cur().line);
        }

            
            if (cur().type == ASSIGN)
            {
                next();
                Value val = logicalOr();
                setVar(varName, val);
                return;
            }

            
            if (cur().type == PLUSEQ || cur().type == MINUSEQ ||
                cur().type == MULEQ || cur().type == T_DIVEQ || cur().type == MODEQ)
            {
                Token_Type op = cur().type;
                next();
                Value right = logicalOr();
                Value left = getVar(varName);

                if (op == PLUSEQ)
                {
                    if (left.type == Value::STR || right.type == Value::STR)
                    {
                        setVar(varName, Value::String(left.toString() + right.toString()));
                    }
                    else
                    {
                        setVar(varName, Value::Number(left.toNumber() + right.toNumber()));
                    }
                }
                else if (op == MINUSEQ)
                {
                    setVar(varName, Value::Number(left.toNumber() - right.toNumber()));
                }
                else if (op == MULEQ)
                {
                    setVar(varName, Value::Number(left.toNumber() * right.toNumber()));
                }
                else if (op == T_DIVEQ)
                {
                    double divisor = right.toNumber();
                    if (divisor == 0)
                        error("Division by zero", line);
                    setVar(varName, Value::Number(left.toNumber() / divisor));
                }
                else if (op == MODEQ)
                {
                    int divisor = (int)right.toNumber();
                    if (divisor == 0)
                        error("Modulo by zero", line);
                    setVar(varName, Value::Number((int)left.toNumber() % divisor));
                }
                return;
            }

            
            if (cur().type == INC)
            {
                next();
                Value v = getVar(varName);
                setVar(varName, Value::Number(v.toNumber() + 1));
                return;
            }

            if (cur().type == DEC)
            {
                next();
                Value v = getVar(varName);
                setVar(varName, Value::Number(v.toNumber() - 1));
                return;
            }

            
            p--;
            logicalOr();
            return;
        }

       
        logicalOr();
    }

public:
    void run(const vector<Token> &tokens, const string &directory = ".")
    {
        t = tokens;
        p = 0;
        currentDirectory = directory;
        scopes.push_back({});
        initBuiltins();

        while (cur().type != END)
        {
            statement();
        }
    }
};


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cout << "Usage: " << argv[0] << " <filename.ez>" << endl;
        return 1;
    }

    try
    {
        string filepath = argv[1];
        string code = readFile(filepath);
        
        string directory = ".";
        fs::path p(filepath);
        if (p.has_parent_path())
        {
            directory = p.parent_path().string();
        }
        
        vector<Token> tokens = tokenize(code);
        EZ interpreter;
        interpreter.run(tokens, directory);
    }
    catch (const InterpreterException &e)
    {
        cerr << e.what() << endl;
        return 1;
    }
    catch (const exception &e)
    {
        cerr << "Unexpected error: " << e.what() << endl;
        return 1;
    }

    return 0;
}