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

#include "BytecodeVM.h"

void Parser::error(const Token& token, const std::string& message) {
    hadError = true;
    if (token.type == TokenType::END_OF_FILE) {
        std::cerr << "\nError: " << message << " (at end of file)\n"
                  << "  " << (token.filename.empty() ? "<unknown>" : token.filename)
                  << ":" << token.line << "\n\n";
    } else {
        std::cerr << "\nError: " << message << "\n"
                  << "  " << (token.filename.empty() ? "<unknown>" : token.filename)
                  << ":" << token.line << ":" << token.column << " near '" << token.lexeme << "'\n\n";
    }
    
    const std::string* sourceLine = EZ_GetSourceLine(token.filename, token.line);
    if (sourceLine) {
        std::cerr << "    " << *sourceLine << "\n";
        std::string caret(token.column > 0 ? token.column - 1 : 0, ' ');
        // If it's EOF, we can just point to the end of the line
        if (token.type == TokenType::END_OF_FILE) {
            caret = std::string(sourceLine->length(), ' ');
        }
        std::cerr << "    " << caret << "^\n";
    }
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

TypeASTPtr Parser::parseType() {
    Token typeToken = consume(TokenType::IDENTIFIER, "Expected type name");
    std::string baseType = typeToken.lexeme;
    
    std::vector<TypeASTPtr> typeArgs;
    if (match(TokenType::LBRACKET)) {
        do {
            typeArgs.push_back(parseType());
        } while (match(TokenType::COMMA));
        consume(TokenType::RBRACKET, "Expected ']' after type arguments");
    }
    
    return std::make_shared<TypeAST>(baseType, typeArgs);
}

StmtPtr Parser::varDeclStatement() {
    int line = peek().line;
    int column = peek().column;
    int length = peek().lexeme.length();
    std::string filename = peek().filename;
    
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected variable name");
    consume(TokenType::COLON, "Expected ':' after variable name");
    
    TypeASTPtr typeHint = parseType();
    
    ExprPtr initializer = nullptr;
    if (match(TokenType::EQUAL)) {
        initializer = expression();
    }
    
    return makeVarDeclStmt(line, column, length, filename, nameToken.lexeme, initializer, typeHint);
}


StmtPtr Parser::declaration() {
    try {
        if (check(TokenType::IDENTIFIER) && current + 1 < tokens.size() && tokens[current + 1].type == TokenType::COLON) {
            return varDeclStatement();
        }
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
    if (match(TokenType::MATCH)) return matchStatement();
    if (match(TokenType::STATIC)) return staticStatement();
    
    // ── Collect decorator tokens ──────────────────────────────────────────────
    // Decorators like @audited, @cached, @persist etc. precede model/task
    std::vector<TokenType> decorators;
    std::string persistPath;
    std::optional<RateLimitConfig> rateLimitCfg;
    while (check(TokenType::DECORATOR_AUDITED)  || check(TokenType::DECORATOR_SNAPSHOT) ||
           check(TokenType::DECORATOR_CACHED)   || check(TokenType::DECORATOR_PERSIST)  ||
           check(TokenType::DECORATOR_VALIDATE) || check(TokenType::DECORATOR_RATELIMIT)) {
        TokenType kind = peek().type;
        advance(); // consume decorator token
        if (kind == TokenType::DECORATOR_PERSIST) {
            consume(TokenType::LPAREN, "Expected '(' after @persist");
            Token pathTok = consume(TokenType::STRING, "Expected file path string in @persist");
            persistPath = std::get<std::string>(pathTok.literal);
            consume(TokenType::RPAREN, "Expected ')' after @persist path");
        } else if (kind == TokenType::DECORATOR_RATELIMIT) {
            consume(TokenType::LPAREN, "Expected '(' after @ratelimit");
            // count
            Token countTok = consume(TokenType::NUMBER, "Expected count in @ratelimit");
            int count = 0;
            if (std::holds_alternative<long long>(countTok.literal)) {
                count = (int)std::get<long long>(countTok.literal);
            } else {
                count = (int)std::get<double>(countTok.literal);
            }
            consume(TokenType::COMMA, "Expected ',' in @ratelimit");
            Token perTok = consume(TokenType::STRING, "Expected time unit string");
            std::string perStr = std::get<std::string>(perTok.literal);
            rateLimitCfg = RateLimitConfig{count, perStr, nullptr};
            consume(TokenType::RPAREN, "Expected ')' after @ratelimit arguments");
        }
        decorators.push_back(kind);
        skipNewlines();
    }

    bool isAsync = false;
    if (match(TokenType::ASYNC)) {
        isAsync = true;
    }
    
    if (match(TokenType::TASK)) {
        auto taskNode = taskStatement(isAsync);
        // Apply task-level decorators
        auto& task = *std::get<std::shared_ptr<TaskStmt>>(taskNode->variant);
        for (auto kind : decorators) {
            if (kind == TokenType::DECORATOR_CACHED)   task.isCached = true;
            if (kind == TokenType::DECORATOR_RATELIMIT && rateLimitCfg) task.rateLimit = rateLimitCfg;
        }
        return taskNode;
    }
    if (isAsync) {
        error(previous(), "Expected 'task' after 'async' modifier");
        return nullptr;
    }
    
    if (match(TokenType::MODEL)) {
        auto modelNode = modelStatement();
        // Apply model-level decorators
        auto& model = *std::get<std::shared_ptr<ModelStmt>>(modelNode->variant);
        for (auto kind : decorators) {
            if (kind == TokenType::DECORATOR_AUDITED)  model.audited  = true;
            if (kind == TokenType::DECORATOR_SNAPSHOT) model.snapshot = true;
            if (kind == TokenType::DECORATOR_PERSIST)  model.persistPath = persistPath;
        }
        return modelNode;
    }
    if (!decorators.empty()) {
        error(previous(), "Decorators can only be applied to 'model' or 'task' declarations");
    }

    if (match(TokenType::GIVE)) return giveStatement();
    if (match(TokenType::ESCAPE)) return escapeStatement();
    if (match(TokenType::SKIP)) return skipStatement();
    if (match(TokenType::LBRACE)) return blockStatement();
    if (match(TokenType::STRUCT)) return structStatement();
    if (match(TokenType::USE)) return useStatement();
    if (match(TokenType::TRY)) return tryStatement();
    if (match(TokenType::THROW)) return throwStatement();
    if (match(TokenType::EXPORT)) return exportStatement();
    
    return expressionStatement();
}

StmtPtr Parser::outStatement() {
    Token op = previous();
    ExprPtr value = expression();
    return makeOutStmt(op.line, op.column, op.lexeme.length(), op.filename, value);
}

StmtPtr Parser::whenStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    
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
            thenBranch = makeBlockStmt(line, column, length, peek().filename, thenStmts);
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
    
    return makeWhenStmt(line, column, length, peek().filename, condition, thenBranch, elseBranch);
}

StmtPtr Parser::whileStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    
    ExprPtr condition = expression();
    skipNewlines();
    
    StmtPtr body;
    if (match(TokenType::LBRACE)) {
        body = blockStatement();
    } else {
        body = statement();
    }
    
    return makeWhileStmt(line, column, length, peek().filename, condition, body);
}

StmtPtr Parser::repeatStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    
    // repeat i = 0 to 10
    Token varToken = consume(TokenType::IDENTIFIER, "Expected variable name after 'repeat'");
    std::string varName = varToken.lexeme;
    
    consume(TokenType::EQUAL, "Expected '=' after variable name");
    
    ExprPtr startValue = expression();
    
    consume(TokenType::TO, "Expected 'to' in repeat statement");
    
    ExprPtr endValue = expression();
    
    ExprPtr stepValue = nullptr;
    if (match(TokenType::STEP)) {
        stepValue = expression();
    }
    
    skipNewlines();
    
    StmtPtr body;
    if (match(TokenType::LBRACE)) {
        body = blockStatement();
    } else {
        body = statement();
    }
    
    return makeRepeatStmt(line, column, length, varToken.filename, varName, startValue, endValue, stepValue, body);
}

StmtPtr Parser::getStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    
    std::string keyName;
    std::string valName;
    std::string filename;
    
    if (match(TokenType::LBRACKET)) {
        Token keyToken = consume(TokenType::IDENTIFIER, "Expected key variable name after '['");
        keyName = keyToken.lexeme;
        filename = keyToken.filename;
        consume(TokenType::COMMA, "Expected ',' between key and value variables");
        Token valToken = consume(TokenType::IDENTIFIER, "Expected value variable name");
        valName = valToken.lexeme;
        consume(TokenType::RBRACKET, "Expected ']' after value variable");
    } else {
        Token varToken = consume(TokenType::IDENTIFIER, "Expected variable name after 'get'");
        keyName = varToken.lexeme;
        filename = varToken.filename;
    }
    
    consume(TokenType::IN, "Expected 'in' after variable name");
    
    ExprPtr iterable = expression();
    
    skipNewlines();
    
    StmtPtr body;
    if (match(TokenType::LBRACE)) {
        body = blockStatement();
    } else {
        body = statement();
    }
    
    if (!valName.empty()) {
        return makeGetKVStmt(line, column, length, filename, keyName, valName, iterable, body);
    }
    return makeGetStmt(line, column, length, filename, keyName, iterable, body);
}

StmtPtr Parser::matchStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    std::string filename = previous().filename;
    
    ExprPtr subject = expression();
    skipNewlines();
    
    consume(TokenType::LBRACE, "Expected '{' before match arms");
    skipNewlines();
    
    std::vector<MatchArm> arms;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        MatchArm arm;
        if (match(TokenType::OTHER)) {
            arm.pattern = nullptr;
        } else {
            arm.pattern = expression();
        }
        
        consume(TokenType::ARROW, "Expected '=>' after match pattern");
        
        if (match(TokenType::LBRACE)) {
            arm.body = blockStatement();
        } else {
            arm.body = statement();
        }
        
        arms.push_back(arm);
        skipNewlines();
    }
    
    consume(TokenType::RBRACE, "Expected '}' after match arms");
    
    return makeMatchStmt(line, column, length, filename, subject, std::move(arms));
}

StmtPtr Parser::taskStatement(bool isAsync) {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    
    // Allow any token as function name (for operator overloading)
    advance();
    Token nameToken = previous();
    std::string name = nameToken.lexeme;
    
    consume(TokenType::LPAREN, "Expected '(' after function name");
    
    std::vector<std::string> params;
    std::vector<TypeASTPtr> paramTypes;
    std::vector<ExprPtr> defaultValues;
    bool hadDefault = false;
    bool isVariadic = false;
    
    if (!check(TokenType::RPAREN)) {
        do {
            if (match(TokenType::ELLIPSIS)) {
                isVariadic = true;
                Token paramToken = consume(TokenType::IDENTIFIER, "Expected parameter name after '...'");
                params.push_back(paramToken.lexeme);
            
            TypeASTPtr pType = nullptr;
            if (match(TokenType::COLON)) {
                pType = parseType();
            } else {
                pType = std::make_shared<TypeAST>("Any");
            }
            paramTypes.push_back(pType);
                paramTypes.push_back(std::make_shared<TypeAST>("Any"));
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
            
            TypeASTPtr pType = nullptr;
            if (match(TokenType::COLON)) {
                pType = parseType();
            } else {
                pType = std::make_shared<TypeAST>("Any");
            }
            paramTypes.push_back(pType);
            
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
    
    TypeASTPtr returnType = nullptr;
    if (match(TokenType::ARROW)) {
        returnType = parseType();
    } else {
        returnType = std::make_shared<TypeAST>("Any");
    }
    skipNewlines();
    
    // Parse Design-by-Contract clauses (requires / ensures)
    std::vector<std::pair<ExprPtr, std::string>> requiresClauses;
    std::vector<std::pair<ExprPtr, std::string>> ensuresClauses;
    
    while (check(TokenType::REQUIRES) || check(TokenType::ENSURES)) {
        bool isRequires = match(TokenType::REQUIRES);
        if (!isRequires) match(TokenType::ENSURES);
        
        // Parse one or more comma-separated condition/message pairs
        do {
            skipNewlines();
            ExprPtr condition = expression();
            std::string message;
            if (match(TokenType::COMMA)) {
                // Check if the next thing is a string literal (the message)
                // vs. another condition (for the next clause)
                // If it's a string literal, consume as the message
                if (check(TokenType::STRING)) {
                    advance();
                    message = std::get<std::string>(previous().literal);
                } else {
                    // Not a string — this comma starts another clause
                    // We'll push what we have and let the outer loop handle it
                    if (isRequires) requiresClauses.push_back({condition, message});
                    else ensuresClauses.push_back({condition, message});
                    skipNewlines();
                    // Re-check for another requires/ensures on same line
                    if (!check(TokenType::REQUIRES) && !check(TokenType::ENSURES)) {
                        // Another condition on the same keyword line
                        continue;
                    }
                    break;
                }
            }
            if (isRequires) requiresClauses.push_back({condition, message});
            else ensuresClauses.push_back({condition, message});
        } while (false); // single pair per line; multiple lines handled by outer while
        
        skipNewlines();
    }
    
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
    
    auto taskStmt = makeTaskStmt(line, column, length, nameToken.filename, name, params, paramTypes, defaultValues, returnType, body, isVariadic, isAsync);
    // Attach contract clauses
    auto& task = *std::get<std::shared_ptr<TaskStmt>>(taskStmt->variant);
    task.requiresClauses = std::move(requiresClauses);
    task.ensuresClauses  = std::move(ensuresClauses);
    return taskStmt;
}

StmtPtr Parser::giveStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    std::string filename = previous().filename;
    
    ExprPtr value = nullptr;
    if (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE) && !check(TokenType::RBRACE)) {
        value = expression();

        // Multi-value give: give expr1, expr2, ...
        // Wrap all values in a single array expression so the caller can destructure.
        if (check(TokenType::COMMA)) {
            std::vector<ExprPtr> elems;
            elems.push_back(value);
            while (match(TokenType::COMMA)) {
                elems.push_back(expression());
            }
            value = makeArrayExpr(line, column, length, filename, std::move(elems));
        } else {
            // TCO: Mark tail calls if the expression is a direct call
            if (value) {
                if (std::holds_alternative<std::shared_ptr<CallExpr>>(value->variant)) {
                    std::get<std::shared_ptr<CallExpr>>(value->variant)->isTailCall = true;
                }
            }
        }
    }
    
    return makeGiveStmt(line, column, length, filename, value);
}

StmtPtr Parser::escapeStatement() {
    Token op = previous();
    return makeEscapeStmt(op.line, op.column, op.lexeme.length(), op.filename);
}

StmtPtr Parser::skipStatement() {
    Token op = previous();
    return makeSkipStmt(op.line, op.column, op.lexeme.length(), op.filename);
}

StmtPtr Parser::staticStatement() {
    Token op = previous();
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected variable name after 'static'");
    consume(TokenType::EQUAL, "Expected '=' after static variable name");
    ExprPtr value = expression();
    return makeStaticStmt(op.line, op.column, op.lexeme.length(), op.filename, nameToken.lexeme, value);
}

StmtPtr Parser::tryStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    std::string filename = previous().filename;
    
    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' after 'try'");
    StmtPtr tryBlock = blockStatement();
    
    std::vector<CatchBlock> catchBlocks;
    
    skipNewlines();
    while (match(TokenType::CATCH)) {
        std::string typeName = "";
        std::string varName = "";
        
        // Allow 'catch (Type var)' or 'catch var'
        if (match(TokenType::LPAREN)) {
            Token t = consume(TokenType::IDENTIFIER, "Expected type name or variable name");
            if (check(TokenType::IDENTIFIER)) {
                typeName = t.lexeme;
                varName = advance().lexeme;
            } else {
                varName = t.lexeme;
            }
            consume(TokenType::RPAREN, "Expected ')' after catch configuration");
        } else {
            Token t = consume(TokenType::IDENTIFIER, "Expected variable name after 'catch'");
            varName = t.lexeme;
        }
        
        skipNewlines();
        consume(TokenType::LBRACE, "Expected '{' after catch");
        StmtPtr catchBody = blockStatement();
        catchBlocks.push_back({typeName, varName, catchBody});
        skipNewlines();
    }
    
    if (catchBlocks.empty()) {
        error(peek(), "Expected at least one 'catch' block");
    }
    
    return makeTryStmt(line, column, length, filename, tryBlock, std::move(catchBlocks));
}

StmtPtr Parser::throwStatement() {
    Token op = previous();
    ExprPtr expr = expression();
    return makeThrowStmt(op.line, op.column, op.lexeme.length(), op.filename, expr);
}

StmtPtr Parser::blockStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    std::vector<StmtPtr> statements;
    
    skipNewlines();
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        auto stmt = declaration();
        if (stmt) statements.push_back(stmt);
        skipNewlines();
    }
    
    consume(TokenType::RBRACE, "Expected '}' after block");
    
    return makeBlockStmt(line, column, length, peek().filename, statements);
}

StmtPtr Parser::expressionStatement() {
    int line = peek().line;
    int column = peek().column;
    int length = peek().lexeme.length();
    ExprPtr expr = expression();
    
    // Check if this is a variable declaration (assignment to new variable)
    // Only create VarDeclStmt if we're in a declaration context (e.g., after 'let')
    // Otherwise, treat as regular assignment statement
    return makeExprStmt(line, column, length, peek().filename, expr);
}

StmtPtr Parser::structStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected struct name");
    std::string name = nameToken.lexeme;
    
    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' before struct body");
    
    std::vector<std::string> fields;
    std::vector<TypeASTPtr> types;
    std::vector<ExprPtr> defaults;
    skipNewlines();
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token field = consume(TokenType::IDENTIFIER, "Expected field name");
        fields.push_back(field.lexeme);
        
        TypeASTPtr typeHint = nullptr;
        if (match(TokenType::COLON)) {
            typeHint = parseType();
        } else {
            typeHint = std::make_shared<TypeAST>("Any");
        }
        types.push_back(typeHint);
        
        ExprPtr defVal = nullptr;
        if (match(TokenType::EQUAL)) {
            defVal = expression();
        }
        defaults.push_back(defVal);
        
        if (match(TokenType::COMMA)) {
            skipNewlines();
        } else if (check(TokenType::NEWLINE)) {
            skipNewlines();
        }
    }
    
    consume(TokenType::RBRACE, "Expected '}' after struct body");
    
    return makeStructStmt(line, column, length, nameToken.filename, name, fields, types, defaults);
}

StmtPtr Parser::useStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    if (!match(TokenType::STRING)) {
        throw ParseError("Expected string path after 'use'", peek().line);
    }
    Token pathToken = previous();
    std::string path = std::get<std::string>(pathToken.literal);
    
    std::string alias = "";
    // Support 'use "lib" as alias'
    // Check if next token's lexeme is "as" (since it's not a reserved keyword in some versions)
    if (peek().lexeme == "as") {
        advance(); // Consume "as"
        if (match(TokenType::STAR)) {
            alias = "*";
        } else {
            Token aliasToken = consume(TokenType::IDENTIFIER, "Expected alias name or '*' after 'as'");
            alias = aliasToken.lexeme;
        }
    }
    
    return makeUseStmt(line, column, length, pathToken.filename, path, alias);
}

StmtPtr Parser::exportStatement() {
    Token tok = previous(); // the 'export' token
    skipNewlines();
    
    // Parse the inner declaration
    StmtPtr inner = statement();
    if (!inner) {
        error(tok, "Expected declaration after 'export'");
        return nullptr;
    }
    return makeExportStmt(tok.line, tok.column, (int)tok.lexeme.length(), tok.filename, std::move(inner));
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
            value = makeBinaryExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, binOp, value);
        }
        
        if (std::holds_alternative<std::shared_ptr<IdentifierExpr>>(expr->variant)) {
            std::string name = std::get<std::shared_ptr<IdentifierExpr>>(expr->variant)->name;
            return makeAssignExpr(op.line, op.column, op.lexeme.length(), op.filename, name, value);
        } else if (std::holds_alternative<std::shared_ptr<IndexExpr>>(expr->variant)) {
            auto indexExpr = std::get<std::shared_ptr<IndexExpr>>(expr->variant);
            // Handle obj[idx] = val where obj can be complex
            return makeAssignExpr(op.line, op.column, op.lexeme.length(), op.filename, "", value, indexExpr->index, indexExpr->object);
        } else if (std::holds_alternative<std::shared_ptr<PropertyAccessExpr>>(expr->variant)) {
            auto propExpr = std::get<std::shared_ptr<PropertyAccessExpr>>(expr->variant);
            return makeSetExpr(op.line, op.column, op.lexeme.length(), op.filename, propExpr->object, propExpr->property, value);
        }
        
        error(op, "Invalid assignment target");
    }
    
    return expr;
}

ExprPtr Parser::ternary() {
    ExprPtr expr = nullCoalescing();
    
    if (match(TokenType::QUESTION_MARK)) {
        Token op = previous();
        ExprPtr thenBranch = expression();
        consume(TokenType::COLON, "Expected ':' after then branch of ternary operator");
        ExprPtr elseBranch = ternary(); // Right-associative
        expr = makeTernaryExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, thenBranch, elseBranch);
    }
    
    return expr;
}

ExprPtr Parser::nullCoalescing() {
    ExprPtr expr = logicalOr();
    
    while (match(TokenType::QUESTION_QUESTION)) {
        Token op = previous();
        ExprPtr right = logicalOr();
        // Null coalescing acts like logical OR but strictly checks for nil
        expr = makeLogicalExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, TokenType::QUESTION_QUESTION, right);
    }
    
    return expr;
}

ExprPtr Parser::logicalOr() {
    ExprPtr expr = logicalAnd();
    
    while (match(TokenType::OR)) {
        Token op = previous();
        ExprPtr right = logicalAnd();
        expr = makeLogicalExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, TokenType::OR, right);
    }
    
    return expr;
}

ExprPtr Parser::logicalAnd() {
    ExprPtr expr = bitwiseOr();
    
    while (match(TokenType::AND)) {
        Token op = previous();
        ExprPtr right = bitwiseOr();
        expr = makeLogicalExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, TokenType::AND, right);
    }
    
    return expr;
}

ExprPtr Parser::bitwiseOr() {
    ExprPtr expr = bitwiseXor();
    while (match(TokenType::PIPE)) {
        Token op = previous();
        ExprPtr right = bitwiseXor();
        expr = makeBinaryExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    return expr;
}

ExprPtr Parser::bitwiseXor() {
    ExprPtr expr = bitwiseAnd();
    while (match(TokenType::CARET)) {
        Token op = previous();
        ExprPtr right = bitwiseAnd();
        expr = makeBinaryExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    return expr;
}

ExprPtr Parser::bitwiseAnd() {
    ExprPtr expr = equality();
    while (match(TokenType::AMPERSAND)) {
        Token op = previous();
        ExprPtr right = equality();
        expr = makeBinaryExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    return expr;
}

ExprPtr Parser::equality() {
    ExprPtr expr = comparison();
    
    while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL})) {
        Token op = previous();
        ExprPtr right = comparison();
        expr = makeBinaryExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::comparison() {
    ExprPtr expr = bitwiseShift();
    
    while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, 
                  TokenType::LESS, TokenType::LESS_EQUAL, TokenType::IN})) {
        Token op = previous();
        ExprPtr right = bitwiseShift();
        expr = makeBinaryExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::bitwiseShift() {
    ExprPtr expr = term();
    
    while (match({TokenType::LSHIFT, TokenType::RSHIFT})) {
        Token op = previous();
        ExprPtr right = term();
        expr = makeBinaryExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::term() {
    ExprPtr expr = factor();
    
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        Token op = previous();
        ExprPtr right = factor();
        expr = makeBinaryExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::factor() {
    ExprPtr expr = unary();
    
    while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        Token op = previous();
        ExprPtr right = unary();
        expr = makeBinaryExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::unary() {
    if (match({TokenType::BANG, TokenType::MINUS, TokenType::NOT, TokenType::TILDE})) {
        Token op = previous();
        ExprPtr right = unary();
        return makeUnaryExpr(op.line, op.column, op.lexeme.length(), op.filename, op.type, right);
    }
    
    if (match(TokenType::AWAIT)) {
        Token op = previous();
        ExprPtr right = unary();
        return std::make_shared<Expr>(op.line, op.column, op.lexeme.length(), op.filename, std::make_shared<AwaitExpr>(right));
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
            expr = makeIndexExpr(op.line, op.column, op.lexeme.length(), op.filename, expr, index);
        } else if (match(TokenType::DOT) || match(TokenType::QUESTION_DOT)) {
            bool isOptional = previous().type == TokenType::QUESTION_DOT;
            // Allow keywords as property names
            advance();
            Token name = previous();
            expr = makePropertyAccessExpr(name.line, name.column, name.lexeme.length(), name.filename, expr, name.lexeme, isOptional);
        } else {
            // Check if next non-newline token is a DOT (Multi-line chaining support)
            size_t temp = current;
            while (temp < tokens.size() && tokens[temp].type == TokenType::NEWLINE) {
                temp++;
            }
            if (temp < tokens.size() && (tokens[temp].type == TokenType::DOT || tokens[temp].type == TokenType::QUESTION_DOT)) {
                current = temp; // Skip newlines
                bool isOptional = tokens[current].type == TokenType::QUESTION_DOT;
                advance(); // Consume DOT or QUESTION_DOT
                
                // Now must have an identifier (or keyword)
                if (isAtEnd()) throw ParseError("Expected property name after property access operator", peek().line);
                advance(); 
                Token name = previous();
                expr = makePropertyAccessExpr(name.line, name.column, name.lexeme.length(), name.filename, expr, name.lexeme, isOptional);
            } else {
                break;
            }
        }
    }
    
    return expr;
}

ExprPtr Parser::finishCall(ExprPtr callee) {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    std::vector<ExprPtr> arguments;
    
    if (!check(TokenType::RPAREN)) {
        do {
            if (match(TokenType::ELLIPSIS)) {
                arguments.push_back(makeSpreadExpr(line, column, length, peek().filename, expression()));
            } else {
                arguments.push_back(expression());
            }
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::RPAREN, "Expected ')' after arguments");
    
    return makeCallExpr(line, column, length, peek().filename, callee, arguments);
}

ExprPtr Parser::primary() {
    int line = peek().line;
    int column = peek().column;
    int length = peek().lexeme.length();
    std::string filename = peek().filename;
    
    if (match(TokenType::FALSE)) return makeLiteralExpr(line, column, length, filename, false);
    if (match(TokenType::TRUE)) return makeLiteralExpr(line, column, length, filename, true);
    if (match(TokenType::NIL)) return makeLiteralExpr(line, column, length, filename, nullptr);
    
    if (match(TokenType::NEW)) {
        Token t = previous();
        Token nameToken = consume(TokenType::IDENTIFIER, "Expected model name after 'new'");
        std::vector<ExprPtr> args;
        if (match(TokenType::LPAREN)) {
            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(expression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "Expected ')' after arguments");
        }
        return makeNewExpr(t.line, t.column, t.lexeme.length(), t.filename, nameToken.lexeme, args);
    }
    
    if (match(TokenType::NUMBER)) {
        Token t = previous();
        if (std::holds_alternative<long long>(t.literal)) {
            return makeLiteralExpr(t.line, t.column, t.lexeme.length(), t.filename, std::get<long long>(t.literal));
        }
        return makeLiteralExpr(t.line, t.column, t.lexeme.length(), t.filename, std::get<double>(t.literal));
    }
    
    if (match(TokenType::STRING)) {
        Token t = previous();
        return makeLiteralExpr(t.line, t.column, t.lexeme.length(), t.filename, std::get<std::string>(t.literal));
    }
    
    if (match(TokenType::IDENTIFIER)) {
        Token t = previous();
        return makeIdentifierExpr(t.line, t.column, t.lexeme.length(), t.filename, t.lexeme);
    }
    
    if (match(TokenType::SUPER)) {
        Token t = previous();
        return makeIdentifierExpr(t.line, t.column, t.lexeme.length(), t.filename, "super");
    }
    
    // Self reference
    if (match(TokenType::SELF)) {
        Token t = previous();
        return makeSelfExpr(t.line, t.column, t.lexeme.length(), t.filename);
    }
    
    if (match(TokenType::IN)) {
        Token t = previous();
        // Special case for 'in' keyword used alone (not as operator)
        // Historically mapped to __input__
        return makeCallExpr(t.line, t.column, t.lexeme.length(), t.filename, makeIdentifierExpr(t.line, t.column, t.lexeme.length(), t.filename, "__input__"), {});
    }
    
    // Lambda expression: |params| => expr or |params| { body }
    if (match(TokenType::PIPE)) {
        return lambdaExpression(false);
    }
    
    // Async Lambda expression: async |params| => expr
    if (match(TokenType::ASYNC)) {
        if (match(TokenType::PIPE)) {
            return lambdaExpression(true);
        }
        throw ParseError("Expected '|' for lambda after 'async'", previous().line);
    }
    
    if (match(TokenType::LBRACKET)) {
        // Array literal
        std::vector<ExprPtr> elements;
        
        if (!check(TokenType::RBRACKET)) {
            do {
                skipNewlines();
                if (match(TokenType::ELLIPSIS)) {
                    elements.push_back(makeSpreadExpr(line, column, length, filename, expression()));
                } else {
                    elements.push_back(expression());
                }
                skipNewlines();
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RBRACKET, "Expected ']' after array elements");
        return makeArrayExpr(line, column, length, filename, elements);
    }
    
    // Dictionary literal
    if (match(TokenType::LBRACE)) {
        int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
        std::vector<std::pair<ExprPtr, ExprPtr>> pairs;
        
        skipNewlines();
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            ExprPtr expr = expression();
            
            if (std::holds_alternative<std::shared_ptr<AssignExpr>>(expr->variant)) {
                 auto assign = std::get<std::shared_ptr<AssignExpr>>(expr->variant);
                 ExprPtr key = makeLiteralExpr(expr->line, expr->column, expr->length, expr->filename, assign->name);
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
                      key = makeLiteralExpr(key->line, key->column, key->length, key->filename, ident->name);
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
        return makeDictionaryExpr(line, column, length, filename, pairs);
    }
    
    if (match(TokenType::LPAREN)) {
        ExprPtr expr = expression();
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    throw ParseError("Expected expression", line);
}

// Lambda: |x, y| => x + y  OR  |x, y| { statements }
ExprPtr Parser::lambdaExpression(bool isAsync) {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    
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
        return makeLambdaExpr(line, column, length, peek().filename, params, std::vector<TypeASTPtr>(params.size(), std::make_shared<TypeAST>("Any")), body, nullptr, isVariadic, isAsync);
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
        return makeLambdaExpr(line, column, length, peek().filename, params, std::vector<TypeASTPtr>(params.size(), std::make_shared<TypeAST>("Any")), stmtBody, nullptr, isVariadic, isAsync);
    } else {
        // Default: treat as expression body without arrow
        ExprPtr body = expression();
        return makeLambdaExpr(line, column, length, peek().filename, params, std::vector<TypeASTPtr>(params.size(), std::make_shared<TypeAST>("Any")), body, nullptr, isVariadic, isAsync);
    }
}

// Model (class) definition
// Syntax: model Name extends Parent { init(params) { body } shown/hidden members and methods }
StmtPtr Parser::modelStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    
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
    std::vector<TypeASTPtr> initParamTypes;
    std::vector<ExprPtr> initDefaultValues;
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
                bool hadDefault = false;
                do {
                    Token paramToken = consume(TokenType::IDENTIFIER, "Expected parameter name");
                    initParams.push_back(paramToken.lexeme);
                    
                    if (match(TokenType::COLON)) {
                        initParamTypes.push_back(parseType());
                    } else {
                        initParamTypes.push_back(std::make_shared<TypeAST>("Any"));
                    }
                    
                    if (match(TokenType::EQUAL)) {
                        initDefaultValues.push_back(expression());
                        hadDefault = true;
                    } else {
                        if (hadDefault) error(paramToken, "Required parameter cannot follow optional parameter");
                        initDefaultValues.push_back(nullptr);
                    }
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
        } else {
            // Check for async task (method)
            bool isAsync = false;
            if (match(TokenType::ASYNC)) {
                isAsync = true;
            }

            // Check for @cached on a method
            bool methodCached = false;
            if (check(TokenType::DECORATOR_CACHED)) {
                advance();
                methodCached = true;
                skipNewlines();
            }

        // Check for task (method)
        if (match(TokenType::TASK)) {
            // Allow keywords as method names
            advance();
            Token methodName = previous();
            consume(TokenType::LPAREN, "Expected '(' after method name");
            
            std::vector<std::string> params;
            std::vector<TypeASTPtr> paramTypes;
            std::vector<ExprPtr> defaultValues;
            if (!check(TokenType::RPAREN)) {
                bool hadDefault = false;
                do {
                    Token paramToken = advance();
                    if (paramToken.type == TokenType::RPAREN || paramToken.type == TokenType::COMMA) {
                        throw ParseError("Expected parameter name", paramToken.line);
                    }
                    params.push_back(paramToken.lexeme);
                    
                    if (match(TokenType::COLON)) {
                        paramTypes.push_back(parseType());
                    } else {
                        paramTypes.push_back(std::make_shared<TypeAST>("Any"));
                    }
                    
                    if (match(TokenType::EQUAL)) {
                        defaultValues.push_back(expression());
                        hadDefault = true;
                    } else {
                        if (hadDefault) error(paramToken, "Required parameter cannot follow optional parameter");
                        defaultValues.push_back(nullptr);
                    }
                } while (match(TokenType::COMMA));
            }
            
            consume(TokenType::RPAREN, "Expected ')' after method parameters");
            skipNewlines();
            
            TypeASTPtr returnType = nullptr;
            if (match(TokenType::ARROW)) {
                returnType = parseType();
            } else {
                returnType = std::make_shared<TypeAST>("Any");
            }
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
            member.isAsync = isAsync;
            member.isCached = methodCached;
            member.name = methodName.lexeme;
            member.params = params;
            member.paramTypes = paramTypes;
            member.typeHint = returnType;
            member.defaultValues = defaultValues;
            member.body = body;
            members.push_back(member);
        } else {
            // Property declaration
            if (isAsync) {
                error(peek(), "Expected 'task' after 'async' modifier");
                advance(); // Avoid infinite loop
            }
            else if (check(TokenType::IDENTIFIER)) {
                Token propName = advance();
                
                TypeASTPtr typeHint = nullptr;
                if (match(TokenType::COLON)) {
                    typeHint = parseType();
                }
                
                ExprPtr initializer = nullptr;
                if (match(TokenType::EQUAL)) {
                    initializer = expression();
                }
                
                skipNewlines();
                
                // Parse @validate decorators following the property
                std::vector<ValidateRule> propValidators;
                while (check(TokenType::DECORATOR_VALIDATE)) {
                    advance(); // consume @validate
                    consume(TokenType::LPAREN, "Expected '(' after @validate");
                    std::string rule;
                    ExprPtr param = nullptr;
                    std::string msg;
                    // rule name
                    Token ruleTok = consume(TokenType::STRING, "Expected rule name string in @validate");
                    rule = std::get<std::string>(ruleTok.literal);
                    if (match(TokenType::COMMA)) {
                        // could be param (number/string) or message string
                        if (check(TokenType::STRING)) {
                            Token maybeMsg = peek();
                            advance();
                            // if another comma follows, this was the param, next will be message
                            if (match(TokenType::COMMA)) {
                                // param is string literal
                                auto strLit = std::make_shared<LiteralExpr>(std::get<std::string>(maybeMsg.literal));
                                param = std::make_shared<Expr>(maybeMsg.line, maybeMsg.column, 1, maybeMsg.filename, strLit);
                                Token msgTok = consume(TokenType::STRING, "Expected message string in @validate");
                                msg = std::get<std::string>(msgTok.literal);
                            } else {
                                msg = std::get<std::string>(maybeMsg.literal);
                            }
                        } else {
                            // numeric param
                            param = expression();
                            if (match(TokenType::COMMA)) {
                                Token msgTok = consume(TokenType::STRING, "Expected message string in @validate");
                                msg = std::get<std::string>(msgTok.literal);
                            }
                        }
                    }
                    consume(TokenType::RPAREN, "Expected ')' after @validate arguments");
                    ValidateRule vr;
                    vr.ruleName = rule;
                    vr.param    = param;
                    vr.message  = msg.empty() ? ("Validation failed: " + rule) : msg;
                    propValidators.push_back(std::move(vr));
                    skipNewlines();
                }
                
                ModelMember member;
                member.visibility = visibility;
                member.isStatic = isStatic;
                member.isMethod = false;
                member.name = propName.lexeme;
                member.typeHint = typeHint;
                member.initializer = initializer;
                member.validators = std::move(propValidators);
                members.push_back(member);
            } else {
                error(peek(), "Unexpected token in model body");
                advance(); // Avoid infinite loop
            }
        }
        }
        
        skipNewlines();
    }
    
    consume(TokenType::RBRACE, "Expected '}' after model body");
    
    // Use the peek().filename or current filename
    return makeModelStmt(line, column, length, nameToken.filename, name, parentName, interfaces, initParams, initParamTypes,
                         initDefaultValues, initBody, members);
}

// Interface definition
// Syntax: interface Name { task method1() task method2() ... }
StmtPtr Parser::interfaceStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected interface name");
    
    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' after interface name");
    skipNewlines();
    
    std::vector<InterfaceMethod> methods;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        consume(TokenType::TASK, "Expected 'task' in interface definition");
        Token methodToken = consume(TokenType::IDENTIFIER, "Expected method name");
        consume(TokenType::LPAREN, "Expected '(' after method name");
        
        InterfaceMethod method;
        method.name = methodToken.lexeme;
        
        if (!check(TokenType::RPAREN)) {
            do {
                Token paramToken = consume(TokenType::IDENTIFIER, "Expected parameter name");
                method.params.push_back(paramToken.lexeme);
                
                if (match(TokenType::COLON)) {
                    method.paramTypes.push_back(parseType());
                } else {
                    method.paramTypes.push_back(std::make_shared<TypeAST>("Any"));
                }
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "Expected ')' after method parameters");
        
        if (match(TokenType::ARROW)) {
            method.returnType = parseType();
        } else {
            method.returnType = std::make_shared<TypeAST>("Any");
        }
        
        methods.push_back(method);
        skipNewlines();
    }
    consume(TokenType::RBRACE, "Expected '}' after interface body");
    
    return makeInterfaceStmt(line, column, length, nameToken.filename, nameToken.lexeme, methods);
}
