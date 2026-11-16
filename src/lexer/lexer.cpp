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

    token.kind = TokenKind::Error;
    token.lexeme = std::string(1, scanner_.advance());
    token.message = "未识别的字符 '" + token.lexeme +
                    "'（数字、字符、字符串与运算符将在后续任务支持）";
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

}  // namespace lexer
