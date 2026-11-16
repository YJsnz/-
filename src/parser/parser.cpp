#include "parser/parser.h"

#include <utility>

#include "codegen/code_generator.h"

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
               bool recovery, codegen::CodeGenerator* generator)
    : lexer_(lexer),
      out_(out),
      on_error_(std::move(on_error)),
      recovery_(recovery),
      semantic_(recovery || generator != nullptr),
      generator_(generator) {
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

int Parser::allocate_cells(int cells) {
    const int base = next_address_;
    next_address_ += cells;
    return base;
}

void Parser::emit_initializer(const semantic::Symbol& symbol) {
    if (generator_ == nullptr || symbol.address < 0) {
        return;
    }
    for (std::size_t i = 0; i < last_initializer_values_.size(); ++i) {
        generator_->emit_lit(last_initializer_values_[i]);
        generator_->emit_sto(symbol.address + static_cast<int>(i));
    }
}

void Parser::define_symbol(const std::string& name, semantic::SymbolKind kind,
                           semantic::DataType type, int dimensions,
                           const lexer::Token& where) {
    if (!semantic_) {
        return;  // 非检查/代码生成模式不做语义记录
    }
    const bool pending_has_value = pending_has_value_;
    const long long pending_int_value = pending_int_value_;
    const char pending_char_value = pending_char_value_;
    pending_has_value_ = false;
    pending_int_value_ = 0;
    pending_char_value_ = 0;

    semantic::Symbol symbol;
    symbol.name = name;
    symbol.kind = kind;
    symbol.type = type;
    symbol.dimensions = dimensions;
    if (kind == semantic::SymbolKind::Constant) {
        symbol.has_value = pending_has_value;
        symbol.int_value = pending_int_value;
        symbol.char_value = pending_char_value;
    } else if (kind == semantic::SymbolKind::Variable) {
        if (pending_address_ >= 0) {
            symbol.address = pending_address_;
            pending_address_ = -1;
        } else {
            symbol.address = allocate_cells(1);
        }
    } else if (kind == semantic::SymbolKind::Array) {
        int cells = 1;
        for (long long size : last_dimensions_) {
            if (size > 0) {
                cells *= static_cast<int>(size);
            }
        }
        symbol.dimension_sizes = last_dimensions_;
        symbol.address = allocate_cells(cells);
    }
    if (!symbols_.insert(symbol)) {
        syntax_error('b', where, "名字重定义：" + name);
    }
}

const semantic::Symbol* Parser::lookup_or_report(const lexer::Token& token,
                                                 bool require_function) {
    if (!semantic_) {
        return nullptr;  // 非检查/代码生成模式不做语义检查
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
    if (ok_ && generator_ != nullptr) {
        generator_->emit_end();
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
            pending_has_value_ = true;
            pending_int_value_ = last_integer_value_;
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
            pending_has_value_ = true;
            pending_char_value_ =
                !previous_.lexeme.empty() ? previous_.lexeme[0] : 0;
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
    long long sign = 1;
    if (current().kind == lexer::TokenKind::Plus ||
        current().kind == lexer::TokenKind::Minus) {
        if (current().kind == lexer::TokenKind::Minus) {
            sign = -1;
        }
        advance();
    }
    if (!parse_unsigned_integer()) {
        return false;
    }
    last_integer_value_ = sign * last_unsigned_value_;
    marker("整数");
    return ok_;
}

bool Parser::parse_unsigned_integer() {
    if (!expect(lexer::TokenKind::IntConstant, "无符号整数")) {
        return false;
    }
    last_unsigned_value_ =
        previous_.kind == lexer::TokenKind::IntConstant ? previous_.int_value : 0;
    marker("无符号整数");
    return ok_;
}

bool Parser::parse_character() {
    if (!expect(lexer::TokenKind::CharConstant, "字符常量")) {
        return false;
    }
    last_char_value_ =
        previous_.kind == lexer::TokenKind::CharConstant && !previous_.lexeme.empty()
            ? previous_.lexeme[0]
            : 0;
    return ok_;
}

bool Parser::parse_constant() {
    if (current().kind == lexer::TokenKind::IntConstant ||
        current().kind == lexer::TokenKind::Plus ||
        current().kind == lexer::TokenKind::Minus) {
        if (!parse_integer()) {
            return false;
        }
        last_constant_type_ = semantic::DataType::Int;
    } else if (current().kind == lexer::TokenKind::CharConstant) {
        if (!parse_character()) {
            return false;
        }
        last_constant_type_ = semantic::DataType::Char;
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
    last_dimensions_.clear();
    last_declarator_token_ = current();
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }

    if (match(lexer::TokenKind::LBracket)) {
        if (!parse_unsigned_integer()) {
            return false;
        }
        last_dimensions_.push_back(previous_.int_value);
        if (!expect(lexer::TokenKind::RBracket, "右中括号 ]")) {
            return false;
        }
        dimensions = 1;

        if (match(lexer::TokenKind::LBracket)) {
            if (!parse_unsigned_integer()) {
                return false;
            }
            last_dimensions_.push_back(previous_.int_value);
            if (!expect(lexer::TokenKind::RBracket, "右中括号 ]")) {
                return false;
            }
            dimensions = 2;
        }
    }
    return ok_;
}

bool Parser::parse_variable_initializer(int dimensions,
                                        semantic::DataType element_type) {
    last_initializer_values_.clear();
    auto record_value = [this]() {
        if (last_constant_type_ == semantic::DataType::Int) {
            last_initializer_values_.push_back(last_integer_value_);
        } else if (last_constant_type_ == semantic::DataType::Char) {
            last_initializer_values_.push_back(
                static_cast<unsigned char>(last_char_value_));
        }
    };

    if (dimensions == 0) {
        if (!parse_constant()) {
            return false;
        }
        record_value();
        if (recovery_ && last_constant_type_ != semantic::DataType::Unknown &&
            element_type != semantic::DataType::Unknown &&
            last_constant_type_ != element_type) {
            syntax_error('o', last_declarator_token_, "常量类型与变量类型不一致");
        }
        return ok_;
    }

    if (!expect(lexer::TokenKind::LBrace, "{")) {
        return false;
    }

    bool type_matches = true;
    auto note_type = [this, element_type, &type_matches]() {
        if (last_constant_type_ != semantic::DataType::Unknown &&
            element_type != semantic::DataType::Unknown &&
            last_constant_type_ != element_type) {
            type_matches = false;
        }
    };

    auto parse_constant_list = [this, &note_type, &record_value](
                                   long long& count) -> bool {
        count = 0;
        if (!parse_constant()) {
            return false;
        }
        ++count;
        record_value();
        note_type();
        while (ok_ && match(lexer::TokenKind::Comma)) {
            if (!parse_constant()) {
                return false;
            }
            ++count;
            record_value();
            note_type();
        }
        return ok_;
    };

    if (dimensions == 1) {
        long long count = 0;
        if (!parse_constant_list(count)) {
            return false;
        }
        if (!expect(lexer::TokenKind::RBrace, "}")) {
            return false;
        }
        if (recovery_) {
            if (!last_dimensions_.empty() && count != last_dimensions_[0]) {
                syntax_error('n', last_declarator_token_, "数组初始化元素个数不匹配");
            }
            if (!type_matches) {
                syntax_error('o', last_declarator_token_,
                             "常量类型与数组元素类型不一致");
            }
        }
        return ok_;
    }

    // 二维：{ { consts } {, { consts }} }
    long long rows = 0;
    long long count = 0;
    bool count_matches = true;

    if (!expect(lexer::TokenKind::LBrace, "{")) {
        return false;
    }
    if (!parse_constant_list(count)) {
        return false;
    }
    ++rows;
    if (last_dimensions_.size() >= 2 && count != last_dimensions_[1]) {
        count_matches = false;
    }
    if (!expect(lexer::TokenKind::RBrace, "}")) {
        return false;
    }

    while (ok_ && match(lexer::TokenKind::Comma)) {
        if (!expect(lexer::TokenKind::LBrace, "{")) {
            return false;
        }
        if (!parse_constant_list(count)) {
            return false;
        }
        ++rows;
        if (last_dimensions_.size() >= 2 && count != last_dimensions_[1]) {
            count_matches = false;
        }
        if (!expect(lexer::TokenKind::RBrace, "}")) {
            return false;
        }
    }

    if (!expect(lexer::TokenKind::RBrace, "}")) {
        return false;
    }
    if (recovery_) {
        if (last_dimensions_.size() >= 2) {
            if (rows != last_dimensions_[0]) {
                count_matches = false;
            }
            if (!count_matches) {
                syntax_error('n', last_declarator_token_, "数组初始化元素个数不匹配");
            }
        }
        if (!type_matches) {
            syntax_error('o', last_declarator_token_,
                         "常量类型与数组元素类型不一致");
        }
    }
    return ok_;
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
        const semantic::Symbol* symbol =
            semantic_ ? symbols_.lookup(last_declarator_token_.value) : nullptr;
        if (!parse_variable_initializer(dimensions, type)) {
            return false;
        }
        if (generator_ != nullptr && symbol != nullptr) {
            emit_initializer(*symbol);
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
    const semantic::Symbol* symbol = lookup_or_report(name_token);
    if (recovery_ && symbol != nullptr &&
        symbol->kind == semantic::SymbolKind::Constant) {
        syntax_error('j', name_token, "不能改变常量的值：" + name_token.value);
    }

    bool is_element = false;
    bool base_pushed = false;

    if (ok_ && match(lexer::TokenKind::LBracket)) {
        is_element = true;
        const lexer::Token bracket_token = previous_;
        if (generator_ != nullptr && symbol != nullptr &&
            symbol->kind == semantic::SymbolKind::Array) {
            generator_->emit_lit(symbol->address);
            base_pushed = true;
        }
        if (!parse_expression()) {
            return false;
        }
        if (base_pushed) {
            if (symbol->dimension_sizes.size() >= 2) {
                generator_->emit_lit(symbol->dimension_sizes[1]);
                generator_->emit_opr(pcode::OprCode::Mul);
            }
            generator_->emit_opr(pcode::OprCode::Add);
        }
        if (recovery_ && last_expression_type_ == semantic::DataType::Char) {
            syntax_error('i', bracket_token, "数组下标只能为整型");
        }
        if (!expect(lexer::TokenKind::RBracket, "右中括号 ]")) {
            return false;
        }
        if (ok_ && match(lexer::TokenKind::LBracket)) {
            const lexer::Token second_bracket_token = previous_;
            if (!parse_expression()) {
                return false;
            }
            if (base_pushed) {
                generator_->emit_opr(pcode::OprCode::Add);
            }
            if (recovery_ && last_expression_type_ == semantic::DataType::Char) {
                syntax_error('i', second_bracket_token, "数组下标只能为整型");
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

    if (generator_ != nullptr && symbol != nullptr) {
        if (is_element && base_pushed) {
            generator_->emit_sti();
        } else if (!is_element &&
                   symbol->kind == semantic::SymbolKind::Variable) {
            generator_->emit_sto(symbol->address);
        }
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
    const semantic::Symbol* symbol = lookup_or_report(name_token);
    if (recovery_ && symbol != nullptr &&
        symbol->kind == semantic::SymbolKind::Constant) {
        syntax_error('j', name_token, "不能修改常量：" + name_token.value);
    }
    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }

    if (generator_ != nullptr && symbol != nullptr &&
        symbol->kind == semantic::SymbolKind::Variable) {
        generator_->emit_opr(symbol->type == semantic::DataType::Char
                                 ? pcode::OprCode::ReadChar
                                 : pcode::OprCode::ReadInt);
        generator_->emit_sto(symbol->address);
    }

    marker("读语句");
    return ok_;
}

bool Parser::parse_write_statement() {
    advance();  // printf
    if (!expect(lexer::TokenKind::LParen, "(")) {
        return false;
    }

    bool has_expression = false;
    if (current().kind == lexer::TokenKind::StringConstant) {
        const std::string text = current().lexeme;
        advance();
        if (generator_ != nullptr) {
            generator_->emit_wrt_str(text);
        }
        marker("字符串");
        if (ok_ && match(lexer::TokenKind::Comma)) {
            if (!parse_expression()) {
                return false;
            }
            has_expression = true;
        }
    } else {
        if (!parse_expression()) {
            return false;
        }
        has_expression = true;
    }

    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }

    if (generator_ != nullptr) {
        if (has_expression) {
            generator_->emit_opr(last_expression_type_ == semantic::DataType::Char
                                     ? pcode::OprCode::WrtChar
                                     : pcode::OprCode::WrtInt);
        }
        generator_->emit_opr(pcode::OprCode::WrtNl);
    }

    marker("写语句");
    return ok_;
}

bool Parser::parse_return_statement() {
    const lexer::Token return_token = current();
    advance();  // return

    const bool function_returns_value =
        current_return_type_ == semantic::DataType::Int ||
        current_return_type_ == semantic::DataType::Char;

    bool has_parentheses = false;
    bool has_expression = false;
    if (ok_ && match(lexer::TokenKind::LParen)) {
        has_parentheses = true;
        if (ok_ && current().kind != lexer::TokenKind::RParen) {
            has_expression = true;
            if (!parse_expression()) {
                return false;
            }
        }
        if (!expect(lexer::TokenKind::RParen, ")")) {
            return false;
        }
    }

    if (recovery_) {
        if (!function_returns_value) {
            if (has_parentheses) {
                syntax_error('g', return_token, "无返回值函数中出现带括号的 return");
            }
        } else {
            current_function_has_return_ = true;
            if (!has_parentheses) {
                syntax_error('h', return_token, "有返回值函数中 return 缺少表达式");
            } else if (!has_expression) {
                syntax_error('h', return_token, "有返回值函数中 return() 缺少表达式");
            } else if (last_expression_type_ != semantic::DataType::Unknown &&
                       last_expression_type_ != current_return_type_) {
                syntax_error('h', return_token,
                             "return 表达式类型与函数返回值不一致");
            }
        }
    }

    if (generator_ != nullptr) {
        if (in_main_) {
            generator_->emit_end();
        } else if (!function_returns_value || has_expression) {
            generator_->emit_ret();
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
        if (semantic_) {
            for (ParameterInfo& parameter : last_parameters_) {
                parameter.address = allocate_cells(1);
                function.parameter_addresses.push_back(parameter.address);
            }
        }
        if (semantic_ && !symbols_.insert(function)) {
            syntax_error('b', name_token, "名字重定义：" + name_token.value);
        }

        symbols_.enter_scope();
        for (const ParameterInfo& parameter : last_parameters_) {
            pending_address_ = parameter.address;
            define_symbol(parameter.token.value, semantic::SymbolKind::Variable,
                          parameter.type, 0, parameter.token);
        }

        if (!expect(lexer::TokenKind::RParen, ")")) {
            symbols_.leave_scope();
            return false;
        }
        const semantic::DataType saved_return_type = current_return_type_;
        current_return_type_ = semantic::DataType::Void;
        int function_jump = -1;
        if (generator_ != nullptr) {
            function_jump = generator_->position();
            generator_->emit_jmp(0);
            function_entries_[name] = generator_->position();
        }
        const bool body_ok = parse_compound_statement_body();
        current_return_type_ = saved_return_type;
        symbols_.leave_scope();
        if (!body_ok) {
            return false;
        }
        if (generator_ != nullptr) {
            generator_->emit_ret();
            generator_->patch(function_jump, generator_->position());
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
    if (semantic_) {
        for (ParameterInfo& parameter : last_parameters_) {
            parameter.address = allocate_cells(1);
            function.parameter_addresses.push_back(parameter.address);
        }
    }
    if (semantic_ && !symbols_.insert(function)) {
        syntax_error('b', name_token, "名字重定义：" + name_token.value);
    }

    symbols_.enter_scope();
    for (const ParameterInfo& parameter : last_parameters_) {
        pending_address_ = parameter.address;
        define_symbol(parameter.token.value, semantic::SymbolKind::Variable,
                      parameter.type, 0, parameter.token);
    }

    if (!expect(lexer::TokenKind::RParen, ")")) {
        symbols_.leave_scope();
        return false;
    }
    const semantic::DataType saved_return_type = current_return_type_;
    const bool saved_has_return = current_function_has_return_;
    current_return_type_ = return_type;
    current_function_has_return_ = false;
    int function_jump = -1;
    if (generator_ != nullptr) {
        function_jump = generator_->position();
        generator_->emit_jmp(0);
        function_entries_[name] = generator_->position();
    }
    const bool body_ok = parse_compound_statement_body();
    if (recovery_ && body_ok && return_type != semantic::DataType::Void &&
        !current_function_has_return_) {
        syntax_error('h', previous_, "有返回值函数缺少 return 语句");
    }
    current_function_has_return_ = saved_has_return;
    current_return_type_ = saved_return_type;
    symbols_.leave_scope();
    if (!body_ok) {
        return false;
    }
    if (generator_ != nullptr) {
        generator_->emit_ret();
        generator_->patch(function_jump, generator_->position());
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
    const bool saved_in_main = in_main_;
    in_main_ = true;
    const bool body_ok = parse_compound_statement_body();
    in_main_ = saved_in_main;
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
    const lexer::TokenKind unary_kind = current().kind;
    if (current().kind == lexer::TokenKind::Plus ||
        current().kind == lexer::TokenKind::Minus) {
        advance();
        has_unary_sign = true;
    }
    if (!parse_term()) {
        return false;
    }
    if (generator_ != nullptr && has_unary_sign &&
        unary_kind == lexer::TokenKind::Minus) {
        generator_->emit_opr(pcode::OprCode::Neg);
    }
    semantic::DataType type =
        has_unary_sign ? semantic::DataType::Int : last_term_type_;
    while (ok_ && (current().kind == lexer::TokenKind::Plus ||
                   current().kind == lexer::TokenKind::Minus)) {
        const lexer::TokenKind op = current().kind;
        advance();
        if (!parse_term()) {
            return false;
        }
        if (generator_ != nullptr) {
            generator_->emit_opr(op == lexer::TokenKind::Plus
                                     ? pcode::OprCode::Add
                                     : pcode::OprCode::Sub);
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
        const lexer::TokenKind op = current().kind;
        advance();
        if (!parse_factor()) {
            return false;
        }
        if (generator_ != nullptr) {
            generator_->emit_opr(op == lexer::TokenKind::Star
                                     ? pcode::OprCode::Mul
                                     : pcode::OprCode::Div);
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

        const bool has_bracket =
            ok_ && current().kind == lexer::TokenKind::LBracket;
        bool base_pushed = false;
        if (generator_ != nullptr && symbol != nullptr) {
            if (!has_bracket) {
                if (symbol->kind == semantic::SymbolKind::Constant) {
                    if (symbol->type == semantic::DataType::Char) {
                        generator_->emit_lit(
                            static_cast<unsigned char>(symbol->char_value));
                    } else {
                        generator_->emit_lit(symbol->int_value);
                    }
                } else if (symbol->kind == semantic::SymbolKind::Variable) {
                    generator_->emit_lod(symbol->address);
                }
            } else if (symbol->kind == semantic::SymbolKind::Array) {
                generator_->emit_lit(symbol->address);
                base_pushed = true;
            }
        }

        if (has_bracket) {
            const lexer::Token bracket_token = current();
            advance();
            if (!parse_expression()) {
                return false;
            }
            if (base_pushed) {
                if (symbol->dimension_sizes.size() >= 2) {
                    generator_->emit_lit(symbol->dimension_sizes[1]);
                    generator_->emit_opr(pcode::OprCode::Mul);
                }
                generator_->emit_opr(pcode::OprCode::Add);
            }
            if (recovery_ && last_expression_type_ == semantic::DataType::Char) {
                syntax_error('i', bracket_token, "数组下标只能为整型");
            }
            if (!expect(lexer::TokenKind::RBracket, "右中括号 ]")) {
                return false;
            }
            if (ok_ && current().kind == lexer::TokenKind::LBracket) {
                const lexer::Token second_bracket_token = current();
                advance();
                if (!parse_expression()) {
                    return false;
                }
                if (base_pushed) {
                    generator_->emit_opr(pcode::OprCode::Add);
                }
                if (recovery_ &&
                    last_expression_type_ == semantic::DataType::Char) {
                    syntax_error('i', second_bracket_token, "数组下标只能为整型");
                }
                if (!expect(lexer::TokenKind::RBracket, "右中括号 ]")) {
                    return false;
                }
            }
            if (base_pushed) {
                generator_->emit_ldi();
            }
        }
        ok = ok_;
    } else if (current().kind == lexer::TokenKind::IntConstant ||
               current().kind == lexer::TokenKind::Plus ||
               current().kind == lexer::TokenKind::Minus) {
        ok = parse_integer();
        if (ok) {
            last_factor_type_ = semantic::DataType::Int;
            if (generator_ != nullptr) {
                generator_->emit_lit(last_integer_value_);
            }
        }
    } else if (current().kind == lexer::TokenKind::CharConstant) {
        ok = parse_character();
        if (ok) {
            last_factor_type_ = semantic::DataType::Char;
            if (generator_ != nullptr) {
                generator_->emit_lit(
                    static_cast<unsigned char>(last_char_value_));
            }
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
    auto store_argument = [this]() {
        if (generator_ != nullptr && active_call_symbol_ != nullptr) {
            const std::size_t index =
                static_cast<std::size_t>(last_argument_count_ - 1);
            if (index < active_call_symbol_->parameter_addresses.size()) {
                generator_->emit_sto(
                    active_call_symbol_->parameter_addresses[index]);
            }
        }
    };
    if (current().kind != lexer::TokenKind::RParen) {
        if (!parse_expression()) {
            return false;
        }
        ++last_argument_count_;
        last_argument_types_.push_back(last_expression_type_);
        store_argument();
        while (ok_ && match(lexer::TokenKind::Comma)) {
            if (!parse_expression()) {
                return false;
            }
            ++last_argument_count_;
            last_argument_types_.push_back(last_expression_type_);
            store_argument();
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
    active_call_symbol_ = symbol;
    const bool arguments_ok = parse_value_parameter_list();
    active_call_symbol_ = nullptr;
    if (!arguments_ok) {
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

    if (generator_ != nullptr && symbol != nullptr) {
        const auto entry = function_entries_.find(to_lower_ascii(symbol->name));
        if (entry != function_entries_.end()) {
            generator_->emit_cal(entry->second);
        }
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
    active_call_symbol_ = symbol;
    const bool arguments_ok = parse_value_parameter_list();
    active_call_symbol_ = nullptr;
    if (!arguments_ok) {
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

    if (generator_ != nullptr && symbol != nullptr) {
        const auto entry = function_entries_.find(to_lower_ascii(symbol->name));
        if (entry != function_entries_.end()) {
            generator_->emit_cal(entry->second);
            if (returns_value) {
                generator_->emit_opr(pcode::OprCode::Pop);
            }
        }
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

    const int condition_jump =
        generator_ != nullptr ? generator_->position() : -1;
    if (generator_ != nullptr) {
        generator_->emit_jpc(0);
    }

    if (!parse_statement()) {
        return false;
    }

    const bool has_else =
        ok_ && current().kind == lexer::TokenKind::KeywordElse;
    if (generator_ != nullptr) {
        if (has_else) {
            const int end_jump = generator_->position();
            generator_->emit_jmp(0);
            generator_->patch(condition_jump, generator_->position());
            advance();  // else
            if (!parse_statement()) {
                return false;
            }
            generator_->patch(end_jump, generator_->position());
        } else {
            generator_->patch(condition_jump, generator_->position());
        }
    } else if (has_else) {
        advance();  // else
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

    if (generator_ != nullptr) {
        pcode::OprCode comparison = pcode::OprCode::Eql;
        switch (kind) {
            case lexer::TokenKind::Less:
                comparison = pcode::OprCode::Lss;
                break;
            case lexer::TokenKind::LessEqual:
                comparison = pcode::OprCode::Leq;
                break;
            case lexer::TokenKind::Greater:
                comparison = pcode::OprCode::Gre;
                break;
            case lexer::TokenKind::GreaterEqual:
                comparison = pcode::OprCode::Geq;
                break;
            case lexer::TokenKind::Equal:
                comparison = pcode::OprCode::Eql;
                break;
            case lexer::TokenKind::NotEqual:
                comparison = pcode::OprCode::Neq;
                break;
            default:
                break;
        }
        generator_->emit_opr(comparison);
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
    const int loop_start =
        generator_ != nullptr ? generator_->position() : -1;
    if (!parse_condition()) {
        return false;
    }
    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }
    const int exit_jump =
        generator_ != nullptr ? generator_->position() : -1;
    if (generator_ != nullptr) {
        generator_->emit_jpc(0);
    }
    if (!parse_statement()) {
        return false;
    }
    if (generator_ != nullptr) {
        generator_->emit_jmp(loop_start);
        generator_->patch(exit_jump, generator_->position());
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
    const lexer::Token init_token = current();
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    const semantic::Symbol* init_symbol = lookup_or_report(init_token);
    if (!expect(lexer::TokenKind::Assign, "=")) {
        return false;
    }
    if (!parse_expression()) {
        return false;
    }
    if (generator_ != nullptr && init_symbol != nullptr &&
        init_symbol->kind == semantic::SymbolKind::Variable) {
        generator_->emit_sto(init_symbol->address);
    }
    if (!expect(lexer::TokenKind::Semicolon, "分号")) {
        return false;
    }

    // 条件
    const int loop_start =
        generator_ != nullptr ? generator_->position() : -1;
    if (!parse_condition()) {
        return false;
    }
    if (!expect(lexer::TokenKind::Semicolon, "分号")) {
        return false;
    }
    const int exit_jump =
        generator_ != nullptr ? generator_->position() : -1;
    if (generator_ != nullptr) {
        generator_->emit_jpc(0);
    }

    // 步长更新：标识符 = 标识符 (+|-) 步长
    const lexer::Token target_token = current();
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    const semantic::Symbol* target_symbol = lookup_or_report(target_token);
    if (!expect(lexer::TokenKind::Assign, "=")) {
        return false;
    }
    const lexer::Token source_token = current();
    if (!expect(lexer::TokenKind::Identifier, "标识符")) {
        return false;
    }
    const semantic::Symbol* source_symbol = lookup_or_report(source_token);
    if (current().kind != lexer::TokenKind::Plus &&
        current().kind != lexer::TokenKind::Minus) {
        syntax_error(current(), "期望 + 或 -");
        return false;
    }
    const lexer::TokenKind step_operator = current().kind;
    advance();

    const int step_code_start =
        generator_ != nullptr ? generator_->position() : -1;
    if (generator_ != nullptr && source_symbol != nullptr) {
        if (source_symbol->kind == semantic::SymbolKind::Constant) {
            generator_->emit_lit(
                source_symbol->type == semantic::DataType::Char
                    ? static_cast<unsigned char>(source_symbol->char_value)
                    : source_symbol->int_value);
        } else if (source_symbol->kind == semantic::SymbolKind::Variable) {
            generator_->emit_lod(source_symbol->address);
        }
    }
    if (!parse_step()) {
        return false;
    }
    if (generator_ != nullptr) {
        generator_->emit_opr(step_operator == lexer::TokenKind::Plus
                                 ? pcode::OprCode::Add
                                 : pcode::OprCode::Sub);
        if (target_symbol != nullptr &&
            target_symbol->kind == semantic::SymbolKind::Variable) {
            generator_->emit_sto(target_symbol->address);
        }
    }

    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }

    std::vector<pcode::Instruction> step_code;
    if (generator_ != nullptr) {
        step_code = generator_->take_from(step_code_start);
    }

    if (!parse_statement()) {
        return false;
    }

    if (generator_ != nullptr) {
        generator_->append(step_code);
        generator_->emit_jmp(loop_start);
        generator_->patch(exit_jump, generator_->position());
    }

    marker("循环语句");
    return ok_;
}

bool Parser::parse_step() {
    if (current().kind == lexer::TokenKind::IntConstant) {
        if (!parse_unsigned_integer()) {
            return false;
        }
        if (generator_ != nullptr) {
            generator_->emit_lit(last_unsigned_value_);
        }
    } else if (current().kind == lexer::TokenKind::Identifier) {
        const lexer::Token name_token = current();
        advance();
        const semantic::Symbol* symbol = lookup_or_report(name_token);
        if (generator_ != nullptr && symbol != nullptr) {
            if (symbol->kind == semantic::SymbolKind::Constant) {
                generator_->emit_lit(symbol->type == semantic::DataType::Char
                                         ? static_cast<unsigned char>(
                                               symbol->char_value)
                                         : symbol->int_value);
            } else if (symbol->kind == semantic::SymbolKind::Variable) {
                generator_->emit_lod(symbol->address);
            }
        }
    } else {
        syntax_error(current(), "期望步长");
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
    const semantic::DataType switch_type = last_expression_type_;
    const int saved_switch_temp = switch_temp_address_;
    std::vector<int> saved_switch_jumps;
    if (generator_ != nullptr) {
        switch_temp_address_ = allocate_cells(1);
        generator_->emit_sto(switch_temp_address_);
        saved_switch_jumps.swap(switch_end_jumps_);
    }
    if (!expect(lexer::TokenKind::RParen, ")")) {
        return false;
    }
    if (!expect(lexer::TokenKind::LBrace, "{")) {
        return false;
    }

    const semantic::DataType saved_switch_type = switch_expression_type_;
    switch_expression_type_ = switch_type;
    const bool table_ok = parse_case_table();
    switch_expression_type_ = saved_switch_type;
    if (!table_ok) {
        return false;
    }

    if (current().kind == lexer::TokenKind::KeywordDefault) {
        if (!parse_default_clause()) {
            return false;
        }
    } else if (recovery_) {
        syntax_error('p', current(), "switch 语句缺少 default 子句");
    } else {
        if (!parse_default_clause()) {
            return false;
        }
    }

    if (!expect(lexer::TokenKind::RBrace, "右花括号 }")) {
        return false;
    }

    if (generator_ != nullptr) {
        for (int index : switch_end_jumps_) {
            generator_->patch(index, generator_->position());
        }
        switch_end_jumps_.clear();
        switch_end_jumps_.swap(saved_switch_jumps);
        switch_temp_address_ = saved_switch_temp;
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
    const lexer::Token case_token = current();
    advance();  // case
    if (!parse_constant()) {
        return false;
    }
    if (recovery_ && last_constant_type_ != semantic::DataType::Unknown &&
        switch_expression_type_ != semantic::DataType::Unknown &&
        last_constant_type_ != switch_expression_type_) {
        syntax_error('o', case_token, "case 常量类型与 switch 表达式不一致");
    }

    int next_case_jump = -1;
    if (generator_ != nullptr && switch_temp_address_ >= 0) {
        generator_->emit_lod(switch_temp_address_);
        if (last_constant_type_ == semantic::DataType::Char) {
            generator_->emit_lit(
                static_cast<unsigned char>(last_char_value_));
        } else {
            generator_->emit_lit(last_integer_value_);
        }
        generator_->emit_opr(pcode::OprCode::Eql);
        next_case_jump = generator_->position();
        generator_->emit_jpc(0);
    }

    if (!expect(lexer::TokenKind::Colon, "冒号 :")) {
        return false;
    }
    if (!parse_statement()) {
        return false;
    }

    if (generator_ != nullptr && switch_temp_address_ >= 0) {
        switch_end_jumps_.push_back(generator_->position());
        generator_->emit_jmp(0);
        generator_->patch(next_case_jump, generator_->position());
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
