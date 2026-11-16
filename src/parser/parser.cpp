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
    // P2-1：框架阶段只消费单词，验证“消费时输出”的顺序。
    while (ok_ && !at_end()) {
        advance();
    }
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
