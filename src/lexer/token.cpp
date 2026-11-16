#include "lexer/token.h"

namespace lexer {

const char* token_code(TokenKind kind) {
    switch (kind) {
        case TokenKind::Identifier:
            return "IDENFR";
        case TokenKind::IntConstant:
            return "INTCON";
        case TokenKind::CharConstant:
            return "CHARCON";
        case TokenKind::KeywordConst:
            return "CONSTTK";
        case TokenKind::KeywordInt:
            return "INTTK";
        case TokenKind::KeywordChar:
            return "CHARTK";
        case TokenKind::KeywordVoid:
            return "VOIDTK";
        case TokenKind::KeywordMain:
            return "MAINTK";
        case TokenKind::KeywordIf:
            return "IFTK";
        case TokenKind::KeywordElse:
            return "ELSETK";
        case TokenKind::KeywordSwitch:
            return "SWITCHTK";
        case TokenKind::KeywordCase:
            return "CASETK";
        case TokenKind::KeywordDefault:
            return "DEFAULTTK";
        case TokenKind::KeywordWhile:
            return "WHILETK";
        case TokenKind::KeywordFor:
            return "FORTK";
        case TokenKind::KeywordScanf:
            return "SCANFTK";
        case TokenKind::KeywordPrintf:
            return "PRINTFTK";
        case TokenKind::KeywordReturn:
            return "RETURNTK";
        case TokenKind::End:
        case TokenKind::Error:
        default:
            return "";
    }
}

}  // namespace lexer
