#include "Parser.h"
#include <iostream>
ExprPtr Parser::expression() {
    return assignment();
}

ExprPtr Parser::assignment() {
    ExprPtr expr = ternary();
    
    if (match({TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, 
               TokenType::STAR_EQUAL, TokenType::STAR_STAR_EQUAL, TokenType::SLASH_EQUAL})) {
        Token op = previous();
        ExprPtr value = assignment();
        
        // Handle compound assignment
        if (op.type != TokenType::EQUAL) {
            if (std::holds_alternative<TupleExpr*>(expr->variant)) {
                throw ParseError("Compound assignment is not allowed for tuple destructuring", op.line);
            }
        }
        
        if (std::holds_alternative<IdentifierExpr*>(expr->variant)) {
            if (op.type != TokenType::EQUAL) {
                TokenType binOp;
                switch (op.type) {
                    case TokenType::PLUS_EQUAL: binOp = TokenType::PLUS; break;
                    case TokenType::MINUS_EQUAL: binOp = TokenType::MINUS; break;
                    case TokenType::STAR_EQUAL: binOp = TokenType::STAR; break;
                    case TokenType::STAR_STAR_EQUAL: binOp = TokenType::STAR_STAR; break;
                    case TokenType::SLASH_EQUAL: binOp = TokenType::SLASH; break;
                    default: binOp = TokenType::PLUS; break;
                }
                value = makeBinaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, binOp, value);
            }
            std::string name = std::get<IdentifierExpr*>(expr->variant)->name;
            return makeAssignExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, name, value);
        } else if (std::holds_alternative<IndexExpr*>(expr->variant)) {
            auto indexExpr = std::get<IndexExpr*>(expr->variant);
            std::optional<TokenType> compoundOp = (op.type != TokenType::EQUAL) ? std::make_optional(op.type) : std::nullopt;
            return makeAssignExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, "", value, indexExpr->index, indexExpr->object, compoundOp);
        } else if (std::holds_alternative<PropertyAccessExpr*>(expr->variant)) {
            auto propExpr = std::get<PropertyAccessExpr*>(expr->variant);
            std::optional<TokenType> compoundOp = (op.type != TokenType::EQUAL) ? std::make_optional(op.type) : std::nullopt;
            return makeSetExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, propExpr->object, propExpr->property, value, compoundOp);
        } else if (std::holds_alternative<TupleExpr*>(expr->variant)) {
            auto tupleExpr = std::get<TupleExpr*>(expr->variant);
            return makeDestructureAssignExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, tupleExpr->elements, std::move(value));
        }
        
        throw ParseError("Invalid assignment target", op.line);
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
        expr = makeTernaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, thenBranch, elseBranch);
    }
    
    return expr;
}

ExprPtr Parser::nullCoalescing() {
    ExprPtr expr = logicalOr();
    
    while (match(TokenType::QUESTION_QUESTION)) {
        Token op = previous();
        ExprPtr right = logicalOr();
        // Null coalescing acts like logical OR but strictly checks for nil
        expr = makeLogicalExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, TokenType::QUESTION_QUESTION, right);
    }
    
    return expr;
}

ExprPtr Parser::logicalOr() {
    ExprPtr expr = logicalAnd();
    
    while (match(TokenType::OR)) {
        Token op = previous();
        ExprPtr right = logicalAnd();
        expr = makeLogicalExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, TokenType::OR, right);
    }
    
    return expr;
}

ExprPtr Parser::logicalAnd() {
    ExprPtr expr = bitwiseOr();
    
    while (match(TokenType::AND)) {
        Token op = previous();
        ExprPtr right = bitwiseOr();
        expr = makeLogicalExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, TokenType::AND, right);
    }
    
    return expr;
}

ExprPtr Parser::bitwiseOr() {
    ExprPtr expr = bitwiseXor();
    while (match(TokenType::PIPE)) {
        Token op = previous();
        ExprPtr right = bitwiseXor();
        expr = makeBinaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    return expr;
}

ExprPtr Parser::bitwiseXor() {
    ExprPtr expr = bitwiseAnd();
    while (match(TokenType::CARET)) {
        Token op = previous();
        ExprPtr right = bitwiseAnd();
        expr = makeBinaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    return expr;
}

ExprPtr Parser::bitwiseAnd() {
    ExprPtr expr = equality();
    while (match(TokenType::AMPERSAND)) {
        Token op = previous();
        ExprPtr right = equality();
        expr = makeBinaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    return expr;
}

ExprPtr Parser::equality() {
    ExprPtr expr = comparison();
    
    while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL})) {
        Token op = previous();
        ExprPtr right = comparison();
        expr = makeBinaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::comparison() {
    ExprPtr expr = bitwiseShift();
    
    while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, 
                  TokenType::LESS, TokenType::LESS_EQUAL, TokenType::IN})) {
        Token op = previous();
        ExprPtr right = bitwiseShift();
        expr = makeBinaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::bitwiseShift() {
    ExprPtr expr = term();
    
    while (match({TokenType::LSHIFT, TokenType::RSHIFT})) {
        Token op = previous();
        ExprPtr right = term();
        expr = makeBinaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::term() {
    ExprPtr expr = factor();
    
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        Token op = previous();
        ExprPtr right = factor();
        expr = makeBinaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::factor() {
    ExprPtr expr = power();
    
    while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        Token op = previous();
        ExprPtr right = power();
        expr = makeBinaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::power() {
    ExprPtr expr = unary();
    
    // Right-associative: 2 ** 3 ** 2 = 2 ** (3 ** 2) = 512
    if (match({TokenType::STAR_STAR})) {
        Token op = previous();
        ExprPtr right = power(); // recurse for right-associativity
        expr = makeBinaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, op.type, right);
    }
    
    return expr;
}

ExprPtr Parser::unary() {
    if (match({TokenType::BANG, TokenType::MINUS, TokenType::NOT, TokenType::TILDE})) {
        Token op = previous();
        ExprPtr right = unary();
        return makeUnaryExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, op.type, right);
    }
    
    if (match(TokenType::AWAIT)) {
        Token op = previous();
        ExprPtr right = unary();
        return arena.allocate<Expr>(op.line, op.column, op.lexeme.length(), op.filename, arena.allocate<AwaitExpr>(right));
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
            expr = makeIndexExpr(arena, op.line, op.column, op.lexeme.length(), op.filename, expr, index);
        } else if (match(TokenType::DOT) || match(TokenType::QUESTION_DOT)) {
            bool isOptional = previous().type == TokenType::QUESTION_DOT;
            // Allow keywords as property names
            advance();
            Token name = previous();
            expr = makePropertyAccessExpr(arena, name.line, name.column, name.lexeme.length(), name.filename, expr, name.lexeme, isOptional);
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
                expr = makePropertyAccessExpr(arena, name.line, name.column, name.lexeme.length(), name.filename, expr, name.lexeme, isOptional);
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
    std::vector<std::string> argNames;
    
    if (!check(TokenType::RPAREN)) {
        do {
            if (check(TokenType::IDENTIFIER) && peekNext().type == TokenType::EQUAL) {
                argNames.push_back(advance().lexeme);
                advance(); // Consume '='
                arguments.push_back(expression());
            } else if (match(TokenType::ELLIPSIS)) {
                argNames.push_back("");
                arguments.push_back(makeSpreadExpr(arena, line, column, length, peek().filename, expression()));
            } else {
                argNames.push_back("");
                arguments.push_back(expression());
            }
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::RPAREN, "Expected ')' after arguments");
    
    return makeCallExpr(arena, line, column, length, peek().filename, callee, arguments, argNames);
}

ExprPtr Parser::primary() {
    int line = peek().line;
    int column = peek().column;
    int length = peek().lexeme.length();
    std::string filename = peek().filename;
    
    if (match(TokenType::FALSE)) return makeLiteralExpr(arena, line, column, length, filename, false);
    if (match(TokenType::TRUE)) return makeLiteralExpr(arena, line, column, length, filename, true);
    if (match(TokenType::NIL)) return makeLiteralExpr(arena, line, column, length, filename, nullptr);
    
    if (match(TokenType::NEW)) {
        Token t = previous();
        Token nameToken = consume(TokenType::IDENTIFIER, "Expected model name after 'new'");
        
        std::vector<TypeASTPtr> typeArgs;
        if (match(TokenType::LBRACKET)) {
            do {
                typeArgs.push_back(parseType());
            } while (match(TokenType::COMMA));
            consume(TokenType::RBRACKET, "Expected ']' after type arguments");
        }
        
        std::vector<ExprPtr> args;
        if (match(TokenType::LPAREN)) {
            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(expression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "Expected ')' after arguments");
        }
        return makeNewExpr(arena, t.line, t.column, t.lexeme.length(), t.filename, nameToken.lexeme, args, typeArgs);
    }
    
    if (match(TokenType::NUMBER)) {
        Token t = previous();
        if (std::holds_alternative<long long>(t.literal)) {
            return makeLiteralExpr(arena, t.line, t.column, t.lexeme.length(), t.filename, std::get<long long>(t.literal));
        }
        return makeLiteralExpr(arena, t.line, t.column, t.lexeme.length(), t.filename, std::get<double>(t.literal));
    }
    
    if (match(TokenType::STRING)) {
        Token t = previous();
        return makeLiteralExpr(arena, t.line, t.column, t.lexeme.length(), t.filename, std::get<std::string>(t.literal));
    }
    
    if (match(TokenType::IDENTIFIER)) {
        Token t = previous();
        return makeIdentifierExpr(arena, t.line, t.column, t.lexeme.length(), t.filename, t.lexeme);
    }
    
    if (match(TokenType::SUPER)) {
        Token t = previous();
        return makeSuperExpr(arena, t.line, t.column, t.lexeme.length(), t.filename);
    }
    
    // Self reference
    if (match(TokenType::SELF)) {
        Token t = previous();
        return makeSelfExpr(arena, t.line, t.column, t.lexeme.length(), t.filename);
    }
    
    if (match(TokenType::IN)) {
        Token t = previous();
        // Special case for 'in' keyword used alone (not as operator)
        // Historically mapped to __input__
        return makeCallExpr(arena, t.line, t.column, t.lexeme.length(), t.filename, makeIdentifierExpr(arena, t.line, t.column, t.lexeme.length(), t.filename, "__input__"), {});
    }
    
    // Lambda expression: |params| => expr or |params| { body }
    if (match(TokenType::PIPE)) {
        return lambdaExpression(false);
    }
    
    // Async Lambda expression: async |params| => expr
    if (match(TokenType::ASYNC)) {
        int asyncLine = previous().line;
        int asyncCol = previous().column;
        int asyncLen = previous().lexeme.length();

        if (match(TokenType::PIPE)) {
            return lambdaExpression(true);
        }

        // `async { ... }` block (README, "async { ... } blocks"):
        //     result = await async { wait(500)  give "done" }
        // It evaluates to a future, so it is an IMMEDIATELY-INVOKED
        // zero-parameter async lambda -- calling an async function is what
        // produces the future. This was documented but never parsed; `async {`
        // failed with "Expected '|' for lambda after 'async'".
        if (check(TokenType::LBRACE)) {
            ExprPtr asyncLambda = lambdaExpression(true, /*noParams=*/true);
            return makeCallExpr(arena, asyncLine, asyncCol, asyncLen, peek().filename,
                                asyncLambda, std::vector<ExprPtr>{}, std::vector<std::string>{});
        }

        throw ParseError("Expected '|' or '{' after 'async'", previous().line);
    }
    
    if (match(TokenType::LBRACKET)) {
        // Array literal
        std::vector<ExprPtr> elements;
        
        if (!check(TokenType::RBRACKET)) {
            do {
                skipNewlines();
                if (match(TokenType::ELLIPSIS)) {
                    elements.push_back(makeSpreadExpr(arena, line, column, length, filename, expression()));
                } else {
                    elements.push_back(expression());
                }
                skipNewlines();
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RBRACKET, "Expected ']' after array elements");
        return makeArrayExpr(arena, line, column, length, filename, elements);
    }
    
    // Dictionary literal
    if (match(TokenType::LBRACE)) {
        int line = previous().line;
        int column = previous().column;
    int length = previous().lexeme.length();
        std::vector<std::pair<ExprPtr, ExprPtr>> pairs;
        
        skipNewlines();
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            ExprPtr key = ternary();
            
            consume(TokenType::COLON, "Expected ':' after dictionary key");
            
            // Support keys like {x: 1} -> {"x": 1}
            // If the key is an identifier, convert it to a string literal
            if (std::holds_alternative<IdentifierExpr*>(key->variant)) {
                auto ident = std::get<IdentifierExpr*>(key->variant);
                key = makeLiteralExpr(arena, key->line, key->column, key->length, key->filename, ident->name);
            }
            
            ExprPtr value = expression();
            pairs.push_back({key, value});
            
            if (match(TokenType::COMMA)) {
                skipNewlines();
            } else if (check(TokenType::NEWLINE)) {
                skipNewlines();
            } else if (!check(TokenType::RBRACE)) {
                throw ParseError("Expected ',' or newline between dictionary entries", peek().line);
            }
        }
        
        consume(TokenType::RBRACE, "Expected '}' after dictionary");
        return makeDictionaryExpr(arena, line, column, length, filename, pairs);
    }
    
    if (match(TokenType::LPAREN)) {
        if (match(TokenType::RPAREN)) {
            // Empty tuple
            return makeTupleExpr(arena, line, column, length, filename, {});
        }
        ExprPtr expr = expression();
        if (match(TokenType::COMMA)) {
            // Tuple with multiple elements or a single element like (1,)
            std::vector<ExprPtr> elements = {expr};
            if (!check(TokenType::RPAREN)) {
                do {
                    skipNewlines();
                    elements.push_back(expression());
                    skipNewlines();
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "Expected ')' after tuple elements");
            return makeTupleExpr(arena, line, column, length, filename, elements);
        }
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    throw ParseError("Expected expression", line);
}

// Lambda: |x, y| => x + y  OR  |x, y| { statements }
ExprPtr Parser::lambdaExpression(bool isAsync, bool noParams) {
    int line = previous().line;
    int column = previous().column;
    int length = previous().lexeme.length();

    // Parse parameters
    std::vector<std::string> params;
    bool isVariadic = false;

    // An `async { ... }` block has no `|...|` list at all -- it is a
    // zero-parameter lambda, so skip straight to the body.
    if (!noParams) {
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
    }
    
    // Check for block body { ... } or expression body => expr
    if (match(TokenType::ARROW)) {
        // Expression body
        skipNewlines();
        ExprPtr body = expression();
        return makeLambdaExpr(arena, line, column, length, peek().filename, params, std::vector<TypeASTPtr>(params.size(), arena.allocate<TypeAST>("Any")), body, nullptr, isVariadic, isAsync);
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
        return makeLambdaExpr(arena, line, column, length, peek().filename, params, std::vector<TypeASTPtr>(params.size(), arena.allocate<TypeAST>("Any")), stmtBody, nullptr, isVariadic, isAsync);
    } else {
        // Default: treat as expression body without arrow
        ExprPtr body = expression();
        return makeLambdaExpr(arena, line, column, length, peek().filename, params, std::vector<TypeASTPtr>(params.size(), arena.allocate<TypeAST>("Any")), body, nullptr, isVariadic, isAsync);
    }
}

// Model (class) definition
// Syntax: model Name extends Parent { init(params) { body } shown/hidden members and methods }
