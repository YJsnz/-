#include "parser/parser.h"

#include <utility>

namespace parser {
namespace {

std::string to_lower_ascii(const std::string& text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (char c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
        lowered.push_back(c);
    }
    return lowered;
}

}  // namespace

Parser::Parser(lexer::Lexer& lexer, std::ostream* out, SyntaxErrorCallback on_error,
               bool recovery)
    : lexer_(lexer), out_(out), on_error_(std::move(on_error)), recovery_(recovery) {
    fill_to(0);
    if (!recovery_ && current().kind == lexer::TokenKind::Error) {
        ok_ = false;  // 词法错误已由 Lexer 回调上报
    }
}

void Parser::fill_to(std::size_t k) {
    while (lookahead_.size() <= k && !eof_) {
        lexer::Token token = lexer_.next_token();
        if (token.kind == lexer::TokenKind::Error) {
            if (!recovery_) {
                ok_ = false;
                lookahead_.push_back(token);
            }
            // 恢复模式：词法错误已由 Lexer 回调上报，丢弃该单元继续读取。
            continue;
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

void Parser::define_symbol(const std::string& name, semantic::SymbolKind kind,
                           semantic::DataType type, int dimensions,
                           const lexer::Token& where) {
    if (!recovery_) {
        return;  // 非检查模式不做语义记录
    }
    semantic::Symbol symbol;
    symbol.name = name;
    symbol.kind = kind;
    symbol.type = type;
    symbol.dimensions = dimensions;
    if (!symbols_.insert(symbol)) {
        syntax_error('b', where, "名字重定义：" + name);
    }
}

const semantic::Symbol* Parser::lookup_or_report(const lexer::Token& token,
                                                 bool require_function) {
    if (!recovery_) {
        return nullptr;  // 非检查模式不做语义检查
    }
    const semantic::Symbol* symbol = symbols_.lookup(token.value);
    if (symbol == nullptr) {
        syntax_error('c', token, "未定义的名字：" + token.value);
        return nullptr;
    }
    if (require_function && symbol->kind != semantic::SymbolKind::Function) {
        syntax_error('c', token, "名字不是函数：" + token.value);
        return nullptr;
    }
    return symbol;
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
        if (!recovery_) {
            return false;
        }
        while (ok_ && !at_end()) {
            advance();
        }
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
    previous_ = token;
    has_previous_ = true;
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
    const lexer::Token& where = has_previous_ ? previous_ : current();
    syntax_error(error_code_for(kind), where, "期望" + context);
    return recovery_;
}

char Parser::error_code_for(lexer::TokenKind expected) const {
    switch (expected) {
        case lexer::TokenKind::Semicolon:
            return 'k';
        case lexer::TokenKind::RParen:
            return 'l';
        case lexer::TokenKind::RBracket:
            return 'm';
        default:
            return 'a';  // 未分类的语法错误暂记为 a
    }
}

void Parser::marker(const std::string& name) {
    if (ok_ && out_ != nullptr) {
        *out_ << '<' << name << ">\n";
    }
}

void Parser::syntax_error(const lexer::Token& token, const std::string& message) {
    syntax_error('a', token, message);
}

void Parser::syntax_error(char code, const lexer::Token& token,
                          const std::string& message) {
    if (recovery_) {
        // 同一位置重复报错：强制前进，避免死循环。
        if (has_last_error_ && last_error_line_ == token.line &&
            last_error_column_ == token.column) {
            if (current().kind != lexer::TokenKind::End) {
                advance();
            }
            return;
        }
        had_error_ = true;
        has_last_error_ = true;
        last_error_line_ = token.line;
        last_error_column_ = token.column;
    } else {
        if (!ok_) {
            return;
        }
        ok_ = false;
        had_error_ = true;
    }

    if (!on_error_) {
        return;
    }
    common::Diagnostic diagnostic;
    diagnostic.line = token.line;
    diagnostic.column = token.column;
    diagnostic.code = code;
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
            const lexer::Token name_token = current();
            if (!expect(lexer::TokenKind::Identifier, "标识符")) {
                return false;
            }
            if (!expect(lexer::TokenKind::Assign, "=")) {
                return false;
            }
            if (!parse_integer()) {
                return false;
            }
            define_symbol(name_token.value, semantic::SymbolKind::Constant,
                          semantic::DataType::Int, 0, name_token);
        } while (ok_ && match(lexer::TokenKind::Comma));
        marker("常量定义");
        return ok_;
    }

    if (match(lexer::TokenKind::KeywordChar)) {
        do {
            const lexer::Token name_token = current();
            if (!expect(lexer::TokenKind::Identifier, "标识符")) {
                return false;
            }
            if (!expect(lexer::TokenKind::Assign, "=")) {
                return false;
            }
            if (!expect(lexer::TokenKind::CharConstant, "字符常量")) {
                return false;
            }
            define_symbol(name_token.value, semantic::SymbolKind::Constant,
                          semantic::DataType::Char, 0, name_token);
        } while (ok_ && match(lexer::TokenKind::Comma));
        marker("常量定义");
        return ok_;
    }

    syntax_error(current(), "期望 int 或 char");
    if (recovery_ && current().kind != lexer::TokenKind::End) {
        advance();
    }
    return recovery_;
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
    if (match(lexer::TokenKind::KeywordInt)) {
        last_type_ = semantic::DataType::Int;
        return true;
    }
    if (match(lexer::TokenKind::KeywordChar)) {
        last_type_ = semantic::DataType::Char;
        return true;
    }
    syntax_error(current(), "期望类型标识符 int 或 char");
    return false;
}

bool Parser::parse_declarator(int& dimensions) {
    dimensions = 0;
    last_declarator_token_ = current();
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
    const semantic::DataType type = last_type_;

    int dimensions = 0;
    if (!parse_declarator(dimensions)) {
        return false;
    }
    define_symbol(last_declarator_token_.value,
                  dimensions > 0 ? semantic::SymbolKind::Array
                                 : semantic::SymbolKind::Variable,
                  type, dimensions, last_declarator_token_);

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
        define_symbol(last_declarator_token_.value,
                      more_dimensions > 0 ? semantic::SymbolKind::Array
                                          : semantic::SymbolKind::Variable,
                      type, more_dimensions, last_declarator_token_);
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
    last_parameters_.clear();
    if (current().kind == lexer::TokenKind::KeywordInt ||
        current().kind == lexer::TokenKind::KeywordChar) {
        do {
            if (!parse_type_identifier()) {
                return false;
            }
            const semantic::DataType type = last_type_;
            const lexer::Token name_token = current();
            if (!expect(lexer::TokenKind::Identifier, "标识符")) {
                return false;
            }
            ParameterInfo parameter;
            parameter.token = name_token;
            parameter.type = type;
            last_parameters_.push_back(parameter);
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

    if (current().kind == lexer::TokenKind::KeywordWhile ||
        current().kind == lexer::TokenKind::KeywordFor) {
        return parse_loop_statement();
    }

    if (current().kind == lexer::TokenKind::KeywordSwitch) {
        return parse_switch_statement();
    }

    if (current().kind == lexer::TokenKind::Identifier &&
        peek_token(1).kind == lexer::TokenKind::LParen) {
        const std::string name = to_lower_ascii(current().value);
        bool returns_value = true;  // 未定义函数暂按有返回值处理
        const auto it = function_returns_.find(name);
        if (it != function_returns_.end()) {
            returns_value = it->second;
        }
        if (!parse_call_statement(returns_value)) {
            return false;
        }
        if (!expect(lexer::TokenKind::Semicolon, "分号")) {
            return false;
        }
        marker("语句");
        return ok_;
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

    syntax_error('a', current(), "无法识别的语句");
    if (recovery_ && current().kind != lexer::TokenKind::End) {
        advance();
    }
    return recovery_;
}

bool Parser::parse_assignment_statement() {
    const lexer::Token name_token = current();
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    (void)lookup_or_report(name_token);

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
    const lexer::Token name_token = current();
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    (void)lookup_or_report(name_token);
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
    const lexer::Token return_token = current();
    advance();  // return
    bool has_parentheses = false;
    if (ok_ && match(lexer::TokenKind::LParen)) {
        has_parentheses = true;
        if (ok_ && current().kind != lexer::TokenKind::RParen) {
            if (!parse_expression()) {
                return false;
            }
        }
        if (!expect(lexer::TokenKind::RParen, ")")) {
            return false;
        }
    }

    if (recovery_ && has_parentheses &&
        current_return_type_ == semantic::DataType::Void) {
        syntax_error('g', return_token, "无返回值函数中出现带括号的 return");
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
        const lexer::Token name_token = current();
        const std::string name = to_lower_ascii(name_token.value);
        if (!expect(lexer::TokenKind::Identifier, "标识符")) {
            return false;
        }
        function_returns_[name] = false;
        if (!expect(lexer::TokenKind::LParen, "(")) {
            return false;
        }
        if (!parse_parameter_list()) {
            return false;
        }

        semantic::Symbol function;
        function.name = name_token.value;
        function.kind = semantic::SymbolKind::Function;
        function.type = semantic::DataType::Void;
        for (const ParameterInfo& parameter : last_parameters_) {
            function.parameter_types.push_back(parameter.type);
        }
        if (recovery_ && !symbols_.insert(function)) {
            syntax_error('b', name_token, "名字重定义：" + name_token.value);
        }

        symbols_.enter_scope();
        for (const ParameterInfo& parameter : last_parameters_) {
            define_symbol(parameter.token.value, semantic::SymbolKind::Variable,
                          parameter.type, 0, parameter.token);
        }

        if (!expect(lexer::TokenKind::RParen, ")")) {
            symbols_.leave_scope();
            return false;
        }
        const semantic::DataType saved_return_type = current_return_type_;
        current_return_type_ = semantic::DataType::Void;
        const bool body_ok = parse_compound_statement_body();
        current_return_type_ = saved_return_type;
        symbols_.leave_scope();
        if (!body_ok) {
            return false;
        }
        marker("无返回值函数定义");
        return ok_;
    }

    const lexer::Token name_token = peek_token(1);
    const std::string name = to_lower_ascii(name_token.value);
    if (!parse_declaration_header()) {
        return false;
    }
    const semantic::DataType return_type = last_type_;
    function_returns_[name] = true;
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }
    if (!parse_parameter_list()) {
        return false;
    }

    semantic::Symbol function;
    function.name = name_token.value;
    function.kind = semantic::SymbolKind::Function;
    function.type = return_type;
    for (const ParameterInfo& parameter : last_parameters_) {
        function.parameter_types.push_back(parameter.type);
    }
    if (recovery_ && !symbols_.insert(function)) {
        syntax_error('b', name_token, "名字重定义：" + name_token.value);
    }

    symbols_.enter_scope();
    for (const ParameterInfo& parameter : last_parameters_) {
        define_symbol(parameter.token.value, semantic::SymbolKind::Variable,
                      parameter.type, 0, parameter.token);
    }

    if (!expect(lexer::TokenKind::RParen, ")")) {
        symbols_.leave_scope();
        return false;
    }
    const semantic::DataType saved_return_type = current_return_type_;
    current_return_type_ = return_type;
    const bool body_ok = parse_compound_statement_body();
    current_return_type_ = saved_return_type;
    symbols_.leave_scope();
    if (!body_ok) {
        return false;
    }
    marker("有返回值函数定义");
    return ok_;
}

bool Parser::parse_main_function() {
    if (current().kind != lexer::TokenKind::KeywordVoid ||
        peek_token(1).kind != lexer::TokenKind::KeywordMain) {
        syntax_error(current(), "期望主函数 void main()");
        return recovery_;
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

    symbols_.enter_scope();
    const semantic::DataType saved_return_type = current_return_type_;
    current_return_type_ = semantic::DataType::Void;
    const bool body_ok = parse_compound_statement_body();
    current_return_type_ = saved_return_type;
    symbols_.leave_scope();
    if (!body_ok) {
        return false;
    }

    marker("主函数");
    return ok_;
}

// ---------------------------------------------------------------- 表达式

bool Parser::parse_expression() {
    bool has_unary_sign = false;
    if (current().kind == lexer::TokenKind::Plus ||
        current().kind == lexer::TokenKind::Minus) {
        advance();
        has_unary_sign = true;
    }
    if (!parse_term()) {
        return false;
    }
    semantic::DataType type =
        has_unary_sign ? semantic::DataType::Int : last_term_type_;
    while (ok_ && (current().kind == lexer::TokenKind::Plus ||
                   current().kind == lexer::TokenKind::Minus)) {
        advance();
        if (!parse_term()) {
            return false;
        }
        type = semantic::DataType::Int;
    }
    last_expression_type_ = type;
    marker("表达式");
    return ok_;
}

bool Parser::parse_term() {
    if (!parse_factor()) {
        return false;
    }
    semantic::DataType type = last_factor_type_;
    while (ok_ && (current().kind == lexer::TokenKind::Star ||
                   current().kind == lexer::TokenKind::Slash)) {
        advance();
        if (!parse_factor()) {
            return false;
        }
        type = semantic::DataType::Int;
    }
    last_term_type_ = type;
    marker("项");
    return ok_;
}

bool Parser::parse_factor() {
    bool ok = false;

    if (current().kind == lexer::TokenKind::Identifier &&
        peek_token(1).kind == lexer::TokenKind::LParen) {
        ok = parse_function_call();
        if (ok) {
            last_factor_type_ = last_call_return_type_;
        }
    } else if (current().kind == lexer::TokenKind::Identifier) {
        const lexer::Token name_token = current();
        if (!advance()) {
            return false;
        }
        const semantic::Symbol* symbol = lookup_or_report(name_token);
        last_factor_type_ = symbol != nullptr ? symbol->type
                                             : semantic::DataType::Unknown;
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
        if (ok) {
            last_factor_type_ = semantic::DataType::Int;
        }
    } else if (current().kind == lexer::TokenKind::CharConstant) {
        ok = parse_character();
        if (ok) {
            last_factor_type_ = semantic::DataType::Char;
        }
    } else if (current().kind == lexer::TokenKind::LParen) {
        advance();
        if (!parse_expression()) {
            return false;
        }
        ok = expect(lexer::TokenKind::RParen, "右小括号 )");
        if (ok) {
            // 括号内的字符型也视为参与运算，结果为整型。
            last_factor_type_ = semantic::DataType::Int;
        }
    } else {
        syntax_error(current(), "期望因子");
        if (recovery_ && current().kind != lexer::TokenKind::End) {
            const lexer::TokenKind kind = current().kind;
            // 分号/右括号等是上层同步点，不要吞掉。
            if (kind != lexer::TokenKind::Semicolon &&
                kind != lexer::TokenKind::RBrace &&
                kind != lexer::TokenKind::RParen &&
                kind != lexer::TokenKind::RBracket) {
                advance();
            }
        }
        return recovery_;
    }

    if (!ok) {
        return false;
    }
    marker("因子");
    return ok_;
}

bool Parser::parse_value_parameter_list() {
    last_argument_count_ = 0;
    last_argument_types_.clear();
    if (current().kind != lexer::TokenKind::RParen) {
        if (!parse_expression()) {
            return false;
        }
        ++last_argument_count_;
        last_argument_types_.push_back(last_expression_type_);
        while (ok_ && match(lexer::TokenKind::Comma)) {
            if (!parse_expression()) {
                return false;
            }
            ++last_argument_count_;
            last_argument_types_.push_back(last_expression_type_);
        }
    }

    marker("值参数表");
    return ok_;
}

bool Parser::parse_function_call() {
    const lexer::Token name_token = current();
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    const semantic::Symbol* symbol = lookup_or_report(name_token, true);
    last_call_return_type_ =
        symbol != nullptr ? symbol->type : semantic::DataType::Unknown;
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }
    if (!parse_value_parameter_list()) {
        return false;
    }
    if (recovery_ && symbol != nullptr) {
        if (last_argument_count_ !=
            static_cast<int>(symbol->parameter_types.size())) {
            syntax_error('d', name_token, "函数参数个数不匹配：" + name_token.value);
        } else {
            bool mismatch = false;
            for (std::size_t i = 0; i < last_argument_types_.size(); ++i) {
                const semantic::DataType actual = last_argument_types_[i];
                const semantic::DataType expected = symbol->parameter_types[i];
                if (actual != semantic::DataType::Unknown &&
                    expected != semantic::DataType::Unknown && actual != expected) {
                    mismatch = true;
                }
            }
            if (mismatch) {
                syntax_error('e', name_token,
                             "函数参数类型不匹配：" + name_token.value);
            }
        }
    }
    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }

    marker("有返回值函数调用语句");
    return ok_;
}

bool Parser::parse_call_statement(bool returns_value) {
    const lexer::Token name_token = current();
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    const semantic::Symbol* symbol = lookup_or_report(name_token, true);
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }
    if (!parse_value_parameter_list()) {
        return false;
    }
    if (recovery_ && symbol != nullptr) {
        if (last_argument_count_ !=
            static_cast<int>(symbol->parameter_types.size())) {
            syntax_error('d', name_token, "函数参数个数不匹配：" + name_token.value);
        } else {
            bool mismatch = false;
            for (std::size_t i = 0; i < last_argument_types_.size(); ++i) {
                const semantic::DataType actual = last_argument_types_[i];
                const semantic::DataType expected = symbol->parameter_types[i];
                if (actual != semantic::DataType::Unknown &&
                    expected != semantic::DataType::Unknown && actual != expected) {
                    mismatch = true;
                }
            }
            if (mismatch) {
                syntax_error('e', name_token,
                             "函数参数类型不匹配：" + name_token.value);
            }
        }
    }
    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }

    marker(returns_value ? "有返回值函数调用语句" : "无返回值函数调用语句");
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
    const semantic::DataType left_type = last_expression_type_;

    const lexer::Token operator_token = current();
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
    const semantic::DataType right_type = last_expression_type_;

    if (recovery_ && (left_type == semantic::DataType::Char ||
                      right_type == semantic::DataType::Char)) {
        syntax_error('f', operator_token, "条件判断中的表达式只能为整型");
    }

    marker("条件");
    return ok_;
}

bool Parser::parse_loop_statement() {
    if (current().kind == lexer::TokenKind::KeywordWhile) {
        return parse_while_statement();
    }
    if (current().kind == lexer::TokenKind::KeywordFor) {
        return parse_for_statement();
    }
    syntax_error(current(), "期望 while 或 for");
    return false;
}

bool Parser::parse_while_statement() {
    advance();  // while
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

    marker("循环语句");
    return ok_;
}

bool Parser::parse_for_statement() {
    advance();  // for
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }

    // 初始化：标识符 = 表达式
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    if (!expect(lexer::TokenKind::Assign, "=")) {
        return false;
    }
    if (!parse_expression()) {
        return false;
    }
    if (!expect(lexer::TokenKind::Semicolon, "分号")) {
        return false;
    }

    // 条件
    if (!parse_condition()) {
        return false;
    }
    if (!expect(lexer::TokenKind::Semicolon, "分号")) {
        return false;
    }

    // 步长更新：标识符 = 标识符 (+|-) 步长
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    if (!expect(lexer::TokenKind::Assign, "=")) {
        return false;
    }
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    if (current().kind != lexer::TokenKind::Plus &&
        current().kind != lexer::TokenKind::Minus) {
        syntax_error(current(), "期望 + 或 -");
        return false;
    }
    advance();
    if (!parse_step()) {
        return false;
    }

    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }
    if (!parse_statement()) {
        return false;
    }

    marker("循环语句");
    return ok_;
}

bool Parser::parse_step() {
    if (!parse_unsigned_integer()) {
        return false;
    }
    marker("步长");
    return ok_;
}

bool Parser::parse_switch_statement() {
    advance();  // switch
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }
    if (!parse_expression()) {
        return false;
    }
    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }
    if (!expect(lexer::TokenKind::LBrace, "{")) {
        return false;
    }
    if (!parse_case_table()) {
        return false;
    }
    if (!parse_default_clause()) {
        return false;
    }
    if (!expect(lexer::TokenKind::RBrace, "右花括号 }")) {
        return false;
    }

    marker("情况语句");
    return ok_;
}

bool Parser::parse_case_table() {
    if (current().kind != lexer::TokenKind::KeywordCase) {
        syntax_error(current(), "期望 case");
        return false;
    }

    do {
        if (!parse_case_clause()) {
            return false;
        }
    } while (ok_ && current().kind == lexer::TokenKind::KeywordCase);

    marker("情况表");
    return ok_;
}

bool Parser::parse_case_clause() {
    advance();  // case
    if (!parse_constant()) {
        return false;
    }
    if (!expect(lexer::TokenKind::Colon, "冒号 :")) {
        return false;
    }
    if (!parse_statement()) {
        return false;
    }

    marker("情况子语句");
    return ok_;
}

bool Parser::parse_default_clause() {
    if (!expect(lexer::TokenKind::KeywordDefault, "default")) {
        return false;
    }
    if (!expect(lexer::TokenKind::Colon, "冒号 :")) {
        return false;
    }
    if (!parse_statement()) {
        return false;
    }

    marker("缺省");
    return ok_;
}

}  // namespace parser
