#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <variant>

enum class TokenType {
    // Literals
    NUMBER,
    STRING,
    IDENTIFIER,
    TRUE,
    FALSE,
    NIL,

    // Keywords
    OUT,
    IN,
    WHEN,
    OTHER,
    REPEAT,
    TO,
    WHILE,
    USE,
    TASK,
    GIVE,
    ESCAPE,
    SKIP,
    GET,
    AND,
    OR,
    NOT,
    
    // OOP Keywords
    MODEL,
    INIT,
    SELF,
    HIDDEN,
    SHOWN,
    EXTENDS,
    STRUCT,
    TRY,
    CATCH,
    THROW,

    // Operators
    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,
    EQUAL,
    EQUAL_EQUAL,
    BANG,
    BANG_EQUAL,
    LESS,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL,
    PLUS_EQUAL,
    MINUS_EQUAL,
    STAR_EQUAL,
    SLASH_EQUAL,

    // Punctuation
    LPAREN,
    RPAREN,
    LBRACKET,
    RBRACKET,
    LBRACE,
    RBRACE,
    COMMA,
    DOT,
    COLON,
    PIPE,
    ARROW,
    NEWLINE,

    // Special
    END_OF_FILE,
    ERROR
};

struct Token {
    TokenType type;
    std::string lexeme;
    std::variant<std::nullptr_t, double, std::string, bool> literal;
    int line;
    int column;

    Token(TokenType type, const std::string& lexeme, int line, int column)
        : type(type), lexeme(lexeme), literal(nullptr), line(line), column(column) {}

    Token(TokenType type, const std::string& lexeme, double value, int line, int column)
        : type(type), lexeme(lexeme), literal(value), line(line), column(column) {}

    Token(TokenType type, const std::string& lexeme, const std::string& value, int line, int column)
        : type(type), lexeme(lexeme), literal(value), line(line), column(column) {}

    Token(TokenType type, const std::string& lexeme, bool value, int line, int column)
        : type(type), lexeme(lexeme), literal(value), line(line), column(column) {}
};

inline std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING: return "STRING";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::NIL: return "NIL";
        case TokenType::OUT: return "OUT";
        case TokenType::IN: return "IN";
        case TokenType::WHEN: return "WHEN";
        case TokenType::OTHER: return "OTHER";
        case TokenType::REPEAT: return "REPEAT";
        case TokenType::TO: return "TO";
        case TokenType::WHILE: return "WHILE";
        case TokenType::TASK: return "TASK";
        case TokenType::GIVE: return "GIVE";
        case TokenType::ESCAPE: return "ESCAPE";
        case TokenType::SKIP: return "SKIP";
        case TokenType::GET: return "GET";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NOT: return "NOT";
        case TokenType::MODEL: return "MODEL";
        case TokenType::INIT: return "INIT";
        case TokenType::SELF: return "SELF";
        case TokenType::HIDDEN: return "HIDDEN";
        case TokenType::SHOWN: return "SHOWN";
        case TokenType::EXTENDS: return "EXTENDS";
        case TokenType::TRY: return "TRY";
        case TokenType::CATCH: return "CATCH";
        case TokenType::THROW: return "THROW";
        case TokenType::STRUCT: return "STRUCT";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::BANG: return "BANG";
        case TokenType::BANG_EQUAL: return "BANG_EQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::PLUS_EQUAL: return "PLUS_EQUAL";
        case TokenType::MINUS_EQUAL: return "MINUS_EQUAL";
        case TokenType::STAR_EQUAL: return "STAR_EQUAL";
        case TokenType::SLASH_EQUAL: return "SLASH_EQUAL";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::COMMA: return "COMMA";
        case TokenType::DOT: return "DOT";
        case TokenType::COLON: return "COLON";
        case TokenType::PIPE: return "PIPE";
        case TokenType::ARROW: return "ARROW";
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

#endif // TOKEN_H
