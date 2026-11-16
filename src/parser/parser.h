// 递归下降语法分析框架
#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <ostream>
#include <string>

#include "common/diagnostic.h"
#include "lexer/lexer.h"
#include "lexer/token.h"
#include "semantic/symbol_table.h"

namespace parser {

class Parser {
public:
    using SyntaxErrorCallback = std::function<void(const common::Diagnostic&)>;

    // out 为 nullptr 时不输出单词与成分名（仅做语法检查）。
    // recovery 为 true 时启用错误局部化：记录错误后继续解析。
    Parser(lexer::Lexer& lexer, std::ostream* out,
           SyntaxErrorCallback on_error = {}, bool recovery = false);

    bool parse_program();

    const lexer::Token& current() const { return lookahead_.front(); }
    bool at_end() const { return lookahead_.front().kind == lexer::TokenKind::End; }
    bool ok() const { return ok_; }
    bool had_error() const { return had_error_; }

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
    void syntax_error(char code, const lexer::Token& token, const std::string& message);
    char error_code_for(lexer::TokenKind expected) const;

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
    bool is_function_definition_start();
    bool parse_main_function();            // <主函数>

    bool parse_compound_statement();       // <复合语句>
    bool parse_statement_list();           // <语句列>
    bool parse_compound_statement_body();  // '{' <复合语句> '}'

    bool parse_statement();                // <语句>
    bool parse_assignment_statement();     // <赋值语句>
    bool parse_read_statement();           // <读语句>
    bool parse_write_statement();          // <写语句>
    bool parse_return_statement();         // <返回语句>
    bool parse_conditional_statement();    // <条件语句>
    bool parse_condition();                // <条件>
    bool parse_loop_statement();           // <循环语句>
    bool parse_while_statement();
    bool parse_for_statement();
    bool parse_step();                     // <步长>
    bool parse_switch_statement();         // <情况语句>
    bool parse_case_table();               // <情况表>
    bool parse_case_clause();              // <情况子语句>
    bool parse_default_clause();           // <缺省>

    bool parse_expression();               // <表达式>
    bool parse_term();                     // <项>
    bool parse_factor();                   // <因子>
    bool parse_value_parameter_list();     // <值参数表>
    bool parse_function_call();            // <有返回值函数调用语句>（表达式内）
    bool parse_call_statement(bool returns_value);  // 函数调用语句

private:
    struct ParameterInfo {
        lexer::Token token;
        semantic::DataType type = semantic::DataType::Int;
    };

    void fill_to(std::size_t k);
    void define_symbol(const std::string& name, semantic::SymbolKind kind,
                       semantic::DataType type, int dimensions,
                       const lexer::Token& where);
    const semantic::Symbol* lookup_or_report(const lexer::Token& token,
                                             bool require_function = false);

    lexer::Lexer& lexer_;
    std::ostream* out_;
    SyntaxErrorCallback on_error_;
    std::deque<lexer::Token> lookahead_;
    std::map<std::string, bool> function_returns_;  // 已定义函数：名字(小写) -> 是否返回值
    semantic::SymbolTable symbols_;
    semantic::DataType last_type_ = semantic::DataType::Unknown;
    semantic::DataType current_return_type_ = semantic::DataType::Unknown;
    bool current_function_has_return_ = false;
    semantic::DataType last_expression_type_ = semantic::DataType::Unknown;
    semantic::DataType last_term_type_ = semantic::DataType::Unknown;
    semantic::DataType last_factor_type_ = semantic::DataType::Unknown;
    semantic::DataType last_call_return_type_ = semantic::DataType::Unknown;
    lexer::Token last_declarator_token_;
    std::vector<ParameterInfo> last_parameters_;
    std::vector<semantic::DataType> last_argument_types_;
    int last_argument_count_ = 0;
    bool eof_ = false;
    bool ok_ = true;
    bool recovery_ = false;
    bool had_error_ = false;
    lexer::Token previous_;
    bool has_previous_ = false;
    int last_error_line_ = -1;
    int last_error_column_ = -1;
    bool has_last_error_ = false;
};

}  // namespace parser
