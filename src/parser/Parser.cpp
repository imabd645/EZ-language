#include "Parser.h"
#include <iostream>
#include "parser/Parser.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}



const Token& Parser::peek() const {
    return tokens[current];
}

const Token& Parser::peekNext() const {
    if (current + 1 >= tokens.size()) return tokens.back();
    return tokens[current + 1];
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

#include "vm/BytecodeVM.h"

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

