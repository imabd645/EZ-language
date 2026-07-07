#include "Parser.h"
#include <iostream>
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
    std::vector<std::string> userDecorators;
    std::string persistPath;
    std::shared_ptr<RateLimitConfig> rateLimitCfg = nullptr;
    while (check(TokenType::DECORATOR_AUDITED)  || check(TokenType::DECORATOR_SNAPSHOT) ||
           check(TokenType::DECORATOR_CACHED)   || check(TokenType::DECORATOR_PERSIST)  ||
           check(TokenType::DECORATOR_VALIDATE) || check(TokenType::DECORATOR_RATELIMIT) ||
           check(TokenType::DECORATOR_USER)) {
        TokenType kind = peek().type;
        std::string lexName = peek().lexeme;
        if (!lexName.empty() && lexName[0] == '@') {
            lexName = lexName.substr(1);
        }
        advance(); // consume decorator token
        if (kind == TokenType::DECORATOR_USER) {
            userDecorators.push_back(lexName);
        } else if (kind == TokenType::DECORATOR_PERSIST) {
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
            rateLimitCfg = std::make_shared<RateLimitConfig>(RateLimitConfig{count, perStr, nullptr});
            consume(TokenType::RPAREN, "Expected ')' after @ratelimit arguments");
        }
        decorators.push_back(kind);
        skipNewlines();
    }

    bool isAsync = false;
    if (match(TokenType::ASYNC)) {
        isAsync = true;
    }
    
    if (match(TokenType::TASK) || match(TokenType::DECORATOR_KW)) {
        auto taskNode = taskStatement(isAsync);
        // Apply task-level decorators
        auto& task = *std::get<std::shared_ptr<TaskStmt>>(taskNode->variant);
        task.userDecorators = userDecorators;
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
        model.userDecorators = userDecorators;
        for (auto kind : decorators) {
            if (kind == TokenType::DECORATOR_AUDITED)  model.audited  = true;
            if (kind == TokenType::DECORATOR_SNAPSHOT) model.snapshot = true;
            if (kind == TokenType::DECORATOR_PERSIST)  model.persistPath = persistPath;
        }
        return modelNode;
    }
    if (!decorators.empty() || !userDecorators.empty()) {
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
    
    std::vector<std::string> typeParams;
    if (match(TokenType::LBRACKET)) {
        do {
            Token paramToken = consume(TokenType::IDENTIFIER, "Expected generic type parameter name");
            typeParams.push_back(paramToken.lexeme);
        } while (match(TokenType::COMMA));
        consume(TokenType::RBRACKET, "Expected ']' after generic type parameters");
    }
    
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
                ExprPtr defaultVal = ternary();
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
    std::get<std::shared_ptr<TaskStmt>>(taskStmt->variant)->typeParams = typeParams;
    
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
        // Allow 'catch (Type var)' or 'catch (Type)' or 'catch (var)'
        if (match(TokenType::LPAREN)) {
            Token t = consume(TokenType::IDENTIFIER, "Expected type name or variable name");
            if (check(TokenType::IDENTIFIER)) {
                typeName = t.lexeme;
                varName = advance().lexeme;
            } else {
                if (t.lexeme.length() > 0 && t.lexeme[0] >= 'A' && t.lexeme[0] <= 'Z') {
                    typeName = t.lexeme;
                } else {
                    varName = t.lexeme;
                }
            }
            consume(TokenType::RPAREN, "Expected ')' after catch configuration");
        } else {
            // Allow 'catch Type var' or 'catch Type' or 'catch var'
            Token t = consume(TokenType::IDENTIFIER, "Expected type or variable name after 'catch'");
            if (check(TokenType::IDENTIFIER)) {
                typeName = t.lexeme;
                varName = advance().lexeme;
            } else {
                if (t.lexeme.length() > 0 && t.lexeme[0] >= 'A' && t.lexeme[0] <= 'Z') {
                    typeName = t.lexeme;
                } else {
                    varName = t.lexeme;
                }
            }
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
    return makeExpressionStmt(line, column, length, peek().filename, expr);
}

StmtPtr Parser::structStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected struct name");
    std::string name = nameToken.lexeme;
    
    std::vector<std::string> typeParams;
    if (match(TokenType::LBRACKET)) {
        do {
            Token paramToken = consume(TokenType::IDENTIFIER, "Expected generic type parameter name");
            typeParams.push_back(paramToken.lexeme);
        } while (match(TokenType::COMMA));
        consume(TokenType::RBRACKET, "Expected ']' after generic type parameters");
    }
    
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
    
    auto stmt = makeStructStmt(line, column, length, nameToken.filename, name, fields, types, defaults);
    std::get<std::shared_ptr<StructStmt>>(stmt->variant)->typeParams = typeParams;
    return stmt;
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

StmtPtr Parser::modelStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected model name");
    std::string name = nameToken.lexeme;
    
    std::vector<std::string> typeParams;
    if (match(TokenType::LBRACKET)) {
        do {
            Token paramToken = consume(TokenType::IDENTIFIER, "Expected generic type parameter name");
            typeParams.push_back(paramToken.lexeme);
        } while (match(TokenType::COMMA));
        consume(TokenType::RBRACKET, "Expected ']' after generic type parameters");
    }
    
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
                        initDefaultValues.push_back(ternary());
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
            
            std::vector<std::string> typeParams;
            if (match(TokenType::LBRACKET)) {
                do {
                    Token paramToken = consume(TokenType::IDENTIFIER, "Expected generic type parameter name");
                    typeParams.push_back(paramToken.lexeme);
                } while (match(TokenType::COMMA));
                consume(TokenType::RBRACKET, "Expected ']' after generic type parameters");
            }
            
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
                        defaultValues.push_back(ternary());
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
            member.typeParams = typeParams;
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
    auto stmt = makeModelStmt(line, column, length, nameToken.filename, name, parentName, interfaces, initParams, initParamTypes,
                         initDefaultValues, initBody, members);
    std::get<std::shared_ptr<ModelStmt>>(stmt->variant)->typeParams = typeParams;
    return stmt;
}

// Interface definition
// Syntax: interface Name { task method1() task method2() ... }
StmtPtr Parser::interfaceStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected interface name");
    
    std::vector<std::string> typeParams;
    if (match(TokenType::LBRACKET)) {
        do {
            Token paramToken = consume(TokenType::IDENTIFIER, "Expected generic type parameter name");
            typeParams.push_back(paramToken.lexeme);
        } while (match(TokenType::COMMA));
        consume(TokenType::RBRACKET, "Expected ']' after generic type parameters");
    }
    
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
    
    auto stmt = makeInterfaceStmt(line, column, length, nameToken.filename, nameToken.lexeme, methods);
    std::get<std::shared_ptr<InterfaceStmt>>(stmt->variant)->typeParams = typeParams;
    return stmt;
}
