#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <string>
#include <stdexcept>
#include "Token.h"
#include "AST.h"

class ParseError : public std::runtime_error {
public:
    int line;
    ParseError(const std::string& message, int line) 
        : std::runtime_error(message), line(line) {}
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    std::vector<StmtPtr> parse();
    bool hasError() const { return hadError; }

private:
    std::vector<Token> tokens;
    size_t current = 0;
    bool hadError = false;

    // Token navigation
    bool isAtEnd() const;
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool match(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& message);
    
    // Skip newlines
    void skipNewlines();
    void consumeNewlines();
    
    // Error handling
    void error(const Token& token, const std::string& message);
    void synchronize();

    // Statement parsing
    StmtPtr declaration();
    StmtPtr statement();
    StmtPtr outStatement();
    StmtPtr whenStatement();
    StmtPtr whileStatement();
    StmtPtr repeatStatement();
    StmtPtr getStatement();
    StmtPtr taskStatement();
    StmtPtr giveStatement();
    StmtPtr escapeStatement();
    StmtPtr skipStatement();
    StmtPtr blockStatement();
    StmtPtr expressionStatement();
    StmtPtr modelStatement();
    StmtPtr structStatement();
    StmtPtr useStatement();
    StmtPtr tryStatement();
    StmtPtr throwStatement();
    
    // Expression parsing (precedence climbing)
    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr logicalOr();
    ExprPtr logicalAnd();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary();
    ExprPtr call();
    ExprPtr primary();
    
    ExprPtr finishCall(ExprPtr callee);
    ExprPtr lambdaExpression();
};

#endif // PARSER_H
