#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <string>
#include <stdexcept>
#include "lexer/Token.h"
#include "ast/AST.h"
#include "ast/ASTArena.h"

class ParseError : public std::runtime_error {
public:
    int line;
    ParseError(const std::string& message, int line) 
        : std::runtime_error(message), line(line) {}
};

class Parser {
public:
    Parser(std::vector<Token> tokens, ASTArena& arena);
    std::vector<StmtPtr> parse();
    bool hasError() const { return hadError; }

private:
    std::vector<Token> tokens;
    ASTArena& arena;
    size_t current = 0;
    bool hadError = false;

    // Token navigation
    bool isAtEnd() const;
    const Token& peek() const;
    const Token& peekNext() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool match(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& message);
    
    // Skip newlines
    void skipNewlines();

    
    // Error handling
    void error(const Token& token, const std::string& message);
    void synchronize();

    // If `t` is a keyword that can only appear as part of a larger construct
    // (e.g. 'finally' only after 'try', 'other' only inside 'when'), returns
    // a message explaining where it actually belongs. Returns nullptr for
    // any token that isn't one of these "orphan-able" keywords.
    static const char* describeMisplacedKeyword(TokenType t);
    // True for any reserved keyword token (not IDENTIFIER, not an operator/
    // punctuation) -- used to give a better error when a name is expected
    // but a keyword was typed instead (e.g. `model try { ... }`).
    static bool isKeywordToken(TokenType t);
    
    // Type parsing
    TypeASTPtr parseType();

    // Statement parsing
    StmtPtr declaration();
    StmtPtr varDeclStatement();
    StmtPtr statement();
    StmtPtr outStatement();
    StmtPtr whenStatement();
    StmtPtr whileStatement();
    StmtPtr repeatStatement();
    StmtPtr getStatement();
    StmtPtr matchStatement();
    StmtPtr staticStatement();
    StmtPtr taskStatement(bool isAsync = false);
    StmtPtr giveStatement();
    StmtPtr escapeStatement();
    StmtPtr skipStatement();
    StmtPtr blockStatement();
    StmtPtr expressionStatement();
    StmtPtr modelStatement();
    StmtPtr interfaceStatement();
    StmtPtr structStatement();
    StmtPtr enumStatement();
    StmtPtr useStatement();
    StmtPtr tryStatement();
    StmtPtr throwStatement();
    StmtPtr exportStatement();
    StmtPtr testStatement();
    
    // Expression parsing (precedence climbing)
    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr ternary();
    ExprPtr nullCoalescing();
    ExprPtr logicalOr();
    ExprPtr logicalAnd();
    ExprPtr bitwiseOr();
    ExprPtr bitwiseXor();
    ExprPtr bitwiseAnd();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr bitwiseShift();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr power();
    ExprPtr unary();
    ExprPtr call();
    ExprPtr primary();
    
    ExprPtr finishCall(ExprPtr callee);
    // noParams: the caller has no `|...|` list to parse (an `async { ... }`
    // block, which is a zero-parameter lambda).
    ExprPtr lambdaExpression(bool isAsync = false, bool noParams = false);
};

#endif // PARSER_H
