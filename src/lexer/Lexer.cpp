#include "lexer/Lexer.h"
#include <iostream>
#include <stdexcept>

std::unordered_map<std::string, TokenType> Lexer::keywords = {
    {"out", TokenType::OUT},
    {"in", TokenType::IN},
    {"when", TokenType::WHEN},
    {"other", TokenType::OTHER},
    {"repeat", TokenType::REPEAT},
    {"to", TokenType::TO},
    {"step", TokenType::STEP},
    {"while", TokenType::WHILE},
    {"use", TokenType::USE},
    {"task", TokenType::TASK},
    {"decorator", TokenType::DECORATOR_KW},
    {"give", TokenType::GIVE},
    {"escape", TokenType::ESCAPE},
    {"skip", TokenType::SKIP},
    {"get", TokenType::GET},
    {"match", TokenType::MATCH},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"not", TokenType::NOT},
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
    {"yes", TokenType::TRUE},
    {"no", TokenType::FALSE},
    {"nil", TokenType::NIL},
    {"async", TokenType::ASYNC},
    {"await", TokenType::AWAIT},
    // OOP keywords
    {"model", TokenType::MODEL},
    {"init", TokenType::INIT},
    {"self", TokenType::SELF},
    {"hidden", TokenType::HIDDEN},
    {"shown", TokenType::SHOWN},
    {"extends", TokenType::EXTENDS},
    {"struct", TokenType::STRUCT},
    {"enum", TokenType::ENUM},
    {"new", TokenType::NEW},
    {"super", TokenType::SUPER},
    {"static", TokenType::STATIC},
    {"interface", TokenType::INTERFACE},
    {"implements", TokenType::IMPLEMENTS},
    {"try", TokenType::TRY},
    {"catch", TokenType::CATCH},
    {"throw", TokenType::THROW},
    {"finally", TokenType::FINALLY},
    {"export", TokenType::EXPORT},
    {"requires", TokenType::REQUIRES},
    {"ensures", TokenType::ENSURES}
};

Lexer::Lexer(const std::string& source, const std::string& filename) : source(source), filename(filename) {}

std::vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        start = current;
        scanToken();
    }
    
    tokens.push_back(Token(TokenType::END_OF_FILE, "", line, column, filename));
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
        case '(': openParens++; addToken(TokenType::LPAREN); break;
        case ')': if (openParens > 0) openParens--; addToken(TokenType::RPAREN); break;
        case '[': openBrackets++; addToken(TokenType::LBRACKET); break;
        case ']': if (openBrackets > 0) openBrackets--; addToken(TokenType::RBRACKET); break;
        case '{': addToken(TokenType::LBRACE); break;
        case '}': addToken(TokenType::RBRACE); break;
        case ',': addToken(TokenType::COMMA); break;
        case '.':
            if (match('.')) {
                if (match('.')) {
                    addToken(TokenType::ELLIPSIS);
                } else {
                    error("Invalid token '..'");
                }
            } else {
                addToken(TokenType::DOT);
            }
            break;
        case ':': addToken(TokenType::COLON); break;
        case '@':
            addToken(TokenType::AT);
            break;
        case '?':
            if (match('?')) {
                addToken(TokenType::QUESTION_QUESTION);
            } else if (match('.')) {
                addToken(TokenType::QUESTION_DOT);
            } else {
                addToken(TokenType::QUESTION_MARK);
            }
            break;
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
            } else if (match('>')) {
                addToken(TokenType::ARROW);
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
            if (match('<')) {
                addToken(TokenType::LSHIFT);
            } else if (match('=')) {
                addToken(TokenType::LESS_EQUAL);
            } else {
                addToken(TokenType::LESS);
            }
            break;
        case '>':
            if (match('>')) {
                addToken(TokenType::RSHIFT);
            } else if (match('=')) {
                addToken(TokenType::GREATER_EQUAL);
            } else {
                addToken(TokenType::GREATER);
            }
            break;
        case '&':
            addToken(TokenType::AMPERSAND);
            break;
        case '|':
            addToken(TokenType::PIPE);
            break;
        case '^':
            addToken(TokenType::CARET);
            break;
        case '~':
            addToken(TokenType::TILDE);
            break;
        case '#':
            skipLineComment();
            break;
        case '\n': {
            if (openParens > 0 || openBrackets > 0) {
                break; // Ignore newline inside parens/brackets
            }
            if (!tokens.empty()) {
                TokenType lastType = tokens.back().type;
                if (lastType == TokenType::PLUS ||
                    lastType == TokenType::MINUS ||
                    lastType == TokenType::STAR ||
                    lastType == TokenType::SLASH ||
                    lastType == TokenType::PERCENT ||
                    lastType == TokenType::EQUAL_EQUAL ||
                    lastType == TokenType::BANG_EQUAL ||
                    lastType == TokenType::LESS ||
                    lastType == TokenType::LESS_EQUAL ||
                    lastType == TokenType::GREATER ||
                    lastType == TokenType::GREATER_EQUAL ||
                    lastType == TokenType::AND ||
                    lastType == TokenType::OR ||
                    lastType == TokenType::COMMA ||
                    lastType == TokenType::DOT ||
                    lastType == TokenType::EQUAL ||
                    lastType == TokenType::PLUS_EQUAL ||
                    lastType == TokenType::MINUS_EQUAL ||
                    lastType == TokenType::STAR_EQUAL ||
                    lastType == TokenType::SLASH_EQUAL ||
                    lastType == TokenType::AMPERSAND ||
                    lastType == TokenType::PIPE ||
                    lastType == TokenType::CARET ||
                    lastType == TokenType::LSHIFT ||
                    lastType == TokenType::RSHIFT ||
                    lastType == TokenType::ARROW) {
                    break; // Implicit continuation after operator
                }
            }
            addToken(TokenType::NEWLINE);
            break;
        }
        case ';':
            // ';' is an OPTIONAL statement terminator. SYNTAX.md has documented
            // it since the beginning, but the lexer had no case for it, so it
            // fell through to the default branch and every use was rejected with
            // "Unexpected character: ;".
            //
            // The parser separates statements on NEWLINE, so emitting a NEWLINE
            // here IS the semantics: `a = 1; b = 2` becomes indistinguishable
            // from the same two statements on separate lines. Emitted directly
            // rather than falling into the '\n' case, which suppresses the token
            // after a trailing operator for implicit line continuation -- a
            // semicolon is explicit and should never be swallowed.
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
        case '`':
            scanInterpolatedString();
            break;
        default:
            if (isDigit(c)) {
                if (c == '0' && (peek() == 'x' || peek() == 'X')) {
                    advance(); // Consume 'x'
                    scanHexNumber();
                } else {
                    scanNumber();
                }
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
    tokens.push_back(Token(type, text, line, column - (int)(current - start), filename));
}

void Lexer::addToken(TokenType type, double value) {
    std::string text = source.substr(start, current - start);
    tokens.push_back(Token(type, text, value, line, column - (int)(current - start), filename));
}

void Lexer::addToken(TokenType type, long long value) {
    std::string text = source.substr(start, current - start);
    tokens.push_back(Token(type, text, value, line, column - (int)(current - start), filename));
}

void Lexer::addToken(TokenType type, const std::string& value) {
    std::string text = source.substr(start, current - start);
    tokens.push_back(Token(type, text, value, line, column - (int)(current - start), filename));
}

void Lexer::addToken(TokenType type, bool value) {
    std::string text = source.substr(start, current - start);
    tokens.push_back(Token(type, text, value, line, column - (int)(current - start), filename));
}

void Lexer::scanString() {
    char quote = source[start];
    std::string value;
    
    bool isMultiline = false;
    if (quote == '"' && peek() == '"' && peekNext() == '"') {
        isMultiline = true;
        advance(); // consume 2nd quote
        advance(); // consume 3rd quote
    }
    
    while (!isAtEnd()) {
        if (isMultiline) {
            if (peek() == '"' && peekNext() == '"' && current + 2 < source.length() && source[current + 2] == '"') {
                break;
            }
            if (peek() == '\n') {
                line++;
                column = 1;
            }
        } else {
            if (peek() == quote) break;
            if (peek() == '\n') {
                error("Unterminated string");
                return;
            }
        }
        
        if (peek() == '\\' && !isAtEnd()) {
            advance();
            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '0': value += '\0'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                case 'x': {
                    if (current + 2 > source.length()) {
                        error("Incomplete hex escape");
                        break;
                    }
                    std::string hex = source.substr(current, 2);
                    current += 2;
                    column += 2;
                    try {
                        value += static_cast<char>(std::stoi(hex, nullptr, 16));
                    } catch (...) {
                        error("Invalid hex escape: \\x" + hex);
                    }
                    break;
                }
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
    
    if (isMultiline) {
        advance(); advance(); advance(); // consume closing """
    } else {
        advance(); // consume closing quote
    }
    
    addToken(TokenType::STRING, value);
}

void Lexer::scanNumber() {
    bool isFloat = false;
    while (isDigit(peek())) advance();
    
    // Look for decimal part
    if (peek() == '.' && isDigit(peekNext())) {
        isFloat = true;
        advance(); // Consume '.'
        while (isDigit(peek())) advance();
    }
    
    // Look for exponent part
    if (peek() == 'e' || peek() == 'E') {
        isFloat = true;
        advance(); // Consume 'e'
        if (peek() == '+' || peek() == '-') advance();
        if (!isDigit(peek())) error("Expected digits after exponent");
        while (isDigit(peek())) advance();
    }
    
    std::string text = source.substr(start, current - start);
    if (isFloat) {
        addToken(TokenType::NUMBER, std::stod(text));
    } else {
        try {
            addToken(TokenType::NUMBER, std::stoll(text));
        } catch (...) {
            // Fallback to double for extremely large integers
            addToken(TokenType::NUMBER, std::stod(text));
        }
    }
}

void Lexer::scanHexNumber() {
    while (isxdigit(peek())) advance();
    std::string text = source.substr(start + 2, current - (start + 2)); // Skip 0x
    if (text.empty()) {
        error("Expected hexadecimal digits after '0x'");
        return;
    }
    try {
        addToken(TokenType::NUMBER, static_cast<long long>(std::stoull(text, nullptr, 16)));
    } catch (...) {
        error("Hexadecimal literal too large");
    }
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
        // Check for raw string prefix: r"..." or r'...'
        if (text == "r" && (peek() == '"' || peek() == '\'')) {
            start = current; // Move start past 'r'
            scanRawString();
            return;
        }
        addToken(TokenType::IDENTIFIER);
    }
}

void Lexer::scanRawString() {
    char quote = advance(); // Consume '"' or '\''
    std::string value;
    
    while (!isAtEnd() && peek() != quote) {
        if (peek() == '\n') {
            line++;
            column = 1;
        }
        value += advance();
    }
    
    if (isAtEnd()) {
        error("Unterminated raw string");
        return;
    }
    
    advance(); // Consume closing quote
    addToken(TokenType::STRING, value);
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

#include "vm/BytecodeVM.h"

void Lexer::error(const std::string& message) {
    hadError = true;
    std::cerr << "\nError: " << message << "\n"
              << "  " << (filename.empty() ? "<unknown>" : filename)
              << ":" << line << ":" << column << "\n\n";
              
    const std::string* sourceLine = EZ_GetSourceLine(filename, line);
    if (sourceLine) {
        std::cerr << "    " << *sourceLine << "\n";
        std::string caret(column > 0 ? column - 1 : 0, ' ');
        std::cerr << "    " << caret << "^\n";
    }
}

void Lexer::scanInterpolatedString() {
    // Backtick template string: `Hello {name}, age {age + 1}`
    // Emits: ( STRING("Hello ") + str(name) + STRING(", age ") + str(age + 1) + STRING("") )
    // We emit a LPAREN, then alternating STRING + PLUS + expression tokens, then RPAREN
    
    int startLine = line;
    int startCol = column;
    
    // Collect all parts first
    struct Part {
        bool isExpr;
        std::string text;
    };
    std::vector<Part> parts;
    std::string currentText;
    
    while (!isAtEnd() && peek() != '`') {
        if (peek() == '\n') {
            currentText += '\n';
            advance();
        } else if (peek() == '\\') {
            advance();
            if (!isAtEnd()) {
                char escaped = advance();
                switch (escaped) {
                    case 'n': currentText += '\n'; break;
                    case 't': currentText += '\t'; break;
                    case 'r': currentText += '\r'; break;
                    case '\\': currentText += '\\'; break;
                    case '`': currentText += '`'; break;
                    case '{': currentText += '{'; break;
                    case '}': currentText += '}'; break;
                    default: currentText += escaped; break;
                }
            }
        } else if (peek() == '{') {
            // Save current text as a string part
            parts.push_back({false, currentText});
            currentText.clear();
            advance(); // consume '{'
            
            // Collect expression text until matching '}'
            std::string exprText;
            int braceDepth = 1;
            while (!isAtEnd() && braceDepth > 0) {
                char p = peek();
                if (p == '"' || p == '\'') {
                    char quote = p;
                    exprText += advance();
                    while (!isAtEnd()) {
                        char inStr = advance();
                        exprText += inStr;
                        if (inStr == '\\' && !isAtEnd()) {
                            exprText += advance(); // skip escaped char
                        } else if (inStr == quote) {
                            break;
                        }
                    }
                    continue;
                }
                if (p == '{') braceDepth++;
                if (p == '}') {
                    braceDepth--;
                    if (braceDepth == 0) break;
                }
                exprText += advance();
            }
            
            if (isAtEnd()) {
                error("Unterminated interpolation in template string");
                return;
            }
            advance(); // consume '}'
            
            if (!exprText.empty()) {
                parts.push_back({true, exprText});
            }
        } else {
            currentText += advance();
        }
    }
    
    if (isAtEnd()) {
        error("Unterminated template string");
        return;
    }
    advance(); // consume closing backtick
    
    // Add trailing text
    parts.push_back({false, currentText});
    
    // If no interpolations, just emit a simple string
    bool hasExpr = false;
    for (auto& p : parts) { if (p.isExpr) { hasExpr = true; break; } }
    
    if (!hasExpr) {
        // Simple string, no interpolation
        std::string fullText;
        for (auto& p : parts) fullText += p.text;
        tokens.push_back(Token(TokenType::STRING, "`" + fullText + "`", fullText, startLine, startCol, filename));
        return;
    }
    
    // Emit: LPAREN + string/expr parts joined by PLUS + RPAREN
    tokens.push_back(Token(TokenType::LPAREN, "(", startLine, startCol, filename));
    
    bool first = true;
    for (auto& part : parts) {
        if (!first) {
            tokens.push_back(Token(TokenType::PLUS, "+", startLine, startCol, filename));
        }
        first = false;
        
        if (!part.isExpr) {
            // String literal part
            tokens.push_back(Token(TokenType::STRING, "\"" + part.text + "\"", part.text, startLine, startCol, filename));
        } else {
            // Expression part: emit str( <tokens> )
            tokens.push_back(Token(TokenType::IDENTIFIER, "str", startLine, startCol, filename));
            tokens.push_back(Token(TokenType::LPAREN, "(", startLine, startCol, filename));
            
            // Sub-lex the expression
            Lexer subLexer(part.text, filename);
            auto subTokens = subLexer.tokenize();
            for (auto& t : subTokens) {
                if (t.type != TokenType::END_OF_FILE && t.type != TokenType::NEWLINE) {
                    t.line = startLine; // Fix line numbers
                    t.filename = filename;
                    tokens.push_back(t);
                }
            }
            
            tokens.push_back(Token(TokenType::RPAREN, ")", startLine, startCol, filename));
        }
    }
    
    tokens.push_back(Token(TokenType::RPAREN, ")", startLine, startCol, filename));
}
