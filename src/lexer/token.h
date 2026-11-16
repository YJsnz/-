// 词法单元定义（P1-3 起逐步扩充）
#pragma once

#include <string>

namespace lexer {

enum class TokenKind {
    End = 0,
    Error,

    Identifier,
    IntConstant,
    CharConstant,
    StringConstant,

    Plus,
    Minus,
    Star,
    Slash,
    Assign,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    Colon,
    Semicolon,
    Comma,
    LParen,
    RParen,
    LBracket,
    RBracket,
    LBrace,
    RBrace,

    KeywordConst,
    KeywordInt,
    KeywordChar,
    KeywordVoid,
    KeywordMain,
    KeywordIf,
    KeywordElse,
    KeywordSwitch,
    KeywordCase,
    KeywordDefault,
    KeywordWhile,
    KeywordFor,
    KeywordScanf,
    KeywordPrintf,
    KeywordReturn,
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string raw;     // 源码原文（字符/字符串含引号）
    std::string lexeme;  // 输出形式（字符/字符串不含引号）
    std::string value;   // 独立单词值：标识符原名、保留字小写、整数规范十进制、字符/字符串内容
    long long int_value = 0;  // INTCON 的数值；CHARCON 的 ASCII 码
    int line = 1;
    int column = 1;
    std::string message;  // 仅 Error 使用
};

// 类别码，如 IDENFR / CONSTTK；End 与 Error 返回空串。
const char* token_code(TokenKind kind);

}  // namespace lexer
