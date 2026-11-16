// 递归下降语法分析框架
#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <ostream>
#include <string>

#include "common/diagnostic.h"
#include "lexer/lexer.h"
#include "lexer/token.h"

namespace parser {

class Parser {
public:
    using SyntaxErrorCallback = std::function<void(const common::Diagnostic&)>;

    // out 为 nullptr 时不输出单词与成分名（仅做语法检查）。
    Parser(lexer::Lexer& lexer, std::ostream* out,
           SyntaxErrorCallback on_error = {});

    bool parse_program();

    const lexer::Token& current() const { return lookahead_.front(); }
    bool at_end() const { return lookahead_.front().kind == lexer::TokenKind::End; }
    bool ok() const { return ok_; }

protected:
    // 消费当前单词（输出后预读下一个）；失败返回 false。
    bool advance();
    // 当前单词匹配则消费。
    bool match(lexer::TokenKind kind);
    // 当前单词必须为 kind，否则报告语法错误。
    bool expect(lexer::TokenKind kind, const std::string& context);
    // 输出 <成分名>。
    void marker(const std::string& name);
    void syntax_error(const lexer::Token& token, const std::string& message);

    // 向前查看第 k 个单词（k=0 为当前单词），不输出。
    const lexer::Token& peek_token(std::size_t k);

    // 产生式（按 TODO 逐步实现）
    bool parse_constant_declaration();  // <常量说明>
    bool parse_constant_definition();   // <常量定义>
    bool parse_integer();               // <整数>
    bool parse_unsigned_integer();      // <无符号整数>
    bool parse_character();             // <字符>（不输出成分名）
    bool parse_constant();              // <常量>

    bool parse_variable_declaration();  // <变量说明>
    bool parse_variable_definition();   // <变量定义>
    bool parse_type_identifier();       // <类型标识符>（不输出成分名）
    bool parse_declarator(int& dimensions);
    bool parse_variable_initializer(int dimensions);
    bool can_start_variable();

    bool parse_declaration_header();       // <声明头部>
    bool parse_parameter_list();           // <参数表>
    bool parse_function_definition();      // <有/无返回值函数定义>
    bool parse_compound_statement_body();  // 临时：仅消费 { ... }
    bool is_function_definition_start();

private:
    void fill_to(std::size_t k);

    lexer::Lexer& lexer_;
    std::ostream* out_;
    SyntaxErrorCallback on_error_;
    std::deque<lexer::Token> lookahead_;
    bool eof_ = false;
    bool ok_ = true;
};

}  // namespace parser
