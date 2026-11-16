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
    if (!ok_ || !parse_main_function()) {
        return false;
    }
    marker("程序");
    if (ok_ && !at_end()) {
        syntax_error(current(), "主函数之后存在多余内容");
        return false;
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
    if (!expect(lexer::TokenKind::LBrace, "{")) {
        return false;
    }
    if (!parse_compound_statement()) {
        return false;
    }
    return expect(lexer::TokenKind::RBrace, "右花括号 }");
}

bool Parser::parse_compound_statement() {
    if (current().kind == lexer::TokenKind::KeywordConst) {
        parse_constant_declaration();
    }
    if (ok_ && can_start_variable()) {
        parse_variable_declaration();
    }
    if (ok_ && !parse_statement_list()) {
        return false;
    }

    marker("复合语句");
    return ok_;
}

bool Parser::parse_statement_list() {
    while (ok_ && !at_end() && current().kind != lexer::TokenKind::RBrace) {
        if (!parse_statement()) {
            return false;
        }
    }
    if (!ok_) {
        return false;
    }

    marker("语句列");
    return true;
}

bool Parser::parse_statement() {
    if (current().kind == lexer::TokenKind::LBrace) {
        advance();  // '{'
        if (!parse_statement_list()) {
            return false;
        }
        if (!expect(lexer::TokenKind::RBrace, "右花括号 }")) {
            return false;
        }
        marker("语句");
        return ok_;
    }

    if (current().kind == lexer::TokenKind::Semicolon) {
        advance();
        marker("语句");
        return ok_;
    }

    if (current().kind == lexer::TokenKind::KeywordReturn) {
        if (!parse_return_statement()) {
            return false;
        }
        if (!expect(lexer::TokenKind::Semicolon, "分号")) {
            return false;
        }
        marker("语句");
        return ok_;
    }

    if (current().kind == lexer::TokenKind::KeywordScanf) {
        if (!parse_read_statement()) {
            return false;
        }
        if (!expect(lexer::TokenKind::Semicolon, "分号")) {
            return false;
        }
        marker("语句");
        return ok_;
    }

    if (current().kind == lexer::TokenKind::KeywordPrintf) {
        if (!parse_write_statement()) {
            return false;
        }
        if (!expect(lexer::TokenKind::Semicolon, "分号")) {
            return false;
        }
        marker("语句");
        return ok_;
    }

    if (current().kind == lexer::TokenKind::KeywordIf) {
        return parse_conditional_statement();
    }

    if (current().kind == lexer::TokenKind::Identifier) {
        if (!parse_assignment_statement()) {
            return false;
        }
        if (!expect(lexer::TokenKind::Semicolon, "分号")) {
            return false;
        }
        marker("语句");
        return ok_;
    }

    syntax_error(current(), "暂不支持的语句（控制语句将在 P2-9～P2-11 实现）");
    return false;
}

bool Parser::parse_assignment_statement() {
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }

    if (ok_ && match(lexer::TokenKind::LBracket)) {
        if (!parse_expression()) {
            return false;
        }
        if (!expect(lexer::TokenKind::RBracket, "右中括号 ]")) {
            return false;
        }
        if (ok_ && match(lexer::TokenKind::LBracket)) {
            if (!parse_expression()) {
                return false;
            }
            if (!expect(lexer::TokenKind::RBracket, "右中括号 ]")) {
                return false;
            }
        }
    }

    if (!expect(lexer::TokenKind::Assign, "=")) {
        return false;
    }
    if (!parse_expression()) {
        return false;
    }

    marker("赋值语句");
    return ok_;
}

bool Parser::parse_read_statement() {
    advance();  // scanf
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }

    marker("读语句");
    return ok_;
}

bool Parser::parse_write_statement() {
    advance();  // printf
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }

    if (current().kind == lexer::TokenKind::StringConstant) {
        advance();
        marker("字符串");
        if (ok_ && match(lexer::TokenKind::Comma)) {
            if (!parse_expression()) {
                return false;
            }
        }
    } else {
        if (!parse_expression()) {
            return false;
        }
    }

    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }

    marker("写语句");
    return ok_;
}

bool Parser::parse_return_statement() {
    advance();  // return
    if (ok_ && match(lexer::TokenKind::LParen)) {
        if (!parse_expression()) {
            return false;
        }
        if (!expect(lexer::TokenKind::RParen, ")")) {
            return false;
        }
    }

    marker("返回语句");
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

bool Parser::parse_main_function() {
    if (current().kind != lexer::TokenKind::KeywordVoid ||
        peek_token(1).kind != lexer::TokenKind::KeywordMain) {
        syntax_error(current(), "期望主函数 void main()");
        return false;
    }

    advance();  // void
    if (!expect(lexer::TokenKind::KeywordMain, "main")) {
        return false;
    }
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }
    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }
    if (!parse_compound_statement_body()) {
        return false;
    }

    marker("主函数");
    return ok_;
}

// ---------------------------------------------------------------- 表达式

bool Parser::parse_expression() {
    if (current().kind == lexer::TokenKind::Plus ||
        current().kind == lexer::TokenKind::Minus) {
        advance();
    }
    if (!parse_term()) {
        return false;
    }
    while (ok_ && (current().kind == lexer::TokenKind::Plus ||
                   current().kind == lexer::TokenKind::Minus)) {
        advance();
        if (!parse_term()) {
            return false;
        }
    }
    marker("表达式");
    return ok_;
}

bool Parser::parse_term() {
    if (!parse_factor()) {
        return false;
    }
    while (ok_ && (current().kind == lexer::TokenKind::Star ||
                   current().kind == lexer::TokenKind::Slash)) {
        advance();
        if (!parse_factor()) {
            return false;
        }
    }
    marker("项");
    return ok_;
}

bool Parser::parse_factor() {
    bool ok = false;

    if (current().kind == lexer::TokenKind::Identifier &&
        peek_token(1).kind == lexer::TokenKind::LParen) {
        ok = parse_function_call();
    } else if (current().kind == lexer::TokenKind::Identifier) {
        if (!advance()) {
            return false;
        }
        if (ok_ && current().kind == lexer::TokenKind::LBracket) {
            advance();
            if (!parse_expression()) {
                return false;
            }
            if (!expect(lexer::TokenKind::RBracket, "右中括号 ]")) {
                return false;
            }
            if (ok_ && current().kind == lexer::TokenKind::LBracket) {
                advance();
                if (!parse_expression()) {
                    return false;
                }
                if (!expect(lexer::TokenKind::RBracket, "右中括号 ]")) {
                    return false;
                }
            }
        }
        ok = ok_;
    } else if (current().kind == lexer::TokenKind::IntConstant ||
               current().kind == lexer::TokenKind::Plus ||
               current().kind == lexer::TokenKind::Minus) {
        ok = parse_integer();
    } else if (current().kind == lexer::TokenKind::CharConstant) {
        ok = parse_character();
    } else if (current().kind == lexer::TokenKind::LParen) {
        advance();
        if (!parse_expression()) {
            return false;
        }
        ok = expect(lexer::TokenKind::RParen, "右小括号 )");
    } else {
        syntax_error(current(), "期望因子");
        return false;
    }

    if (!ok) {
        return false;
    }
    marker("因子");
    return ok_;
}

bool Parser::parse_value_parameter_list() {
    if (current().kind != lexer::TokenKind::RParen) {
        if (!parse_expression()) {
            return false;
        }
        while (ok_ && match(lexer::TokenKind::Comma)) {
            if (!parse_expression()) {
                return false;
            }
        }
    }

    marker("值参数表");
    return ok_;
}

bool Parser::parse_function_call() {
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }
    if (!parse_value_parameter_list()) {
        return false;
    }
    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }

    marker("有返回值函数调用语句");
    return ok_;
}

bool Parser::parse_conditional_statement() {
    advance();  // if
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }
    if (!parse_condition()) {
        return false;
    }
    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }
    if (!parse_statement()) {
        return false;
    }
    if (ok_ && match(lexer::TokenKind::KeywordElse)) {
        if (!parse_statement()) {
            return false;
        }
    }

    marker("条件语句");
    return ok_;
}

bool Parser::parse_condition() {
    if (!parse_expression()) {
        return false;
    }

    const lexer::TokenKind kind = current().kind;
    if (kind != lexer::TokenKind::Less && kind != lexer::TokenKind::LessEqual &&
        kind != lexer::TokenKind::Greater &&
        kind != lexer::TokenKind::GreaterEqual && kind != lexer::TokenKind::Equal &&
        kind != lexer::TokenKind::NotEqual) {
        syntax_error(current(), "期望关系运算符");
        return false;
    }
    advance();

    if (!parse_expression()) {
        return false;
    }

    marker("条件");
    return ok_;
}

}  // namespace parser
