#include "parser/parser.h"

#include <utility>

namespace parser {

Parser::Parser(lexer::Lexer& lexer, std::ostream* out, SyntaxErrorCallback on_error)
    : lexer_(lexer), out_(out), on_error_(std::move(on_error)) {
    fill_to(0);
    if (current().kind == lexer::TokenKind::Error) {
        ok_ = false;  // 词法错误已由 Lexer 回调上报
    }
}

void Parser::fill_to(std::size_t k) {
    while (lookahead_.size() <= k && !eof_) {
        lexer::Token token = lexer_.next_token();
        if (token.kind == lexer::TokenKind::Error) {
            ok_ = false;  // 词法错误已由 Lexer 回调上报
        }
        if (token.kind == lexer::TokenKind::End) {
            eof_ = true;
        }
        lookahead_.push_back(token);
    }
}

const lexer::Token& Parser::peek_token(std::size_t k) {
    fill_to(k);
    if (k >= lookahead_.size()) {
        return lookahead_.back();  // 文件尾之后统一返回 End
    }
    return lookahead_[k];
}

bool Parser::parse_program() {
    if (current().kind == lexer::TokenKind::KeywordConst) {
        parse_constant_declaration();
    }
    if (ok_ && can_start_variable()) {
        parse_variable_declaration();
    }
    while (ok_ && is_function_definition_start()) {
        parse_function_definition();
    }
    // 临时：其余部分在后续 TODO 中实现，先消费以保证演示可跑通。
    while (ok_ && !at_end()) {
        advance();
    }
    return ok_;
}

bool Parser::advance() {
    if (!ok_ || current().kind == lexer::TokenKind::End) {
        return false;
    }

    const lexer::Token token = current();
    if (out_ != nullptr) {
        *out_ << lexer::token_code(token.kind) << ' ' << token.lexeme << '\n';
    }
    lookahead_.pop_front();
    fill_to(0);

    if (current().kind == lexer::TokenKind::Error) {
        ok_ = false;
        return false;
    }
    return true;
}

bool Parser::match(lexer::TokenKind kind) {
    if (current().kind != kind) {
        return false;
    }
    return advance();
}

bool Parser::expect(lexer::TokenKind kind, const std::string& context) {
    if (match(kind)) {
        return true;
    }
    syntax_error(current(), "期望" + context);
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

// ---------------------------------------------------------------- 常量

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
    } while (ok_ && current().kind == lexer::TokenKind::KeywordConst);

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

    syntax_error(current(), "期望 int 或 char");
    return false;
}

bool Parser::parse_integer() {
    if (current().kind == lexer::TokenKind::Plus ||
        current().kind == lexer::TokenKind::Minus) {
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
    if (current().kind == lexer::TokenKind::IntConstant ||
        current().kind == lexer::TokenKind::Plus ||
        current().kind == lexer::TokenKind::Minus) {
        if (!parse_integer()) {
            return false;
        }
    } else if (current().kind == lexer::TokenKind::CharConstant) {
        if (!parse_character()) {
            return false;
        }
    } else {
        syntax_error(current(), "期望常量");
        return false;
    }

    marker("常量");
    return ok_;
}

// ---------------------------------------------------------------- 变量

bool Parser::parse_type_identifier() {
    if (match(lexer::TokenKind::KeywordInt) ||
        match(lexer::TokenKind::KeywordChar)) {
        return true;
    }
    syntax_error(current(), "期望类型标识符 int 或 char");
    return false;
}

bool Parser::parse_declarator(int& dimensions) {
    dimensions = 0;
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }

    if (match(lexer::TokenKind::LBracket)) {
        if (!parse_unsigned_integer()) {
            return false;
        }
        if (!expect(lexer::TokenKind::RBracket, "右中括号 ]")) {
            return false;
        }
        dimensions = 1;

        if (match(lexer::TokenKind::LBracket)) {
            if (!parse_unsigned_integer()) {
                return false;
            }
            if (!expect(lexer::TokenKind::RBracket, "右中括号 ]")) {
                return false;
            }
            dimensions = 2;
        }
    }
    return ok_;
}

bool Parser::parse_variable_initializer(int dimensions) {
    if (dimensions == 0) {
        return parse_constant();
    }

    if (!expect(lexer::TokenKind::LBrace, "{")) {
        return false;
    }

    auto parse_constant_list = [this]() -> bool {
        if (!parse_constant()) {
            return false;
        }
        while (ok_ && match(lexer::TokenKind::Comma)) {
            if (!parse_constant()) {
                return false;
            }
        }
        return ok_;
    };

    if (dimensions == 1) {
        if (!parse_constant_list()) {
            return false;
        }
        return expect(lexer::TokenKind::RBrace, "}");
    }

    // 二维：{ { consts } {, { consts }} }
    if (!expect(lexer::TokenKind::LBrace, "{")) {
        return false;
    }
    if (!parse_constant_list()) {
        return false;
    }
    if (!expect(lexer::TokenKind::RBrace, "}")) {
        return false;
    }

    while (ok_ && match(lexer::TokenKind::Comma)) {
        if (!expect(lexer::TokenKind::LBrace, "{")) {
            return false;
        }
        if (!parse_constant_list()) {
            return false;
        }
        if (!expect(lexer::TokenKind::RBrace, "}")) {
            return false;
        }
    }

    return expect(lexer::TokenKind::RBrace, "}");
}

bool Parser::parse_variable_definition() {
    if (!parse_type_identifier()) {
        return false;
    }

    int dimensions = 0;
    if (!parse_declarator(dimensions)) {
        return false;
    }

    if (current().kind == lexer::TokenKind::Assign) {
        advance();  // '='
        if (!parse_variable_initializer(dimensions)) {
            return false;
        }
        marker("变量定义及初始化");
        marker("变量定义");
        return ok_;
    }

    // 无初始化：允许逗号分隔的多个声明符
    while (ok_ && match(lexer::TokenKind::Comma)) {
        int more_dimensions = 0;
        if (!parse_declarator(more_dimensions)) {
            return false;
        }
    }

    marker("变量定义无初始化");
    marker("变量定义");
    return ok_;
}

bool Parser::can_start_variable() {
    if (current().kind != lexer::TokenKind::KeywordInt &&
        current().kind != lexer::TokenKind::KeywordChar) {
        return false;
    }
    if (peek_token(1).kind != lexer::TokenKind::Identifier) {
        return false;
    }
    // 类型 名字 ( 是函数定义，不是变量定义。
    return peek_token(2).kind != lexer::TokenKind::LParen;
}

bool Parser::parse_variable_declaration() {
    if (!parse_variable_definition()) {
        return false;
    }
    if (!expect(lexer::TokenKind::Semicolon, "分号")) {
        return false;
    }

    while (ok_ && can_start_variable()) {
        if (!parse_variable_definition()) {
            return false;
        }
        if (!expect(lexer::TokenKind::Semicolon, "分号")) {
            return false;
        }
    }

    marker("变量说明");
    return ok_;
}

// ---------------------------------------------------------------- 函数

bool Parser::parse_declaration_header() {
    if (!parse_type_identifier()) {
        return false;
    }
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    marker("声明头部");
    return ok_;
}

bool Parser::parse_parameter_list() {
    if (current().kind == lexer::TokenKind::KeywordInt ||
        current().kind == lexer::TokenKind::KeywordChar) {
        do {
            if (!parse_type_identifier()) {
                return false;
            }
            if (!expect(lexer::TokenKind::Identifier, "标识符")) {
                return false;
            }
        } while (ok_ && match(lexer::TokenKind::Comma));
    }

    marker("参数表");
    return ok_;
}

bool Parser::parse_compound_statement_body() {
    // 临时：P2-6 将替换为 <复合语句> 的真实解析。
    if (!expect(lexer::TokenKind::LBrace, "{")) {
        return false;
    }

    int depth = 1;
    while (ok_ && depth > 0) {
        if (at_end()) {
            syntax_error(current(), "复合语句缺少右花括号 }");
            return false;
        }
        if (current().kind == lexer::TokenKind::LBrace) {
            ++depth;
        } else if (current().kind == lexer::TokenKind::RBrace) {
            --depth;
        }
        if (!advance()) {
            return false;
        }
    }
    return ok_;
}

bool Parser::is_function_definition_start() {
    if (current().kind != lexer::TokenKind::KeywordVoid &&
        current().kind != lexer::TokenKind::KeywordInt &&
        current().kind != lexer::TokenKind::KeywordChar) {
        return false;
    }
    return peek_token(1).kind == lexer::TokenKind::Identifier &&
           peek_token(2).kind == lexer::TokenKind::LParen;
}

bool Parser::parse_function_definition() {
    if (current().kind == lexer::TokenKind::KeywordVoid) {
        advance();  // void
        if (!expect(lexer::TokenKind::Identifier, "标识符")) {
            return false;
        }
        if (!expect(lexer::TokenKind::LParen, "(")) {
            return false;
        }
        if (!parse_parameter_list()) {
            return false;
        }
        if (!expect(lexer::TokenKind::RParen, ")")) {
            return false;
        }
        if (!parse_compound_statement_body()) {
            return false;
        }
        marker("无返回值函数定义");
        return ok_;
    }

    if (!parse_declaration_header()) {
        return false;
    }
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }
    if (!parse_parameter_list()) {
        return false;
    }
    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }
    if (!parse_compound_statement_body()) {
        return false;
    }
    marker("有返回值函数定义");
    return ok_;
}

}  // namespace parser
