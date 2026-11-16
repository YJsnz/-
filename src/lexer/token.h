// 词法单元定义（P1-3 起逐步扩充）
#pragma once

#include <string>

namespace lexer {

enum class TokenKind {
    End = 0,
    Error,

    Identifier,
    IntConstant,

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
    std::string lexeme;  // 原样保留（大小写、前导零等）
    int line = 1;
    int column = 1;
    std::string message;  // 仅 Error 使用
};

// 类别码，如 IDENFR / CONSTTK；End 与 Error 返回空串。
const char* token_code(TokenKind kind);

}  // namespace lexer
