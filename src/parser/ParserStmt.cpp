#include "Parser.h"
#include <iostream>
#include <unordered_set>
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
    
    return arena.allocate<TypeAST>(baseType, typeArgs);
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
    
    return makeVarDeclStmt(arena, line, column, length, filename, nameToken.lexeme, initializer, typeHint);
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
    std::vector<ExprPtr> userDecorators;
    std::string persistPath;
    RateLimitConfig* rateLimitCfg = nullptr;
    bool isCached = false;
    bool isAudited = false;
    bool isSnapshot = false;

    while (match(TokenType::AT)) {
        ExprPtr decExpr = expression();
        bool isBuiltin = false;
        
        if (std::holds_alternative<IdentifierExpr*>(decExpr->variant)) {
            auto varExpr = std::get<IdentifierExpr*>(decExpr->variant);
            std::string name = varExpr->name;
            if (name == "cached") { isCached = true; isBuiltin = true; }
            else if (name == "audited") { isAudited = true; isBuiltin = true; }
            else if (name == "snapshot") { isSnapshot = true; isBuiltin = true; }
        } else if (std::holds_alternative<CallExpr*>(decExpr->variant)) {
            auto callExpr = std::get<CallExpr*>(decExpr->variant);
            if (std::holds_alternative<IdentifierExpr*>(callExpr->callee->variant)) {
                auto varExpr = std::get<IdentifierExpr*>(callExpr->callee->variant);
                std::string name = varExpr->name;
                if (name == "persist") {
                    if (callExpr->arguments.size() == 1) {
                        if (std::holds_alternative<LiteralExpr*>(callExpr->arguments[0]->variant)) {
                            auto strExpr = std::get<LiteralExpr*>(callExpr->arguments[0]->variant);
                            if (std::holds_alternative<std::string>(strExpr->value)) {
                                persistPath = std::get<std::string>(strExpr->value);
                                isBuiltin = true;
                            }
                        }
                    }
                } else if (name == "ratelimit") {
                    if (callExpr->arguments.size() >= 2) {
                        int count = 0;
                        if (std::holds_alternative<LiteralExpr*>(callExpr->arguments[0]->variant)) {
                            auto numExpr = std::get<LiteralExpr*>(callExpr->arguments[0]->variant);
                            if (std::holds_alternative<long long>(numExpr->value)) count = (int)std::get<long long>(numExpr->value);
                            else if (std::holds_alternative<double>(numExpr->value)) count = (int)std::get<double>(numExpr->value);
                        }
                        std::string perStr;
                        if (std::holds_alternative<LiteralExpr*>(callExpr->arguments[1]->variant)) {
                            auto strExpr = std::get<LiteralExpr*>(callExpr->arguments[1]->variant);
                            if (std::holds_alternative<std::string>(strExpr->value)) perStr = std::get<std::string>(strExpr->value);
                        }
                        rateLimitCfg = arena.allocate<RateLimitConfig>(RateLimitConfig{count, perStr, nullptr});
                        isBuiltin = true;
                    }
                }
            }
        }
        
        if (!isBuiltin) {
            userDecorators.push_back(decExpr);
        }
        skipNewlines();
    }

    bool isAsync = false;
    if (match(TokenType::ASYNC)) {
        isAsync = true;
    }
    
    if (match(TokenType::TASK) || match(TokenType::DECORATOR_KW)) {
        auto taskNode = taskStatement(isAsync);
        // Apply task-level decorators
        auto& task = *std::get<TaskStmt*>(taskNode->variant);
        task.userDecorators = userDecorators;
        if (isCached) task.isCached = true;
        if (rateLimitCfg) task.rateLimit = rateLimitCfg;
        return taskNode;
    }
    if (isAsync) {
        throw ParseError("Expected 'task' after 'async' modifier", previous().line);
    }
    
    if (match(TokenType::MODEL)) {
        auto modelNode = modelStatement();
        // Apply model-level decorators
        auto& model = *std::get<ModelStmt*>(modelNode->variant);
        model.userDecorators = userDecorators;
        if (isAudited) model.audited = true;
        if (isSnapshot) model.snapshot = true;
        if (!persistPath.empty()) model.persistPath = persistPath;
        return modelNode;
    }
    if (isCached || isAudited || isSnapshot || !userDecorators.empty()) {
        throw ParseError("Decorators can only be applied to 'model' or 'task' declarations", peek().line);
    }

    if (match(TokenType::GIVE)) return giveStatement();
    if (match(TokenType::ESCAPE)) return escapeStatement();
    if (match(TokenType::SKIP)) return skipStatement();
    if (match(TokenType::LBRACE)) return blockStatement();
    if (match(TokenType::STRUCT)) return structStatement();
    if (match(TokenType::ENUM)) return enumStatement();
    if (match(TokenType::USE)) return useStatement();
    if (match(TokenType::TRY)) return tryStatement();
    if (match(TokenType::THROW)) return throwStatement();
    if (match(TokenType::EXPORT)) return exportStatement();
    
    return expressionStatement();
}

StmtPtr Parser::outStatement() {
    Token op = previous();
    ExprPtr value = expression();
    return makeOutStmt(arena, op.line, op.column, op.lexeme.length(), op.filename, value);
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
            thenBranch = makeBlockStmt(arena, line, column, length, peek().filename, thenStmts);
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
    
    return makeWhenStmt(arena, line, column, length, peek().filename, condition, thenBranch, elseBranch);
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
    
    return makeWhileStmt(arena, line, column, length, peek().filename, condition, body);
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
    
    return makeRepeatStmt(arena, line, column, length, varToken.filename, varName, startValue, endValue, stepValue, body);
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
        return makeGetKVStmt(arena, line, column, length, filename, keyName, valName, iterable, body);
    }
    return makeGetStmt(arena, line, column, length, filename, keyName, iterable, body);
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
    
    return makeMatchStmt(arena, line, column, length, filename, subject, std::move(arms));
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
                pType = arena.allocate<TypeAST>("Any");
            }
            paramTypes.push_back(pType);
            defaultValues.push_back(nullptr);
                if (check(TokenType::COMMA)) {
                    throw ParseError("Rest parameter must be the last parameter", peek().line);
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
                pType = arena.allocate<TypeAST>("Any");
            }
            paramTypes.push_back(pType);
            
            // Check for default value: param = expr
            if (match(TokenType::EQUAL)) {
                ExprPtr defaultVal = ternary();
                defaultValues.push_back(defaultVal);
                hadDefault = true;
            } else {
                if (hadDefault) {
                    throw ParseError("Required parameter cannot follow optional parameter", paramToken.line);
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
        returnType = arena.allocate<TypeAST>("Any");
    }
    skipNewlines();
    
    // Parse Design-by-Contract clauses (requires / ensures)
    std::vector<std::pair<ExprPtr, std::string>> requiresClauses;
    std::vector<std::pair<ExprPtr, std::string>> ensuresClauses;
    
    while (check(TokenType::REQUIRES) || check(TokenType::ENSURES)) {
        bool isRequires = match(TokenType::REQUIRES);
        if (!isRequires) match(TokenType::ENSURES);
        
        // Parse one or more comma-separated condition/message pairs
        while (true) {
            skipNewlines();
            ExprPtr condition = expression();
            std::string message;
            if (match(TokenType::COMMA)) {
                if (check(TokenType::STRING)) {
                    advance();
                    message = std::get<std::string>(previous().literal);
                } else {
                    if (isRequires) requiresClauses.push_back({condition, message});
                    else ensuresClauses.push_back({condition, message});
                    skipNewlines();
                    if (!check(TokenType::REQUIRES) && !check(TokenType::ENSURES) && !check(TokenType::LBRACE)) {
                        continue;
                    }
                    break;
                }
            }
            if (isRequires) requiresClauses.push_back({condition, message});
            else ensuresClauses.push_back({condition, message});
            
            if (match(TokenType::COMMA)) {
                skipNewlines();
                if (!check(TokenType::REQUIRES) && !check(TokenType::ENSURES) && !check(TokenType::LBRACE)) {
                    continue;
                }
            }
            break;
        }
        
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
    
    auto taskStmt = makeTaskStmt(arena, line, column, length, nameToken.filename, name, params, paramTypes, defaultValues, returnType, body, isVariadic, isAsync);
    std::get<TaskStmt*>(taskStmt->variant)->typeParams = typeParams;
    
    // Attach contract clauses
    auto& task = *std::get<TaskStmt*>(taskStmt->variant);
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
            value = makeArrayExpr(arena, line, column, length, filename, std::move(elems));
        } else {
            // TCO: Mark tail calls if the expression is a direct call
            if (value) {
                if (std::holds_alternative<CallExpr*>(value->variant)) {
                    std::get<CallExpr*>(value->variant)->isTailCall = true;
                }
            }
        }
    }
    
    return makeGiveStmt(arena, line, column, length, filename, value);
}

StmtPtr Parser::escapeStatement() {
    Token op = previous();
    return makeEscapeStmt(arena, op.line, op.column, op.lexeme.length(), op.filename);
}

StmtPtr Parser::skipStatement() {
    Token op = previous();
    return makeSkipStmt(arena, op.line, op.column, op.lexeme.length(), op.filename);
}

StmtPtr Parser::staticStatement() {
    Token op = previous();
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected variable name after 'static'");
    consume(TokenType::EQUAL, "Expected '=' after static variable name");
    ExprPtr value = expression();
    return makeStaticStmt(arena, op.line, op.column, op.lexeme.length(), op.filename, nameToken.lexeme, value);
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
    
    // Parse optional finally block
    StmtPtr finallyBlock = nullptr;
    if (match(TokenType::FINALLY)) {
        skipNewlines();
        consume(TokenType::LBRACE, "Expected '{' after 'finally'");
        finallyBlock = blockStatement();
    }
    
    if (catchBlocks.empty() && !finallyBlock) {
        throw ParseError("Expected at least one 'catch' or 'finally' block", peek().line);
    }
    
    return makeTryStmt(arena, line, column, length, filename, tryBlock, std::move(catchBlocks), std::move(finallyBlock));
}

StmtPtr Parser::throwStatement() {
    Token op = previous();
    ExprPtr expr = expression();
    return makeThrowStmt(arena, op.line, op.column, op.lexeme.length(), op.filename, expr);
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
    
    return makeBlockStmt(arena, line, column, length, peek().filename, statements);
}

StmtPtr Parser::expressionStatement() {
    int line = peek().line;
    int column = peek().column;
    int length = peek().lexeme.length();
    ExprPtr expr = expression();
    
    // Check if this is a variable declaration (assignment to new variable)
    // Only create VarDeclStmt if we're in a declaration context (e.g., after 'let')
    // Otherwise, treat as regular assignment statement
    return makeExpressionStmt(arena, line, column, length, peek().filename, expr);
}

// enum Color { RED, GREEN, BLUE }
// enum Status { OK = 200, NOT_FOUND = 404 }
//
// Lowered to a model whose members are statics, so `Color.RED` is an ordinary
// static read and needs no new runtime machinery:
//
//     model Color { static RED = 0  static GREEN = 1  static BLUE = 2 }
//
// Members without an explicit value continue from the previous one, starting at
// 0 -- the rule every C-family enum uses. A member may also carry a string, for
// the common case of an enum whose values are names.
StmtPtr Parser::enumStatement() {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected enum name");
    std::string name = nameToken.lexeme;

    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' before enum body");
    skipNewlines();

    std::vector<ModelMember> members;
    long long nextValue = 0;
    std::unordered_set<std::string> seen;

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token memberTok = consume(TokenType::IDENTIFIER, "Expected enum member name");
        const std::string memberName = memberTok.lexeme;

        // A duplicated name would silently shadow the earlier one, and the
        // reader would have no way to tell which value they were getting.
        if (!seen.insert(memberName).second) {
            throw ParseError("Duplicate enum member '" + memberName + "' in enum '" + name + "'", memberTok.line);
        }

        ExprPtr valueExpr = nullptr;
        if (match(TokenType::EQUAL)) {
            valueExpr = expression();
            // Keep auto-numbering in step with an explicit integer, so
            // `enum E { A = 5, B }` yields B = 6 rather than restarting.
            if (std::holds_alternative<LiteralExpr*>(valueExpr->variant)) {
                auto lit = std::get<LiteralExpr*>(valueExpr->variant);
                if (std::holds_alternative<long long>(lit->value)) {
                    nextValue = std::get<long long>(lit->value) + 1;
                }
            }
        } else {
            valueExpr = makeLiteralExpr(arena, memberTok.line, memberTok.column,
                                        (int)memberName.length(), memberTok.filename, nextValue);
            nextValue++;
        }

        ModelMember m;
        m.visibility = MemberVisibility::PUBLIC;
        m.isStatic = true;
        m.isMethod = false;
        m.name = memberName;
        m.typeHint = nullptr;
        m.initializer = valueExpr;
        members.push_back(m);

        if (match(TokenType::COMMA)) {
            skipNewlines();
        } else {
            skipNewlines();
        }
    }

    consume(TokenType::RBRACE, "Expected '}' after enum body");

    if (members.empty()) {
        throw ParseError("Enum '" + name + "' has no members", line);
    }

    return makeModelStmt(arena, line, column, length, nameToken.filename,
                         name, "", {}, {}, {}, {}, {}, members);
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
            typeHint = arena.allocate<TypeAST>("Any");
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

    // Lower the struct to a model.
    //
    // StructStmt was parsed and then thrown away: BytecodeCompiler::compileStmt
    // has no StructStmt branch, and std::visit on a variant with no matching
    // handler simply does nothing. A struct therefore emitted ZERO bytecode, the
    // name was never bound, and `new POINT()` failed with "'POINT' is not a
    // model" -- while the parser happily accepted the declaration, so the
    // feature looked supported right up until it was used.
    //
    // Lowering here rather than adding a compiler branch means the typechecker
    // sees a model too, so `new POINT()` type-checks instead of being an unknown
    // name. A struct is a model with no methods and a generated constructor:
    //
    //     struct POINT { x: int  y: int }
    //  => model POINT { init(x = 0, y = 0) { self.x = x  self.y = y } }
    //
    // Fields keep their declared order, so positional construction
    // (`new POINT(1920, 1080)`) works the way a POD type should.
    std::vector<StmtPtr> initBody;
    std::vector<ExprPtr> initDefaults;
    for (size_t i = 0; i < fields.size(); i++) {
        // self.<field> = <field>
        ExprPtr selfRef = makeSelfExpr(arena, line, column, length, nameToken.filename);
        ExprPtr paramRef = makeIdentifierExpr(arena, line, column, length, nameToken.filename, fields[i]);
        ExprPtr assign = makeSetExpr(arena, line, column, length, nameToken.filename,
                                     selfRef, fields[i], paramRef);
        initBody.push_back(makeExpressionStmt(arena, line, column, length, nameToken.filename, assign));

        // An omitted field gets a zero of its declared type rather than nil, so
        // `new POINT()` yields a usable zero value the way a C struct does.
        if (defaults[i]) {
            initDefaults.push_back(defaults[i]);
        } else {
            const std::string t = types[i] ? types[i]->baseType : std::string("Any");
            if (t == "int" || t == "int64" || t == "byte" || t == "number") {
                initDefaults.push_back(makeLiteralExpr(arena, line, column, length, nameToken.filename, (long long)0));
            } else if (t == "float" || t == "double") {
                initDefaults.push_back(makeLiteralExpr(arena, line, column, length, nameToken.filename, 0.0));
            } else if (t == "ptr") {
                initDefaults.push_back(makeLiteralExpr(arena, line, column, length, nameToken.filename, (long long)0));
            } else if (t == "string" || t == "str") {
                initDefaults.push_back(makeLiteralExpr(arena, line, column, length, nameToken.filename, std::string("")));
            } else if (t == "bool") {
                initDefaults.push_back(makeLiteralExpr(arena, line, column, length, nameToken.filename, false));
            } else {
                initDefaults.push_back(makeLiteralExpr(arena, line, column, length, nameToken.filename, nullptr));
            }
        }
    }

    auto stmt = makeModelStmt(arena, line, column, length, nameToken.filename,
                              name, "", {},
                              fields, types, initDefaults, initBody, {});
    std::get<ModelStmt*>(stmt->variant)->typeParams = typeParams;
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
    
    return makeUseStmt(arena, line, column, length, pathToken.filename, path, alias);
}

StmtPtr Parser::exportStatement() {
    Token tok = previous(); // the 'export' token
    skipNewlines();
    
    // Parse the inner declaration
    StmtPtr inner = declaration();
    if (!inner) {
        throw ParseError("Expected declaration after 'export'", tok.line);
    }
    return makeExportStmt(arena, tok.line, tok.column, (int)tok.lexeme.length(), tok.filename, std::move(inner));
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
            if (isStatic) throw ParseError("Constructor cannot be static", previous().line);
            consume(TokenType::LPAREN, "Expected '(' after 'init'");
            
            if (!check(TokenType::RPAREN)) {
                bool hadDefault = false;
                do {
                    Token paramToken = consume(TokenType::IDENTIFIER, "Expected parameter name");
                    initParams.push_back(paramToken.lexeme);
                    
                    if (match(TokenType::COLON)) {
                        initParamTypes.push_back(parseType());
                    } else {
                        initParamTypes.push_back(arena.allocate<TypeAST>("Any"));
                    }
                    
                    if (match(TokenType::EQUAL)) {
                        initDefaultValues.push_back(ternary());
                        hadDefault = true;
                    } else {
                        if (hadDefault) throw ParseError("Required parameter cannot follow optional parameter", paramToken.line);
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
            if (check(TokenType::AT) && peekNext().type == TokenType::IDENTIFIER && peekNext().lexeme == "cached") {
                advance(); // consume @
                advance(); // consume cached
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
            bool isVariadic = false;
            if (!check(TokenType::RPAREN)) {
                bool hadDefault = false;
                do {
                    if (match(TokenType::ELLIPSIS)) {
                        isVariadic = true;
                        Token paramToken = consume(TokenType::IDENTIFIER, "Expected parameter name after '...'");
                        params.push_back(paramToken.lexeme);
                        
                        TypeASTPtr pType = nullptr;
                        if (match(TokenType::COLON)) {
                            pType = parseType();
                        } else {
                            pType = arena.allocate<TypeAST>("Any");
                        }
                        paramTypes.push_back(pType);
                        defaultValues.push_back(nullptr);
                        if (check(TokenType::COMMA)) {
                            throw ParseError("Rest parameter must be the last parameter", peek().line);
                        }
                        break;
                    }

                    Token paramToken = advance();
                    if (paramToken.type == TokenType::RPAREN || paramToken.type == TokenType::COMMA) {
                        throw ParseError("Expected parameter name", paramToken.line);
                    }
                    params.push_back(paramToken.lexeme);
                    
                    if (match(TokenType::COLON)) {
                        paramTypes.push_back(parseType());
                    } else {
                        paramTypes.push_back(arena.allocate<TypeAST>("Any"));
                    }
                    
                    if (match(TokenType::EQUAL)) {
                        defaultValues.push_back(ternary());
                        hadDefault = true;
                    } else {
                        if (hadDefault) throw ParseError("Required parameter cannot follow optional parameter", paramToken.line);
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
                returnType = arena.allocate<TypeAST>("Any");
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
            member.isVariadic = isVariadic;
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
                throw ParseError("Expected 'task' after 'async' modifier", peek().line);
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
                while (check(TokenType::AT) && peekNext().type == TokenType::IDENTIFIER && peekNext().lexeme == "validate") {
                    advance(); // consume @
                    advance(); // consume validate
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
                                auto strLit = arena.allocate<LiteralExpr>(std::get<std::string>(maybeMsg.literal));
                                param = arena.allocate<Expr>(maybeMsg.line, maybeMsg.column, 1, maybeMsg.filename, strLit);
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
                throw ParseError("Unexpected token in model body", peek().line);
            }
        }
        }
        
        skipNewlines();
    }
    
    consume(TokenType::RBRACE, "Expected '}' after model body");
    
    // Use the peek().filename or current filename
    auto stmt = makeModelStmt(arena, line, column, length, nameToken.filename, name, parentName, interfaces, initParams, initParamTypes,
                         initDefaultValues, initBody, members);
    std::get<ModelStmt*>(stmt->variant)->typeParams = typeParams;
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
                    method.paramTypes.push_back(arena.allocate<TypeAST>("Any"));
                }
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "Expected ')' after method parameters");
        
        if (match(TokenType::ARROW)) {
            method.returnType = parseType();
        } else {
            method.returnType = arena.allocate<TypeAST>("Any");
        }
        
        methods.push_back(method);
        skipNewlines();
    }
    consume(TokenType::RBRACE, "Expected '}' after interface body");
    
    auto stmt = makeInterfaceStmt(arena, line, column, length, nameToken.filename, nameToken.lexeme, methods);
    std::get<InterfaceStmt*>(stmt->variant)->typeParams = typeParams;
    return stmt;
}
