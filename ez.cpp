#include <iostream>
#include <vector>
#include <string>
#include <cctype>
#include <unordered_map>
#include <fstream>
#include <sstream>

// ===================== TOKENS =====================
enum TokenType {
    NUMBER,
    IDENT,
    PLUS,
    MINUS,
    EQUAL,
    PRINT,
    END
};

struct Token {
    TokenType type;
    std::string text;
};

// ===================== FILE READER =====================
std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        exit(1);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// ===================== LEXER =====================
std::vector<Token> tokenize(const std::string& src) {
    std::vector<Token> tokens;

    for (size_t i = 0; i < src.size();) {
        if (isspace(src[i])) {
            i++;
        }
        else if (isdigit(src[i])) {
            std::string num;
            while (i < src.size() && isdigit(src[i])) {
                num += src[i++];
            }
            tokens.push_back({NUMBER, num});
        }
        else if (isalpha(src[i])) {
            std::string id;
            while (i < src.size() && isalpha(src[i])) {
                id += src[i++];
            }
            if (id == "print")
                tokens.push_back({PRINT, id});
            else
                tokens.push_back({IDENT, id});
        }
        else if (src[i] == '+') {
            tokens.push_back({PLUS, "+"});
            i++;
        }
        else if (src[i] == '-') {
            tokens.push_back({MINUS, "-"});
            i++;
        }
        else if (src[i] == '=') {
            tokens.push_back({EQUAL, "="});
            i++;
        }
        else {
            std::cerr << "Error: Unknown character '" << src[i] << "'\n";
            exit(1);
        }
    }

    tokens.push_back({END, ""});
    return tokens;
}

// ===================== INTERPRETER =====================
class Interpreter {
    std::vector<Token> tokens;
    size_t pos = 0;
    std::unordered_map<std::string, int> variables;

public:
    Interpreter(const std::vector<Token>& t) : tokens(t) {}

    Token current() {
        return tokens[pos];
    }

    void advance() {
        pos++;
    }

    int parseTerm() {
        if (current().type == NUMBER) {
            int value = std::stoi(current().text);
            advance();
            return value;
        }
        else if (current().type == IDENT) {
            std::string name = current().text;
            advance();
            if (variables.count(name) == 0) {
                std::cerr << "Error: Undefined variable " << name << std::endl;
                exit(1);
            }
            return variables[name];
        }
        else {
            std::cerr << "Error: Invalid expression\n";
            exit(1);
        }
    }

    int parseExpression() {
        int value = parseTerm();

        while (current().type == PLUS || current().type == MINUS) {
            if (current().type == PLUS) {
                advance();
                value += parseTerm();
            }
            else if (current().type == MINUS) {
                advance();
                value -= parseTerm();
            }
        }
        return value;
    }

    void parseStatement() {
        if (current().type == IDENT) {
            std::string name = current().text;
            advance(); // IDENT

            if (current().type != EQUAL) {
                std::cerr << "Error: Expected '='\n";
                exit(1);
            }
            advance(); // '='

            int value = parseExpression();
            variables[name] = value;
        }
        else if (current().type == PRINT) {
            advance(); // print
            int value = parseExpression();
            std::cout << value << std::endl;
        }
        else {
            std::cerr << "Error: Invalid statement\n";
            exit(1);
        }
    }

    void run() {
        while (current().type != END) {
            parseStatement();
        }
    }
};

// ===================== MAIN =====================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "EZ Language Interpreter\n";
        std::cout << "Usage: ez.exe <file.ez>\n";
        return 1;
    }

    std::string filename = argv[1];

    // Check extension
    if (filename.substr(filename.find_last_of('.') + 1) != "ez") {
        std::cerr << "Error: Only .ez files are supported\n";
        return 1;
    }

    std::string code = readFile(filename);
    auto tokens = tokenize(code);

    Interpreter interpreter(tokens);
    interpreter.run();

    return 0;
}
