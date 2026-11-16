#include "parser/parser.h"

#include <utility>

namespace parser {

Parser::Parser(lexer::Lexer& lexer, std::ostream* out, SyntaxErrorCallback on_error)
    : lexer_(lexer), out_(out), on_error_(std::move(on_error)) {
    current_ = lexer_.next_token();
    if (current_.kind == lexer::TokenKind::Error) {
        ok_ = false;  // 词法错误已由 Lexer 回调上报
    }
}

bool Parser::parse_program() {
    if (current_.kind == lexer::TokenKind::KeywordConst) {
        parse_constant_declaration();
    }
    // 临时：其余部分在后续 TODO 中实现，先消费以保证演示可跑通。
    while (ok_ && !at_end()) {
        advance();
    }
    return ok_;
}

bool Parser::parse_constant_declaration() {
    do {
        if (!expect(lexer::TokenKind::KeywordConst, "const")) {
            return false;
        }
        if (!parse_constant_definition()) {
            return false;
        }
        if (!expect(lexer::TokenKind::Semicolon, "分号")) {
            return false;
        }
    } while (ok_ && current_.kind == lexer::TokenKind::KeywordConst);

    marker("常量说明");
    return ok_;
}

bool Parser::parse_constant_definition() {
    if (match(lexer::TokenKind::KeywordInt)) {
        do {
            if (!expect(lexer::TokenKind::Identifier, "标识符")) {
                return false;
            }
            if (!expect(lexer::TokenKind::Assign, "=")) {
                return false;
            }
            if (!parse_integer()) {
                return false;
            }
        } while (ok_ && match(lexer::TokenKind::Comma));
        marker("常量定义");
        return ok_;
    }

    if (match(lexer::TokenKind::KeywordChar)) {
        do {
            if (!expect(lexer::TokenKind::Identifier, "标识符")) {
                return false;
            }
            if (!expect(lexer::TokenKind::Assign, "=")) {
                return false;
            }
            if (!expect(lexer::TokenKind::CharConstant, "字符常量")) {
                return false;
            }
        } while (ok_ && match(lexer::TokenKind::Comma));
        marker("常量定义");
        return ok_;
    }

    syntax_error(current_, "期望 int 或 char");
    return false;
}

bool Parser::parse_integer() {
    if (current_.kind == lexer::TokenKind::Plus ||
        current_.kind == lexer::TokenKind::Minus) {
        advance();
    }
    if (!parse_unsigned_integer()) {
        return false;
    }
    marker("整数");
    return ok_;
}

bool Parser::parse_unsigned_integer() {
    if (!expect(lexer::TokenKind::IntConstant, "无符号整数")) {
        return false;
    }
    marker("无符号整数");
    return ok_;
}

bool Parser::parse_character() {
    return expect(lexer::TokenKind::CharConstant, "字符常量");
}

bool Parser::parse_constant() {
    if (current_.kind == lexer::TokenKind::IntConstant ||
        current_.kind == lexer::TokenKind::Plus ||
        current_.kind == lexer::TokenKind::Minus) {
        if (!parse_integer()) {
            return false;
        }
    } else if (current_.kind == lexer::TokenKind::CharConstant) {
        if (!parse_character()) {
            return false;
        }
    } else {
        syntax_error(current_, "期望常量");
        return false;
    }

    marker("常量");
    return ok_;
}

bool Parser::advance() {
    if (!ok_) {
        return false;
    }
    if (current_.kind != lexer::TokenKind::End && out_ != nullptr) {
        *out_ << lexer::token_code(current_.kind) << ' ' << current_.lexeme << '\n';
    }
    current_ = lexer_.next_token();
    if (current_.kind == lexer::TokenKind::Error) {
        ok_ = false;  // 词法错误已由 Lexer 回调上报
        return false;
    }
    return true;
}

bool Parser::match(lexer::TokenKind kind) {
    if (current_.kind != kind) {
        return false;
    }
    return advance();
}

bool Parser::expect(lexer::TokenKind kind, const std::string& context) {
    if (match(kind)) {
        return true;
    }
    syntax_error(current_, "期望" + context);
    return false;
}

void Parser::marker(const std::string& name) {
    if (ok_ && out_ != nullptr) {
        *out_ << '<' << name << ">\n";
    }
}

void Parser::syntax_error(const lexer::Token& token, const std::string& message) {
    if (!ok_) {
        return;
    }
    ok_ = false;
    if (!on_error_) {
        return;
    }
    common::Diagnostic diagnostic;
    diagnostic.line = token.line;
    diagnostic.column = token.column;
    diagnostic.code = '\0';  // 语法错误：实验三再分配类别码
    diagnostic.message = message;
    on_error_(diagnostic);
}

}  // namespace parser
