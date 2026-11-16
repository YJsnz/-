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
        case TokenKind::StringConstant:
            return "STRCON";
        case TokenKind::Plus:
            return "PLUS";
        case TokenKind::Minus:
            return "MINU";
        case TokenKind::Star:
            return "MULT";
        case TokenKind::Slash:
            return "DIV";
        case TokenKind::Assign:
            return "ASSIGN";
        case TokenKind::Less:
            return "LSS";
        case TokenKind::LessEqual:
            return "LEQ";
        case TokenKind::Greater:
            return "GRE";
        case TokenKind::GreaterEqual:
            return "GEQ";
        case TokenKind::Equal:
            return "EQL";
        case TokenKind::NotEqual:
            return "NEQ";
        case TokenKind::Colon:
            return "COLON";
        case TokenKind::Semicolon:
            return "SEMICN";
        case TokenKind::Comma:
            return "COMMA";
        case TokenKind::LParen:
            return "LPARENT";
        case TokenKind::RParen:
            return "RPARENT";
        case TokenKind::LBracket:
            return "LBRACK";
        case TokenKind::RBracket:
            return "RBRACK";
        case TokenKind::LBrace:
            return "LBRACE";
        case TokenKind::RBrace:
            return "RBRACE";
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
