#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <variant>

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#ifdef IN
#undef IN
#endif
#ifdef OUT
#undef OUT
#endif
#ifdef ERROR
#undef ERROR
#endif
#ifdef INTERFACE
#undef INTERFACE
#endif

// Prevent collision with Windows API's TokenType in TOKEN_INFORMATION_CLASS
#define TokenType TokenKind

enum class TokenKind {
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
    STEP,
    WHILE,
    USE,
    TASK,
    GIVE,
    ESCAPE,
    SKIP,
    GET,
    MATCH,
    AND,
    OR,
    NOT,
    ASYNC,
    AWAIT,
    DECORATOR_KW,
    
    // OOP Keywords
    MODEL,
    INIT,
    SELF,
    HIDDEN,
    SHOWN,
    EXTENDS,
    STRUCT,
    NEW,
    SUPER,
    STATIC,
    INTERFACE,
    IMPLEMENTS,
    TRY,
    CATCH,
    THROW,
    FINALLY,
    EXPORT,
    REQUIRES,
    ENSURES,

    // Decorator tokens
    AT,

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

    // Bitwise
    AMPERSAND,
    CARET,
    TILDE,
    LSHIFT,
    RSHIFT,

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
    QUESTION_MARK,
    QUESTION_QUESTION,
    QUESTION_DOT,
    PIPE,
    ARROW,
    NEWLINE,
    ELLIPSIS,

    // Special
    END_OF_FILE,
    ERROR
};

struct Token {
    TokenType type;
    std::string lexeme;
    std::variant<std::nullptr_t, double, long long, std::string, bool> literal;
    std::string filename;
    int line;
    int column;

    Token(TokenType type, const std::string& lexeme, int line, int column, const std::string& file = "")
        : type(type), lexeme(lexeme), literal(nullptr), filename(file), line(line), column(column) {}

    Token(TokenType type, const std::string& lexeme, double value, int line, int column, const std::string& file = "")
        : type(type), lexeme(lexeme), literal(value), filename(file), line(line), column(column) {}

    Token(TokenType type, const std::string& lexeme, long long value, int line, int column, const std::string& file = "")
        : type(type), lexeme(lexeme), literal(value), filename(file), line(line), column(column) {}

    Token(TokenType type, const std::string& lexeme, const std::string& value, int line, int column, const std::string& file = "")
        : type(type), lexeme(lexeme), literal(value), filename(file), line(line), column(column) {}

    Token(TokenType type, const std::string& lexeme, bool value, int line, int column, const std::string& file = "")
        : type(type), lexeme(lexeme), literal(value), filename(file), line(line), column(column) {}
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
        case TokenType::STEP: return "STEP";
        case TokenType::WHILE: return "WHILE";
        case TokenType::USE: return "USE";
        case TokenType::TASK: return "TASK";
        case TokenType::GIVE: return "GIVE";
        case TokenType::ESCAPE: return "ESCAPE";
        case TokenType::SKIP: return "SKIP";
        case TokenType::GET: return "GET";
        case TokenType::MATCH: return "MATCH";
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
        case TokenType::EXPORT: return "EXPORT";
        case TokenType::REQUIRES: return "REQUIRES";
        case TokenType::ENSURES: return "ENSURES";
        case TokenType::DECORATOR_KW: return "DECORATOR_KW";
        case TokenType::AT: return "AT";
        case TokenType::STRUCT: return "STRUCT";
        case TokenType::NEW: return "NEW";
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
        case TokenType::AMPERSAND: return "AMPERSAND";
        case TokenType::CARET: return "CARET";
        case TokenType::TILDE: return "TILDE";
        case TokenType::LSHIFT: return "LSHIFT";
        case TokenType::RSHIFT: return "RSHIFT";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::COMMA: return "COMMA";
        case TokenType::DOT: return "DOT";
        case TokenType::COLON: return "COLON";
        case TokenType::QUESTION_MARK: return "QUESTION_MARK";
        case TokenType::QUESTION_QUESTION: return "QUESTION_QUESTION";
        case TokenType::QUESTION_DOT: return "QUESTION_DOT";
        case TokenType::PIPE: return "PIPE";
        case TokenType::ARROW: return "ARROW";
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::ELLIPSIS: return "ELLIPSIS";
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

#endif // TOKEN_H
