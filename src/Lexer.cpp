#include "Lexer.h"
#include <iostream>
#include <stdexcept>

std::unordered_map<std::string, TokenType> Lexer::keywords = {
    {"out", TokenType::OUT},
    {"in", TokenType::IN},
    {"when", TokenType::WHEN},
    {"other", TokenType::OTHER},
    {"repeat", TokenType::REPEAT},
    {"to", TokenType::TO},
    {"while", TokenType::WHILE},
    {"use", TokenType::USE},
    {"task", TokenType::TASK},
    {"give", TokenType::GIVE},
    {"escape", TokenType::ESCAPE},
    {"skip", TokenType::SKIP},
    {"get", TokenType::GET},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"not", TokenType::NOT},
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
    {"nil", TokenType::NIL},
    // OOP keywords
    {"model", TokenType::MODEL},
    {"init", TokenType::INIT},
    {"self", TokenType::SELF},
    {"hidden", TokenType::HIDDEN},
    {"shown", TokenType::SHOWN},
    {"extends", TokenType::EXTENDS},
    {"struct", TokenType::STRUCT},
    {"try", TokenType::TRY},
    {"catch", TokenType::CATCH},
    {"throw", TokenType::THROW},
    {"error", TokenType::THROW}
};

Lexer::Lexer(const std::string& source) : source(source) {}

std::vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        start = current;
        scanToken();
    }
    
    tokens.push_back(Token(TokenType::END_OF_FILE, "", line, column));
    return tokens;
}

bool Lexer::isAtEnd() const {
    return current >= source.length();
}

char Lexer::advance() {
    char c = source[current++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.length()) return '\0';
    return source[current + 1];
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source[current] != expected) return false;
    advance();
    return true;
}

void Lexer::scanToken() {
    char c = advance();
    
    switch (c) {
        case '(': addToken(TokenType::LPAREN); break;
        case ')': addToken(TokenType::RPAREN); break;
        case '[': addToken(TokenType::LBRACKET); break;
        case ']': addToken(TokenType::RBRACKET); break;
        case '{': addToken(TokenType::LBRACE); break;
        case '}': addToken(TokenType::RBRACE); break;
        case ',': addToken(TokenType::COMMA); break;
        case '.': addToken(TokenType::DOT); break;
        case ':': addToken(TokenType::COLON); break;
        case '+':
            if (match('=')) {
                addToken(TokenType::PLUS_EQUAL);
            } else {
                addToken(TokenType::PLUS);
            }
            break;
        case '-':
            if (match('=')) {
                addToken(TokenType::MINUS_EQUAL);
            } else {
                addToken(TokenType::MINUS);
            }
            break;
        case '*':
            if (match('=')) {
                addToken(TokenType::STAR_EQUAL);
            } else {
                addToken(TokenType::STAR);
            }
            break;
        case '/':
            if (match('/')) {
                skipLineComment();
            } else if (match('*')) {
                skipBlockComment();
            } else if (match('=')) {
                addToken(TokenType::SLASH_EQUAL);
            } else {
                addToken(TokenType::SLASH);
            }
            break;
        case '%': addToken(TokenType::PERCENT); break;
        case '=':
            if (match('=')) {
                addToken(TokenType::EQUAL_EQUAL);
            } else if (match('>')) {
                addToken(TokenType::ARROW);
            } else {
                addToken(TokenType::EQUAL);
            }
            break;
        case '!':
            if (match('=')) {
                addToken(TokenType::BANG_EQUAL);
            } else {
                addToken(TokenType::BANG);
            }
            break;
        case '<':
            if (match('=')) {
                addToken(TokenType::LESS_EQUAL);
            } else {
                addToken(TokenType::LESS);
            }
            break;
        case '>':
            if (match('=')) {
                addToken(TokenType::GREATER_EQUAL);
            } else {
                addToken(TokenType::GREATER);
            }
            break;
        case '|':
            addToken(TokenType::PIPE);
            break;
        case '#':
            skipLineComment();
            break;
        case '\n':
            addToken(TokenType::NEWLINE);
            break;
        case ' ':
        case '\r':
        case '\t':
            // Ignore whitespace (except newlines)
            break;
        case '"':
        case '\'':
            scanString();
            break;
        default:
            if (isDigit(c)) {
                scanNumber();
            } else if (isAlpha(c)) {
                scanIdentifier();
            } else {
                error("Unexpected character: " + std::string(1, c));
            }
            break;
    }
}

void Lexer::addToken(TokenType type) {
    std::string text = source.substr(start, current - start);
    tokens.push_back(Token(type, text, line, column - (int)(current - start)));
}

void Lexer::addToken(TokenType type, double value) {
    std::string text = source.substr(start, current - start);
    tokens.push_back(Token(type, text, value, line, column - (int)(current - start)));
}

void Lexer::addToken(TokenType type, const std::string& value) {
    std::string text = source.substr(start, current - start);
    tokens.push_back(Token(type, text, value, line, column - (int)(current - start)));
}

void Lexer::addToken(TokenType type, bool value) {
    std::string text = source.substr(start, current - start);
    tokens.push_back(Token(type, text, value, line, column - (int)(current - start)));
}

void Lexer::scanString() {
    char quote = source[start];
    std::string value;
    
    while (!isAtEnd() && peek() != quote) {
        if (peek() == '\n') {
            error("Unterminated string");
            return;
        }
        if (peek() == '\\' && !isAtEnd()) {
            advance();
            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                default: value += escaped; break;
            }
        } else {
            value += advance();
        }
    }
    
    if (isAtEnd()) {
        error("Unterminated string");
        return;
    }
    
    advance(); // Closing quote
    addToken(TokenType::STRING, value);
}

void Lexer::scanNumber() {
    while (isDigit(peek())) advance();
    
    // Look for decimal part
    if (peek() == '.' && isDigit(peekNext())) {
        advance(); // Consume '.'
        while (isDigit(peek())) advance();
    }
    
    std::string text = source.substr(start, current - start);
    addToken(TokenType::NUMBER, std::stod(text));
}

void Lexer::scanIdentifier() {
    while (isAlphaNumeric(peek())) advance();
    
    std::string text = source.substr(start, current - start);
    
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        TokenType type = it->second;
        if (type == TokenType::TRUE) {
            addToken(type, true);
        } else if (type == TokenType::FALSE) {
            addToken(type, false);
        } else {
            addToken(type);
        }
    } else {
        addToken(TokenType::IDENTIFIER);
    }
}

void Lexer::skipLineComment() {
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

void Lexer::skipBlockComment() {
    int nesting = 1;
    while (!isAtEnd() && nesting > 0) {
        if (peek() == '/' && peekNext() == '*') {
            advance();
            advance();
            nesting++;
        } else if (peek() == '*' && peekNext() == '/') {
            advance();
            advance();
            nesting--;
        } else {
            advance();
        }
    }
    
    if (nesting > 0) {
        error("Unterminated block comment");
    }
}

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlpha(char c) const {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

void Lexer::error(const std::string& message) {
    hadError = true;
    std::cerr << "[Line " << line << ", Col " << column << "] Error: " << message << std::endl;
}
