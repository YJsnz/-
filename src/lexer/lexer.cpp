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

    token.kind = TokenKind::Error;
    token.lexeme = std::string(1, scanner_.advance());
    token.message = "未识别的字符 '" + token.lexeme +
                    "'（字符串、运算符与界符将在后续任务支持）";
    return token;
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

}  // namespace lexer
