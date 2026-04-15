#include "Parser.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

std::vector<StmtPtr> Parser::parse() {
    std::vector<StmtPtr> statements;
    
    skipNewlines();
    
    while (!isAtEnd()) {
        try {
            auto stmt = declaration();
            if (stmt) {
                statements.push_back(stmt);
            }
            skipNewlines();
        } catch (const ParseError& e) {
            synchronize();
        }
    }
    
    return statements;
}

// ============ Token Navigation ============

bool Parser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}

const Token& Parser::peek() const {
    return tokens[current];
}

const Token& Parser::previous() const {
    return tokens[current - 1];
}

const Token& Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    throw ParseError(message, peek().line);
}

void Parser::skipNewlines() {
    while (match(TokenType::NEWLINE)) {}
}

void Parser::consumeNewlines() {
    if (!isAtEnd() && !check(TokenType::OTHER)) {
        // Expect at least one newline or end of file after statements
        if (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE) && !check(TokenType::RBRACE)) {
            // Allow no newline if we're at end or before 'other'
        }
    }
    skipNewlines();
}

void Parser::error(const Token& token, const std::string& message) {
    hadError = true;
    std::cerr << "[" << token.filename << ":" << token.line << "] Error";
    if (token.type == TokenType::END_OF_FILE) {
        std::cerr << " at end";
    } else {
        std::cerr << " at '" << token.lexeme << "'";
    }
    std::cerr << ": " << message << std::endl;
}

void Parser::synchronize() {
    advance();
    
    while (!isAtEnd()) {
        if (previous().type == TokenType::NEWLINE) return;
        
        switch (peek().type) {
            case TokenType::TASK:
            case TokenType::WHEN:
            case TokenType::WHILE:
            case TokenType::REPEAT:
            case TokenType::GET:
            case TokenType::OUT:
            case TokenType::GIVE:
            case TokenType::ESCAPE:
            case TokenType::SKIP:
                return;
            default:
                break;
        }
        
        advance();
    }
}

// ============ Statement Parsing ============

StmtPtr Parser::declaration() {
    try {
        return statement();
    } catch (const ParseError& e) {
        error(peek(), e.what());
        synchronize();
        return nullptr;
    }
}

StmtPtr Parser::statement() {
    if (match(TokenType::INTERFACE)) return interfaceStatement();
    if (match(TokenType::OUT)) return outStatement();
    if (match(TokenType::WHEN)) return whenStatement();
    if (match(TokenType::WHILE)) return whileStatement();
    if (match(TokenType::REPEAT)) return repeatStatement();
    if (match(TokenType::GET)) return getStatement();
    if (match(TokenType::STATIC)) return staticStatement();
    if (match(TokenType::TASK)) return taskStatement();
    if (match(TokenType::GIVE)) return giveStatement();
    if (match(TokenType::ESCAPE)) return escapeStatement();
    if (match(TokenType::SKIP)) return skipStatement();
    if (match(TokenType::LBRACE)) return blockStatement();
    if (match(TokenType::MODEL)) return modelStatement();
    if (match(TokenType::STRUCT)) return structStatement();
    if (match(TokenType::USE)) return useStatement();
    if (match(TokenType::TRY)) return tryStatement();
    if (match(TokenType::THROW)) return throwStatement();
    
    return expressionStatement();
}

StmtPtr Parser::outStatement() {
    Token op = previous();
    ExprPtr value = expression();
    return makeOutStmt(op.line, op.filename, value);
}

StmtPtr Parser::whenStatement() {
    int line = previous().line;
    
    ExprPtr condition = expression();
    skipNewlines();
    
    StmtPtr thenBranch;
    if (match(TokenType::LBRACE)) {
        thenBranch = blockStatement();
    } else {
        // Single statement (indented block style)
        std::vector<StmtPtr> thenStmts;
        skipNewlines();
        
        // Parse statements until 'other' or dedent
        while (!isAtEnd() && !check(TokenType::OTHER)) {
            // Check if we've hit a top-level statement
            if (check(TokenType::TASK) || check(TokenType::WHEN) || 
                check(TokenType::WHILE) || check(TokenType::REPEAT) ||
                check(TokenType::GET)) {
                break;
            }
            
            auto stmt = statement();
            if (stmt) thenStmts.push_back(stmt);
            skipNewlines();
            
            // If only parsing single statement without brace
            if (thenStmts.size() == 1 && !check(TokenType::OTHER)) {
                break;
            }
        }
        
        if (thenStmts.size() == 1) {
            thenBranch = thenStmts[0];
        } else {
            thenBranch = makeBlockStmt(line, peek().filename, thenStmts);
        }
    }
    
    skipNewlines();
    
    StmtPtr elseBranch = nullptr;
    if (match(TokenType::OTHER)) {
        skipNewlines();
        if (match(TokenType::WHEN)) {
            // else if
            elseBranch = whenStatement();
        } else if (match(TokenType::LBRACE)) {
            elseBranch = blockStatement();
        } else {
            // Single statement else
            elseBranch = statement();
        }
    }
    
    return makeWhenStmt(line, peek().filename, condition, thenBranch, elseBranch);
}

StmtPtr Parser::whileStatement() {
    int line = previous().line;
    
    ExprPtr condition = expression();
    skipNewlines();
    
    StmtPtr body;
    if (match(TokenType::LBRACE)) {
        body = blockStatement();
    } else {
        body = statement();
    }
    
    return makeWhileStmt(line, peek().filename, condition, body);
}

StmtPtr Parser::repeatStatement() {
    int line = previous().line;
    
    // repeat i = 0 to 10
    Token varToken = consume(TokenType::IDENTIFIER, "Expected variable name after 'repeat'");
    std::string varName = varToken.lexeme;
    
    consume(TokenType::EQUAL, "Expected '=' after variable name");
    
    ExprPtr startValue = expression();
    
    consume(TokenType::TO, "Expected 'to' in repeat statement");
    
    ExprPtr endValue = expression();
    
    skipNewlines();
    
    StmtPtr body;
    if (match(TokenType::LBRACE)) {
        body = blockStatement();
    } else {
        body = statement();
    }
    
    return makeRepeatStmt(line, varToken.filename, varName, startValue, endValue, body);
}

StmtPtr Parser::getStatement() {
    int line = previous().line;
    
    // get x in array
    Token varToken = consume(TokenType::IDENTIFIER, "Expected variable name after 'get'");
    std::string varName = varToken.lexeme;
    
    consume(TokenType::IN, "Expected 'in' after variable name");
    
    ExprPtr iterable = expression();
    
    skipNewlines();
    
    StmtPtr body;
    if (match(TokenType::LBRACE)) {
        body = blockStatement();
    } else {
        body = statement();
    }
    
    return makeGetStmt(line, varToken.filename, varName, iterable, body);
}

StmtPtr Parser::taskStatement() {
    int line = previous().line;
    
    // Allow any token as function name (for operator overloading)
    advance();
    Token nameToken = previous();
    std::string name = nameToken.lexeme;
    
    consume(TokenType::LPAREN, "Expected '(' after function name");
    
    std::vector<std::string> params;
    std::vector<ExprPtr> defaultValues;
    bool hadDefault = false;
    bool isVariadic = false;
    
    if (!check(TokenType::RPAREN)) {
        do {
            if (match(TokenType::ELLIPSIS)) {
                isVariadic = true;
                Token paramToken = consume(TokenType::IDENTIFIER, "Expected parameter name after '...'");
                params.push_back(paramToken.lexeme);
                defaultValues.push_back(nullptr);
                if (check(TokenType::COMMA)) {
                    error(peek(), "Rest parameter must be the last parameter");
                }
                break;
            }
            
            Token paramToken = advance();
            if (paramToken.type == TokenType::RPAREN || paramToken.type == TokenType::COMMA) {
                throw ParseError("Expected parameter name", paramToken.line);
            }
            params.push_back(paramToken.lexeme);
            
            // Check for default value: param = expr
            if (match(TokenType::EQUAL)) {
                ExprPtr defaultVal = expression();
                defaultValues.push_back(defaultVal);
                hadDefault = true;
            } else {
                if (hadDefault) {
                    error(paramToken, "Required parameter cannot follow optional parameter");
                }
                defaultValues.push_back(nullptr);
            }
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::RPAREN, "Expected ')' after parameters");
    skipNewlines();
    
    std::vector<StmtPtr> body;
    if (match(TokenType::LBRACE)) {
        skipNewlines();
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            auto stmt = declaration();
            if (stmt) body.push_back(stmt);
            skipNewlines();
        }
        consume(TokenType::RBRACE, "Expected '}' after function body");
    } else {
        // Single statement body
        auto stmt = statement();
        if (stmt) body.push_back(stmt);
    }
    
    return makeTaskStmt(line, nameToken.filename, name, params, defaultValues, body, isVariadic);
}

StmtPtr Parser::giveStatement() {
    int line = previous().line;
    
    ExprPtr value = nullptr;
    if (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE) && !check(TokenType::RBRACE)) {
        value = expression();
    }
    
    return makeGiveStmt(line, peek().filename, value);
}

StmtPtr Parser::escapeStatement() {
    Token op = previous();
    return makeEscapeStmt(op.line, op.filename);
}

StmtPtr Parser::skipStatement() {
    Token op = previous();
    return makeSkipStmt(op.line, op.filename);
}

StmtPtr Parser::staticStatement() {
    Token op = previous();
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected variable name after 'static'");
    consume(TokenType::EQUAL, "Expected '=' after static variable name");
    ExprPtr value = expression();
    return makeStaticStmt(op.line, op.filename, nameToken.lexeme, value);
}

StmtPtr Parser::tryStatement() {
    int line = previous().line;
    
    consume(TokenType::LBRACE, "Expected '{' after 'try'");
    StmtPtr tryBlock = blockStatement();
    
    skipNewlines();
    
    consume(TokenType::CATCH, "Expected 'catch' after try block");
    Token varToken = consume(TokenType::IDENTIFIER, "Expected variable name after 'catch'");
    std::string catchVar = varToken.lexeme;
    
    skipNewlines();
    
    consume(TokenType::LBRACE, "Expected '{' after catch variable");
    StmtPtr catchBlock = blockStatement();
    
    return makeTryStmt(line, varToken.filename, tryBlock, catchVar, catchBlock);
}

StmtPtr Parser::throwStatement() {
    Token op = previous();
    ExprPtr expr = expression();
    return makeThrowStmt(op.line, op.filename, expr);
}

StmtPtr Parser::blockStatement() {
    int line = previous().line;
    std::vector<StmtPtr> statements;
    
    skipNewlines();
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        auto stmt = declaration();
        if (stmt) statements.push_back(stmt);
        skipNewlines();
    }
    
    consume(TokenType::RBRACE, "Expected '}' after block");
    
    return makeBlockStmt(line, peek().filename, statements);
}

StmtPtr Parser::expressionStatement() {
    int line = peek().line;
    ExprPtr expr = expression();
    
    // Check if this is a variable declaration (assignment to new variable)
    if (auto* exprNode = expr.get()) {
        if (auto* assignExpr = std::get_if<std::shared_ptr<AssignExpr>>(&exprNode->variant)) {
            if (!(*assignExpr)->index) {
                // Simple assignment, treat as var declaration
                return makeVarDeclStmt(line, peek().filename, (*assignExpr)->name, (*assignExpr)->value);
            }
        }
    }
    
    return makeExprStmt(line, peek().filename, expr);
}

StmtPtr Parser::structStatement() {
    int line = previous().line;
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected struct name");
    std::string name = nameToken.lexeme;
    
    consume(TokenType::LBRACE, "Expected '{' before struct body");
    
    std::vector<std::string> fields;
    skipNewlines();
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token field = consume(TokenType::IDENTIFIER, "Expected field name");
        fields.push_back(field.lexeme);
        
        if (match(TokenType::COMMA)) {
            skipNewlines();
        } else if (check(TokenType::NEWLINE)) {
            skipNewlines();
        }
    }
    
    consume(TokenType::RBRACE, "Expected '}' after struct body");
    
    return makeStructStmt(line, nameToken.filename, name, fields);
}

StmtPtr Parser::useStatement() {
    int line = previous().line;
    if (!match(TokenType::STRING)) {
        throw ParseError("Expected string path after 'use'", peek().line);
    }
    Token pathToken = previous();
    std::string path = std::get<std::string>(pathToken.literal);
    return makeUseStmt(line, pathToken.filename, path);
}

// ============ Expression Parsing ============

ExprPtr Parser::expression() {
    return assignment();
}

ExprPtr Parser::assignment() {
    ExprPtr expr = ternary();
    
    if (match({TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, 
               TokenType::STAR_EQUAL, TokenType::SLASH_EQUAL})) {
        Token op = previous();
        ExprPtr value = assignment();
        
        // Handle compound assignment
        if (op.type != TokenType::EQUAL) {
            TokenType binOp;
            switch (op.type) {
                case TokenType::PLUS_EQUAL: binOp = TokenType::PLUS; break;
                case TokenType::MINUS_EQUAL: binOp = TokenType::MINUS; break;
                case TokenType::STAR_EQUAL: binOp = TokenType::STAR; break;
                case TokenType::SLASH_EQUAL: binOp = TokenType::SLASH; break;
                default: binOp = TokenType::PLUS; break;
            }
            value = makeBinaryExpr(op.line, op.filename, expr, binOp, value);
        }
        
        if (std::holds_alternative<std::shared_ptr<IdentifierExpr>>(expr->variant)) {
            std::string name = std::get<std::shared_ptr<IdentifierExpr>>(expr->variant)->name;
            return makeAssignExpr(op.line, op.filename, name, value);
        } else if (std::holds_alternative<std::shared_ptr<IndexExpr>>(expr->variant)) {
            auto indexExpr = std::get<std::shared_ptr<IndexExpr>>(expr->variant);
            // Handle obj[idx] = val where obj can be complex
            return makeAssignExpr(op.line, op.filename, "", value, indexExpr->index, indexExpr->object);
        } else if (std::holds_alternative<std::shared_ptr<PropertyAccessExpr>>(expr->variant)) {
            auto propExpr = std::get<std::shared_ptr<PropertyAccessExpr>>(expr->variant);
            return makeSetExpr(op.line, op.filename, propExpr->object, propExpr->property, value);
        }
        
        error(op, "Invalid assignment target");
    }
    
    return expr;
}

ExprPtr Parser::ternary() {
    ExprPtr expr = logicalOr();
    
    if (match(TokenType::QUESTION_MARK)) {
        Token op = previous();
        ExprPtr thenBranch = expression();
        consume(TokenType::COLON, "Expected ':' after then branch of ternary operator");
        ExprPtr elseBranch = ternary(); // Right-associative
        expr = makeTernaryExpr(op.line, op.filename, expr, thenBranch, elseBranch);
    }
    
    return expr;
}

ExprPtr Parser::logicalOr() {
    ExprPtr expr = logicalAnd();
    
    while (match(TokenType::OR)) {
        Token op = previous();
        ExprPtr right = logicalAnd();
        expr = makeLogicalExpr(op.line, op.filename, expr, TokenType::OR, right);
    }
    
    return expr;
}

ExprPtr Parser::logicalAnd() {
    ExprPtr expr = equality();
    
    while (match(TokenType::AND)) {
        Token op = previous();
        ExprPtr right = equality();
        expr = makeLogicalExpr(op.line, op.filename, expr, TokenType::AND, right);
    }
    
    return expr;
}

ExprPtr Parser::equality() {
    ExprPtr expr = comparison();
    
    while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL})) {
        Token op = previous();
        ExprPtr right = comparison();
        expr = makeBinaryExpr(op.line, op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::comparison() {
    ExprPtr expr = term();
    
    while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, 
                  TokenType::LESS, TokenType::LESS_EQUAL, TokenType::IN})) {
        Token op = previous();
        ExprPtr right = term();
        expr = makeBinaryExpr(op.line, op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::term() {
    ExprPtr expr = factor();
    
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        Token op = previous();
        ExprPtr right = factor();
        expr = makeBinaryExpr(op.line, op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::factor() {
    ExprPtr expr = unary();
    
    while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        Token op = previous();
        ExprPtr right = unary();
        expr = makeBinaryExpr(op.line, op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::unary() {
    if (match({TokenType::BANG, TokenType::MINUS, TokenType::NOT})) {
        Token op = previous();
        ExprPtr right = unary();
        return makeUnaryExpr(op.line, op.filename, op.type, right);
    }
    
    return call();
}

ExprPtr Parser::call() {
    ExprPtr expr = primary();
    
    while (true) {
        if (match(TokenType::LPAREN)) {
            expr = finishCall(expr);
        } else if (match(TokenType::LBRACKET)) {
            Token op = previous();
            ExprPtr index = expression();
            consume(TokenType::RBRACKET, "Expected ']' after index");
            expr = makeIndexExpr(op.line, op.filename, expr, index);
        } else if (match(TokenType::DOT)) {
            // Allow keywords as property names
            advance();
            Token name = previous();
            expr = makePropertyAccessExpr(name.line, name.filename, expr, name.lexeme);
        } else {
            // Check if next non-newline token is a DOT (Multi-line chaining support)
            size_t temp = current;
            while (temp < tokens.size() && tokens[temp].type == TokenType::NEWLINE) {
                temp++;
            }
            if (temp < tokens.size() && tokens[temp].type == TokenType::DOT) {
                current = temp; // Skip newlines
                advance(); // Consume DOT
                
                // Now must have an identifier (or keyword)
                if (isAtEnd()) throw ParseError("Expected property name after '.'", peek().line);
                advance(); 
                Token name = previous();
                expr = makePropertyAccessExpr(name.line, name.filename, expr, name.lexeme);
            } else {
                break;
            }
        }
    }
    
    return expr;
}

ExprPtr Parser::finishCall(ExprPtr callee) {
    int line = previous().line;
    std::vector<ExprPtr> arguments;
    
    if (!check(TokenType::RPAREN)) {
        do {
            if (match(TokenType::ELLIPSIS)) {
                arguments.push_back(makeSpreadExpr(line, peek().filename, expression()));
            } else {
                arguments.push_back(expression());
            }
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::RPAREN, "Expected ')' after arguments");
    
    return makeCallExpr(line, peek().filename, callee, arguments);
}

ExprPtr Parser::primary() {
    int line = peek().line;
    std::string filename = peek().filename;
    
    if (match(TokenType::FALSE)) return makeLiteralExpr(line, filename, false);
    if (match(TokenType::TRUE)) return makeLiteralExpr(line, filename, true);
    if (match(TokenType::NIL)) return makeLiteralExpr(line, filename, nullptr);
    
    if (match(TokenType::NUMBER)) {
        Token t = previous();
        return makeLiteralExpr(t.line, t.filename, std::get<double>(t.literal));
    }
    
    if (match(TokenType::STRING)) {
        Token t = previous();
        return makeLiteralExpr(t.line, t.filename, std::get<std::string>(t.literal));
    }
    
    if (match(TokenType::IDENTIFIER)) {
        Token t = previous();
        return makeIdentifierExpr(t.line, t.filename, t.lexeme);
    }
    
    if (match(TokenType::SUPER)) {
        Token t = previous();
        return makeIdentifierExpr(t.line, t.filename, "super");
    }
    
    // Self reference
    if (match(TokenType::SELF)) {
        Token t = previous();
        return makeSelfExpr(t.line, t.filename);
    }
    
    if (match(TokenType::IN)) {
        Token t = previous();
        // Special case for 'in' keyword used alone (not as operator)
        // Historically mapped to __input__
        return makeCallExpr(t.line, t.filename, makeIdentifierExpr(t.line, t.filename, "__input__"), {});
    }
    
    // Lambda expression: |params| => expr or |params| { body }
    if (match(TokenType::PIPE)) {
        return lambdaExpression();
    }
    
    if (match(TokenType::LBRACKET)) {
        // Array literal
        std::vector<ExprPtr> elements;
        
        if (!check(TokenType::RBRACKET)) {
            do {
                skipNewlines();
                if (match(TokenType::ELLIPSIS)) {
                    elements.push_back(makeSpreadExpr(line, filename, expression()));
                } else {
                    elements.push_back(expression());
                }
                skipNewlines();
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RBRACKET, "Expected ']' after array elements");
        return makeArrayExpr(line, filename, elements);
    }
    
    // Dictionary literal
    if (match(TokenType::LBRACE)) {
        int line = previous().line;
        std::vector<std::pair<ExprPtr, ExprPtr>> pairs;
        
        skipNewlines();
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            ExprPtr expr = expression();
            
            if (std::holds_alternative<std::shared_ptr<AssignExpr>>(expr->variant)) {
                 auto assign = std::get<std::shared_ptr<AssignExpr>>(expr->variant);
                 ExprPtr key = makeLiteralExpr(expr->line, expr->filename, assign->name);
                 pairs.push_back({key, assign->value});
            } else {
                 ExprPtr key = expr;
                 
                 bool isAssign = match(TokenType::EQUAL);
                 if (!isAssign) {
                      consume(TokenType::COLON, "Expected ':' or '=' after dictionary key");
                 }
                 
                 // Support keys like {x: 1} or {x=1} -> {"x": 1}
                 // If the key is an identifier, convert it to a string literal
                 if (std::holds_alternative<std::shared_ptr<IdentifierExpr>>(key->variant)) {
                      auto ident = std::get<std::shared_ptr<IdentifierExpr>>(key->variant);
                      key = makeLiteralExpr(key->line, key->filename, ident->name);
                 }
                 
                 ExprPtr value = expression();
                 pairs.push_back({key, value});
            }
            
            if (match(TokenType::COMMA)) {
                skipNewlines();
            } else if (check(TokenType::NEWLINE)) {
                skipNewlines();
            }
        }
        
        consume(TokenType::RBRACE, "Expected '}' after dictionary");
        return makeDictionaryExpr(line, filename, pairs);
    }
    
    if (match(TokenType::LPAREN)) {
        ExprPtr expr = expression();
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    throw ParseError("Expected expression", line);
}

// Lambda: |x, y| => x + y  OR  |x, y| { statements }
ExprPtr Parser::lambdaExpression() {
    int line = previous().line;
    
    // Parse parameters
    std::vector<std::string> params;
    bool isVariadic = false;
    
    if (!check(TokenType::PIPE)) {
        do {
            if (match(TokenType::ELLIPSIS)) {
                isVariadic = true;
                Token paramToken = consume(TokenType::IDENTIFIER, "Expected parameter name after '...'");
                params.push_back(paramToken.lexeme);
                if (check(TokenType::COMMA)) {
                    error(peek(), "Rest parameter must be the last parameter");
                }
                break;
            }
            Token paramToken = consume(TokenType::IDENTIFIER, "Expected parameter name");
            params.push_back(paramToken.lexeme);
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::PIPE, "Expected '|' after lambda parameters");
    
    skipNewlines();
    
    // Check for block body { ... } or expression body => expr
    if (match(TokenType::ARROW)) {
        // Expression body
        skipNewlines();
        ExprPtr body = expression();
        return makeLambdaExpr(line, peek().filename, params, body, isVariadic);
    } else if (match(TokenType::LBRACE)) {
        // Statement body
        skipNewlines();
        std::vector<StmtPtr> stmtBody;
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            auto stmt = declaration();
            if (stmt) stmtBody.push_back(stmt);
            skipNewlines();
        }
        consume(TokenType::RBRACE, "Expected '}' after lambda body");
        return makeLambdaExpr(line, peek().filename, params, stmtBody, isVariadic);
    } else {
        // Default: treat as expression body without arrow
        ExprPtr body = expression();
        return makeLambdaExpr(line, peek().filename, params, body, isVariadic);
    }
}

// Model (class) definition
// Syntax: model Name extends Parent { init(params) { body } shown/hidden members and methods }
StmtPtr Parser::modelStatement() {
    int line = previous().line;
    
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected model name");
    std::string name = nameToken.lexeme;
    
    // Check for inheritance
    std::string parentName = "";
    if (match(TokenType::EXTENDS)) {
        Token parentToken = consume(TokenType::IDENTIFIER, "Expected parent model name");
        parentName = parentToken.lexeme;
    }
    
    std::vector<std::string> interfaces;
    if (match(TokenType::IMPLEMENTS)) {
        do {
            Token interfaceToken = consume(TokenType::IDENTIFIER, "Expected interface name");
            interfaces.push_back(interfaceToken.lexeme);
        } while (match(TokenType::COMMA));
    }
    
    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' after model name");
    skipNewlines();
    
    std::vector<std::string> initParams;
    std::vector<StmtPtr> initBody;
    std::vector<ModelMember> members;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        MemberVisibility visibility = MemberVisibility::PUBLIC;
        
        // Check for visibility modifiers
        if (match(TokenType::HIDDEN)) {
            visibility = MemberVisibility::PRIVATE;
        } else if (match(TokenType::SHOWN)) {
            visibility = MemberVisibility::PUBLIC;
        }
        
        bool isStatic = false;
        if (match(TokenType::STATIC)) {
            isStatic = true;
        }
        
        // Check for init (constructor)
        if (match(TokenType::INIT)) {
            if (isStatic) error(previous(), "Constructor cannot be static");
            consume(TokenType::LPAREN, "Expected '(' after 'init'");
            
            if (!check(TokenType::RPAREN)) {
                do {
                    Token paramToken = consume(TokenType::IDENTIFIER, "Expected parameter name");
                    initParams.push_back(paramToken.lexeme);
                } while (match(TokenType::COMMA));
            }
            
            consume(TokenType::RPAREN, "Expected ')' after init parameters");
            skipNewlines();
            
            if (match(TokenType::LBRACE)) {
                skipNewlines();
                while (!check(TokenType::RBRACE) && !isAtEnd()) {
                    auto stmt = declaration();
                    if (stmt) initBody.push_back(stmt);
                    skipNewlines();
                }
                consume(TokenType::RBRACE, "Expected '}' after init body");
            }
        }
        // Check for task (method)
        else if (match(TokenType::TASK)) {
            // Allow keywords as method names
            advance();
            Token methodName = previous();
            consume(TokenType::LPAREN, "Expected '(' after method name");
            
            std::vector<std::string> params;
            if (!check(TokenType::RPAREN)) {
                do {
                    Token paramToken = advance();
                    if (paramToken.type == TokenType::RPAREN || paramToken.type == TokenType::COMMA) {
                        throw ParseError("Expected parameter name", paramToken.line);
                    }
                    params.push_back(paramToken.lexeme);
                } while (match(TokenType::COMMA));
            }
            
            consume(TokenType::RPAREN, "Expected ')' after method parameters");
            skipNewlines();
            
            std::vector<StmtPtr> body;
            if (match(TokenType::LBRACE)) {
                skipNewlines();
                while (!check(TokenType::RBRACE) && !isAtEnd()) {
                    auto stmt = declaration();
                    if (stmt) body.push_back(stmt);
                    skipNewlines();
                }
                consume(TokenType::RBRACE, "Expected '}' after method body");
            }
            
            ModelMember member;
            member.visibility = visibility;
            member.isStatic = isStatic;
            member.isMethod = true;
            member.name = methodName.lexeme;
            member.params = params;
            member.body = body;
            members.push_back(member);
        }
        // Property declaration
        else if (check(TokenType::IDENTIFIER)) {
            Token propName = advance();
            
            ExprPtr initializer = nullptr;
            if (match(TokenType::EQUAL)) {
                initializer = expression();
            }
            
            ModelMember member;
            member.visibility = visibility;
            member.isStatic = isStatic;
            member.isMethod = false;
            member.name = propName.lexeme;
            member.initializer = initializer;
            members.push_back(member);
        } else {
            error(peek(), "Unexpected token in model body");
            advance(); // Avoid infinite loop
        }
        
        skipNewlines();
    }
    
    consume(TokenType::RBRACE, "Expected '}' after model body");
    
    // Use the peek().filename or current filename
    return makeModelStmt(line, nameToken.filename, name, parentName, interfaces, initParams, initBody, members);
}

// Interface definition
// Syntax: interface Name { task method1() task method2() ... }
StmtPtr Parser::interfaceStatement() {
    int line = previous().line;
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected interface name");
    
    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' after interface name");
    skipNewlines();
    
    std::vector<std::string> methods;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        consume(TokenType::TASK, "Expected 'task' in interface definition");
        Token methodToken = consume(TokenType::IDENTIFIER, "Expected method name");
        consume(TokenType::LPAREN, "Expected '(' after method name");
        while (!check(TokenType::RPAREN) && !isAtEnd()) advance();
        consume(TokenType::RPAREN, "Expected ')' after method parameters");
        methods.push_back(methodToken.lexeme);
        skipNewlines();
    }
    consume(TokenType::RBRACE, "Expected '}' after interface body");
    
    return makeInterfaceStmt(line, nameToken.filename, nameToken.lexeme, methods);
}
