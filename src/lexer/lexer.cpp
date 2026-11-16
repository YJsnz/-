#include "lexer/lexer.h"

#include <cstddef>

namespace lexer {
namespace {

struct KeywordEntry {
    const char* text;
    TokenKind kind;
};

// 15 个保留字；匹配时不区分大小写。
const KeywordEntry kKeywords[] = {
    {"const", TokenKind::KeywordConst},
    {"int", TokenKind::KeywordInt},
    {"char", TokenKind::KeywordChar},
    {"void", TokenKind::KeywordVoid},
    {"main", TokenKind::KeywordMain},
    {"if", TokenKind::KeywordIf},
    {"else", TokenKind::KeywordElse},
    {"switch", TokenKind::KeywordSwitch},
    {"case", TokenKind::KeywordCase},
    {"default", TokenKind::KeywordDefault},
    {"while", TokenKind::KeywordWhile},
    {"for", TokenKind::KeywordFor},
    {"scanf", TokenKind::KeywordScanf},
    {"printf", TokenKind::KeywordPrintf},
    {"return", TokenKind::KeywordReturn},
};

// <字母> ::= _ | a..z | A..Z
bool is_letter(char c) {
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_identifier_char(char c) {
    return is_letter(c) || is_digit(c);
}

// <字符> ::= '+' | '-' | '*' | '/' | <字母> | <数字>
bool is_char_literal_char(char c) {
    return is_letter(c) || is_digit(c) || c == '+' || c == '-' || c == '*' || c == '/';
}

// 字符串允许十进制编码 32、33、35-126 的 ASCII 字符（不含双引号 34）。
bool is_string_char(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return u == 32 || u == 33 || (u >= 35 && u <= 126);
}

char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

std::string to_ascii_lower(const std::string& text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (char c : text) {
        lowered.push_back(ascii_lower(c));
    }
    return lowered;
}

TokenKind keyword_kind(const std::string& lexeme) {
    const std::string lowered = to_ascii_lower(lexeme);
    for (const KeywordEntry& entry : kKeywords) {
        if (lowered == entry.text) {
            return entry.kind;
        }
    }
    return TokenKind::Identifier;
}

}  // namespace

Lexer::Lexer(const std::string& source) : scanner_(source) {}

Token Lexer::next_token() {
    scanner_.skip_whitespace();

    Token token;
    token.line = scanner_.line();
    token.column = scanner_.column();

    if (scanner_.at_end()) {
        token.kind = TokenKind::End;
        return token;
    }

    if (is_letter(scanner_.peek())) {
        return scan_identifier_or_keyword();
    }

    if (is_digit(scanner_.peek())) {
        return scan_number();
    }

    if (scanner_.peek() == '\'') {
        return scan_char_constant();
    }

    if (scanner_.peek() == '"') {
        return scan_string_constant();
    }

    return scan_operator_or_delimiter();
}

Token Lexer::scan_identifier_or_keyword() {
    Token token;
    token.line = scanner_.line();
    token.column = scanner_.column();

    while (!scanner_.at_end() && is_identifier_char(scanner_.peek())) {
        token.lexeme.push_back(scanner_.advance());
    }

    token.kind = keyword_kind(token.lexeme);
    return token;
}

Token Lexer::scan_number() {
    Token token;
    token.line = scanner_.line();
    token.column = scanner_.column();

    while (!scanner_.at_end() && is_digit(scanner_.peek())) {
        token.lexeme.push_back(scanner_.advance());
    }

    token.kind = TokenKind::IntConstant;
    return token;
}

Token Lexer::scan_char_constant() {
    Token token;
    token.line = scanner_.line();
    token.column = scanner_.column();

    scanner_.advance();  // 开引号

    if (scanner_.at_end()) {
        token.kind = TokenKind::Error;
        token.message = "字符常量缺少结束引号";
        return token;
    }

    const char content = scanner_.peek();
    if (content == '\'') {
        scanner_.advance();
        token.kind = TokenKind::Error;
        token.lexeme = "''";
        token.message = "字符常量不能为空";
        return token;
    }

    if (!is_char_literal_char(content)) {
        scanner_.advance();
        token.kind = TokenKind::Error;
        token.lexeme = std::string("'") + content;
        token.message = "字符常量中出现非法字符 '" + std::string(1, content) + "'";
        return token;
    }

    token.lexeme = std::string(1, content);  // 输出不含引号
    scanner_.advance();                       // 内容字符

    if (scanner_.at_end() || scanner_.peek() != '\'') {
        token.kind = TokenKind::Error;
        token.message = "字符常量只能包含一个字符，且必须以单引号结束";
        return token;
    }

    scanner_.advance();  // 闭引号
    token.kind = TokenKind::CharConstant;
    return token;
}

Token Lexer::scan_string_constant() {
    Token token;
    token.line = scanner_.line();
    token.column = scanner_.column();

    scanner_.advance();  // 开引号
    std::string content;

    for (;;) {
        if (scanner_.at_end()) {
            token.kind = TokenKind::Error;
            token.lexeme = content;
            token.message = "字符串常量缺少结束引号";
            return token;
        }

        const char c = scanner_.peek();
        if (c == '"') {
            scanner_.advance();
            if (content.empty()) {
                token.kind = TokenKind::Error;
                token.lexeme = "\"\"";
                token.message = "字符串常量不能为空";
                return token;
            }
            token.kind = TokenKind::StringConstant;
            token.lexeme = content;  // 输出不含双引号
            return token;
        }

        if (!is_string_char(c)) {
            const unsigned char u = static_cast<unsigned char>(c);
            scanner_.advance();
            token.kind = TokenKind::Error;
            token.lexeme = content;
            token.message = "字符串常量中出现非法字符（ASCII " + std::to_string(u) + "）";
            return token;
        }

        content.push_back(scanner_.advance());
    }
}

Token Lexer::scan_operator_or_delimiter() {
    Token token;
    token.line = scanner_.line();
    token.column = scanner_.column();

    token.lexeme.push_back(scanner_.advance());
    const char c = token.lexeme[0];

    switch (c) {
        case '+':
            token.kind = TokenKind::Plus;
            break;
        case '-':
            token.kind = TokenKind::Minus;
            break;
        case '*':
            token.kind = TokenKind::Star;
            break;
        case '/':
            token.kind = TokenKind::Slash;
            break;
        case '=':
            if (scanner_.peek() == '=') {
                token.lexeme.push_back(scanner_.advance());
                token.kind = TokenKind::Equal;
            } else {
                token.kind = TokenKind::Assign;
            }
            break;
        case '<':
            if (scanner_.peek() == '=') {
                token.lexeme.push_back(scanner_.advance());
                token.kind = TokenKind::LessEqual;
            } else {
                token.kind = TokenKind::Less;
            }
            break;
        case '>':
            if (scanner_.peek() == '=') {
                token.lexeme.push_back(scanner_.advance());
                token.kind = TokenKind::GreaterEqual;
            } else {
                token.kind = TokenKind::Greater;
            }
            break;
        case '!':
            if (scanner_.peek() == '=') {
                token.lexeme.push_back(scanner_.advance());
                token.kind = TokenKind::NotEqual;
            } else {
                token.kind = TokenKind::Error;
                token.message = "未识别的字符 '!'（只有 '!=' 是合法运算符）";
            }
            break;
        case ';':
            token.kind = TokenKind::Semicolon;
            break;
        case ',':
            token.kind = TokenKind::Comma;
            break;
        case '(':
            token.kind = TokenKind::LParen;
            break;
        case ')':
            token.kind = TokenKind::RParen;
            break;
        case '[':
            token.kind = TokenKind::LBracket;
            break;
        case ']':
            token.kind = TokenKind::RBracket;
            break;
        case '{':
            token.kind = TokenKind::LBrace;
            break;
        case '}':
            token.kind = TokenKind::RBrace;
            break;
        case ':':
            token.kind = TokenKind::Colon;
            break;
        default:
            token.kind = TokenKind::Error;
            token.message = "未识别的字符 '" + token.lexeme + "'";
            break;
    }

    return token;
}

}  // namespace lexer
