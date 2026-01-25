#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include <unordered_map>
#include "Token.h"

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();
    bool hasError() const { return hadError; }

private:
    std::string source;
    std::vector<Token> tokens;
    size_t start = 0;
    size_t current = 0;
    int line = 1;
    int column = 1;
    bool hadError = false;

    static std::unordered_map<std::string, TokenType> keywords;

    bool isAtEnd() const;
    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);
    
    void scanToken();
    void addToken(TokenType type);
    void addToken(TokenType type, double value);
    void addToken(TokenType type, const std::string& value);
    void addToken(TokenType type, bool value);
    
    void scanString();
    void scanNumber();
    void scanIdentifier();
    void skipLineComment();
    void skipBlockComment();
    
    bool isDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;
    
    void error(const std::string& message);
};

#endif // LEXER_H
