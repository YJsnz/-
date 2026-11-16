#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "codegen/code_generator.h"
#include "common/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "pcode/pcode.h"

namespace {

int failures = 0;

void check(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "[PASS] " << name << "\n";
    } else {
        std::cout << "[FAIL] " << name << "\n";
        ++failures;
    }
}

std::vector<common::Diagnostic> collect_errors(const std::string& source,
                                               bool& parsed) {
    lexer::Lexer lexer(source);
    std::ostringstream out;
    std::vector<common::Diagnostic> diagnostics;
    parser::Parser parser(
        lexer, &out,
        [&diagnostics](const common::Diagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        },
        true);
    parsed = parser.parse_program();
    return diagnostics;
}

std::vector<pcode::Instruction> generate(const std::string& source,
                                         bool& parsed) {
    lexer::Lexer lexer(source);
    std::ostringstream out;
    codegen::CodeGenerator generator;
    parser::Parser parser(lexer, &out, nullptr, false, &generator);
    parsed = parser.parse_program();
    return generator.instructions();
}

bool has_error(const std::vector<common::Diagnostic>& diagnostics, char code,
               int line) {
    for (const common::Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.code == code && diagnostic.line == line) {
            return true;
        }
    }
    return false;
}

bool has_any_error(const std::vector<common::Diagnostic>& diagnostics, char code) {
    for (const common::Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

// 暴露受保护的解析原语，便于单元测试。
class TestParser : public parser::Parser {
public:
    using parser::Parser::Parser;
    using parser::Parser::advance;
    using parser::Parser::expect;
    using parser::Parser::marker;
    using parser::Parser::match;
    using parser::Parser::parse_constant;
    using parser::Parser::parse_constant_declaration;
    using parser::Parser::parse_constant_definition;
    using parser::Parser::parse_integer;
    using parser::Parser::can_start_variable;
    using parser::Parser::is_function_definition_start;
    using parser::Parser::parse_assignment_statement;
    using parser::Parser::parse_call_statement;
    using parser::Parser::parse_case_clause;
    using parser::Parser::parse_case_table;
    using parser::Parser::parse_condition;
    using parser::Parser::parse_conditional_statement;
    using parser::Parser::parse_declaration_header;
    using parser::Parser::parse_default_clause;
    using parser::Parser::parse_expression;
    using parser::Parser::parse_factor;
    using parser::Parser::parse_for_statement;
    using parser::Parser::parse_function_call;
    using parser::Parser::parse_function_definition;
    using parser::Parser::parse_loop_statement;
    using parser::Parser::parse_main_function;
    using parser::Parser::parse_parameter_list;
    using parser::Parser::parse_read_statement;
    using parser::Parser::parse_return_statement;
    using parser::Parser::parse_statement;
    using parser::Parser::parse_step;
    using parser::Parser::parse_switch_statement;
    using parser::Parser::parse_term;
    using parser::Parser::parse_value_parameter_list;
    using parser::Parser::parse_while_statement;
    using parser::Parser::parse_write_statement;
    using parser::Parser::parse_variable_declaration;
    using parser::Parser::parse_variable_definition;
    using parser::Parser::peek_token;
};

}  // namespace

int main() {
    {
        lexer::Lexer lexer("a 1");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.current().kind == lexer::TokenKind::Identifier,
              "构造后已预读首个单词");
        check(out.str().empty(), "预读不输出");

        check(parser.advance(), "advance 成功");
        check(out.str() == "IDENFR a\n", "消费时输出当前单词");
        check(parser.current().kind == lexer::TokenKind::IntConstant,
              "advance 后预读下一个单词");

        parser.marker("测试");
        check(out.str() == "IDENFR a\n<测试>\n", "marker 输出成分名");

        parser.advance();
        check(out.str() == "IDENFR a\n<测试>\nINTCON 1\n",
              "第二个单词按序输出");
        check(parser.at_end(), "消费完毕到达文件尾");
    }
    {
        lexer::Lexer lexer("a =");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                              [&diagnostics](const common::Diagnostic& diagnostic) {
                                  diagnostics.push_back(diagnostic);
                              });

        check(parser.expect(lexer::TokenKind::Identifier, "标识符"),
              "expect 匹配成功并消费");
        check(!parser.expect(lexer::TokenKind::Semicolon, "分号"),
              "expect 不匹配返回 false");
        check(!parser.ok(), "expect 失败后 ok 为假");
        check(diagnostics.size() == 1, "语法错误触发回调");
        check(diagnostics.size() == 1 && diagnostics[0].line == 1 &&
                  diagnostics[0].column == 1,
              "语法错误位置为前一个单词");
        check(diagnostics.size() == 1 && diagnostics[0].code == 'k',
              "缺失分号类别码为 k");
    }
    {
        lexer::Lexer lexer("a 1");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.match(lexer::TokenKind::Identifier), "match 命中并消费");
        check(!parser.match(lexer::TokenKind::Identifier),
              "match 未命中返回 false");
        check(out.str() == "IDENFR a\n", "match 命中时输出被消费的单词");
    }
    {
        lexer::Lexer lexer("@");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(!parser.ok(), "首个词法单元为 Error 时 ok 为假");
        check(out.str().empty(), "词法错误时不输出单词");
        parser.marker("不应输出");
        check(out.str().empty(), "失败后 marker 不输出");
    }
    {
        lexer::Lexer lexer("const int a = 1, b = -2; const char c = 'x';");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_constant_declaration(), "常量说明解析成功");
        check(parser.at_end(), "常量说明消费完所有单词");
        const std::string expected =
            "CONSTTK const\n"
            "INTTK int\n"
            "IDENFR a\n"
            "ASSIGN =\n"
            "INTCON 1\n"
            "<无符号整数>\n"
            "<整数>\n"
            "COMMA ,\n"
            "IDENFR b\n"
            "ASSIGN =\n"
            "MINU -\n"
            "INTCON 2\n"
            "<无符号整数>\n"
            "<整数>\n"
            "<常量定义>\n"
            "SEMICN ;\n"
            "CONSTTK const\n"
            "CHARTK char\n"
            "IDENFR c\n"
            "ASSIGN =\n"
            "CHARCON x\n"
            "<常量定义>\n"
            "SEMICN ;\n"
            "<常量说明>\n";
        check(out.str() == expected, "常量说明与成分名顺序正确");
    }
    {
        lexer::Lexer lexer("+5");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_integer(), "带 + 的整数解析成功");
        check(out.str() == "PLUS +\nINTCON 5\n<无符号整数>\n<整数>\n",
              "整数输出顺序正确");
    }
    {
        lexer::Lexer lexer("'x'");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_constant(), "字符常量解析成功");
        check(out.str() == "CHARCON x\n<常量>\n", "字符常量输出 <常量>");
    }
    {
        lexer::Lexer lexer("const int = 1;");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& diagnostic) {
                              diagnostics.push_back(diagnostic);
                          });

        check(!parser.parse_constant_declaration(), "缺少标识符时解析失败");
        check(!parser.ok(), "失败后 ok 为假");
        check(diagnostics.size() == 1, "语法错误被上报");
    }
    {
        lexer::Lexer lexer("const char c = 1;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(!parser.parse_constant_declaration(), "char 常量类型不匹配时失败");
    }
    {
        lexer::Lexer lexer(";");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(!parser.parse_constant(), "非常量输入解析失败");
    }
    {
        lexer::Lexer lexer("int a;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_variable_declaration(), "简单变量声明解析成功");
        check(parser.at_end(), "变量声明消费完所有单词");
        const std::string expected =
            "INTTK int\n"
            "IDENFR a\n"
            "<变量定义无初始化>\n"
            "<变量定义>\n"
            "SEMICN ;\n"
            "<变量说明>\n";
        check(out.str() == expected, "变量声明与成分名顺序正确");
    }
    {
        lexer::Lexer lexer("int a, b[2]; char c;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_variable_declaration(), "多变量与一维数组声明成功");
        const std::string expected =
            "INTTK int\n"
            "IDENFR a\n"
            "COMMA ,\n"
            "IDENFR b\n"
            "LBRACK [\n"
            "INTCON 2\n"
            "<无符号整数>\n"
            "RBRACK ]\n"
            "<变量定义无初始化>\n"
            "<变量定义>\n"
            "SEMICN ;\n"
            "CHARTK char\n"
            "IDENFR c\n"
            "<变量定义无初始化>\n"
            "<变量定义>\n"
            "SEMICN ;\n"
            "<变量说明>\n";
        check(out.str() == expected, "逗号分隔与数组维度输出正确");
    }
    {
        lexer::Lexer lexer("int a = 1;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_variable_declaration(), "简单变量初始化解析成功");
        const std::string expected =
            "INTTK int\n"
            "IDENFR a\n"
            "ASSIGN =\n"
            "INTCON 1\n"
            "<无符号整数>\n"
            "<整数>\n"
            "<常量>\n"
            "<变量定义及初始化>\n"
            "<变量定义>\n"
            "SEMICN ;\n"
            "<变量说明>\n";
        check(out.str() == expected, "初始化变量与成分名顺序正确");
    }
    {
        lexer::Lexer lexer("int m[2][2] = {{1, 2}, {3, 4}};");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_variable_declaration(), "二维数组初始化解析成功");
        check(out.str().find("<变量定义及初始化>\n<变量定义>\nSEMICN ;\n<变量说明>\n") !=
                  std::string::npos,
              "二维数组初始化标记顺序正确");
    }
    {
        lexer::Lexer lexer("int f(int a)");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.can_start_variable(), "函数定义不会被识别为变量定义");

        lexer::Lexer lexer2("int a;");
        std::ostringstream out2;
        TestParser parser2(lexer2, &out2);
        check(parser2.can_start_variable(), "普通变量声明可被识别");
    }
    {
        lexer::Lexer lexer("int = 1;");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.parse_variable_declaration(), "缺少变量名时解析失败");
    }
    {
        lexer::Lexer lexer("int f(int a, char b){}");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_function_definition(), "有返回值函数定义解析成功");
        check(parser.at_end(), "函数定义消费完所有单词");
        const std::string expected =
            "INTTK int\n"
            "IDENFR f\n"
            "<声明头部>\n"
            "LPARENT (\n"
            "INTTK int\n"
            "IDENFR a\n"
            "COMMA ,\n"
            "CHARTK char\n"
            "IDENFR b\n"
            "<参数表>\n"
            "RPARENT )\n"
            "LBRACE {\n"
            "<语句列>\n"
            "<复合语句>\n"
            "RBRACE }\n"
            "<有返回值函数定义>\n";
        check(out.str() == expected, "函数定义与成分名顺序正确");
    }
    {
        lexer::Lexer lexer("void g(){}");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_function_definition(), "无返回值函数定义解析成功");
        const std::string expected =
            "VOIDTK void\n"
            "IDENFR g\n"
            "LPARENT (\n"
            "<参数表>\n"
            "RPARENT )\n"
            "LBRACE {\n"
            "<语句列>\n"
            "<复合语句>\n"
            "RBRACE }\n"
            "<无返回值函数定义>\n";
        check(out.str() == expected, "无返回值函数输出正确（空参数表）");
    }
    {
        lexer::Lexer lexer("int f(int a){ return(a); }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_function_definition(), "带函数体的定义解析成功");
        const std::string expected =
            "INTTK int\n"
            "IDENFR f\n"
            "<声明头部>\n"
            "LPARENT (\n"
            "INTTK int\n"
            "IDENFR a\n"
            "<参数表>\n"
            "RPARENT )\n"
            "LBRACE {\n"
            "RETURNTK return\n"
            "LPARENT (\n"
            "IDENFR a\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n"
            "RPARENT )\n"
            "<返回语句>\n"
            "SEMICN ;\n"
            "<语句>\n"
            "<语句列>\n"
            "<复合语句>\n"
            "RBRACE }\n"
            "<有返回值函数定义>\n";
        check(out.str() == expected, "函数体单词按序输出");
    }
    {
        lexer::Lexer lexer1("int f(int a)");
        std::ostringstream out1;
        TestParser parser1(lexer1, &out1);
        check(parser1.is_function_definition_start(), "int f( 被识别为函数定义");

        lexer::Lexer lexer2("int a;");
        std::ostringstream out2;
        TestParser parser2(lexer2, &out2);
        check(!parser2.is_function_definition_start(), "int a; 不是函数定义");

        lexer::Lexer lexer3("void main()");
        std::ostringstream out3;
        TestParser parser3(lexer3, &out3);
        check(!parser3.is_function_definition_start(), "void main 不按普通函数处理");
    }
    {
        lexer::Lexer lexer("void ()");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& diagnostic) {
                              diagnostics.push_back(diagnostic);
                          });

        check(!parser.parse_function_definition(), "缺少函数名时解析失败");
        check(diagnostics.size() == 1, "函数名缺失错误被上报");
    }
    {
        lexer::Lexer lexer("void g(){ { a = 1; } }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_function_definition(), "嵌套块语句的函数体解析成功");
        check(parser.at_end(), "嵌套块语句函数体消费完所有单词");
    }
    {
        lexer::Lexer lexer("void main(){}");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_program(), "空主函数程序解析成功");
        check(parser.at_end(), "主函数后到达文件尾");
        const std::string expected =
            "VOIDTK void\n"
            "MAINTK main\n"
            "LPARENT (\n"
            "RPARENT )\n"
            "LBRACE {\n"
            "<语句列>\n"
            "<复合语句>\n"
            "RBRACE }\n"
            "<主函数>\n"
            "<程序>\n";
        check(out.str() == expected, "主函数与程序标记顺序正确");
    }
    {
        lexer::Lexer lexer(
            "const int a = 1;\n"
            "int x;\n"
            "int f(int n){ return(n); }\n"
            "void main(){ }\n");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_program(), "完整程序骨架解析成功");
        const std::string text = out.str();
        check(text.find("<常量说明>\nINTTK int\nIDENFR x") != std::string::npos,
              "常量说明在变量说明之前");
        check(text.find("<变量说明>\nINTTK int\nIDENFR f") != std::string::npos,
              "变量说明在函数定义之前");
        check(text.find("<有返回值函数定义>\nVOIDTK void\nMAINTK main") !=
                  std::string::npos,
              "函数定义在主函数之前");
        check(text.size() >= 9 &&
                  text.compare(text.size() - 9, 9, "<程序>\n") == 0,
              "程序标记位于最后");
    }
    {
        lexer::Lexer lexer("int a;");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.parse_program(), "缺少主函数时解析失败");
    }
    {
        lexer::Lexer lexer("void main(){} int a;");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.parse_program(), "主函数后的多余内容被拒绝");
    }
    {
        lexer::Lexer lexer("void main(int a){}");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.parse_program(), "主函数带参数时解析失败");
    }
    {
        lexer::Lexer lexer("void g(){ const int a=1; int x; }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_function_definition(), "复合语句内常量/变量说明解析成功");
        const std::string text = out.str();
        check(text.find("LBRACE {\nCONSTTK const") != std::string::npos,
              "复合语句内常量说明位于体首");
        check(text.find("<常量说明>\nINTTK int\nIDENFR x") != std::string::npos,
              "常量说明后接变量说明");
        check(text.find("<变量说明>\n<语句列>\n<复合语句>\nRBRACE }") !=
                  std::string::npos,
              "变量说明后输出语句列与复合语句");
    }
    {
        lexer::Lexer lexer("void g(){ a = 1; b = 2; }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_function_definition(), "多条语句的复合语句解析成功");
        check(out.str().find("<语句列>\n<复合语句>\nRBRACE }") != std::string::npos,
              "语句列与复合语句位于右花括号前");
    }
    {
        lexer::Lexer lexer("void g(){ { a = 1; } }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_function_definition(), "嵌套块语句的复合语句解析成功");
        check(parser.at_end(), "嵌套块语句消费完所有单词");
    }
    {
        lexer::Lexer lexer("1");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_expression(), "单因子表达式解析成功");
        const std::string expected =
            "INTCON 1\n"
            "<无符号整数>\n"
            "<整数>\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n";
        check(out.str() == expected, "整数因子与表达式标记顺序正确");
    }
    {
        lexer::Lexer lexer("-1 + 2 * 3");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_expression(), "带符号与乘除的表达式解析成功");
        const std::string expected =
            "MINU -\n"
            "INTCON 1\n"
            "<无符号整数>\n"
            "<整数>\n"
            "<因子>\n"
            "<项>\n"
            "PLUS +\n"
            "INTCON 2\n"
            "<无符号整数>\n"
            "<整数>\n"
            "<因子>\n"
            "MULT *\n"
            "INTCON 3\n"
            "<无符号整数>\n"
            "<整数>\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n";
        check(out.str() == expected, "表达式与项的优先级顺序正确");
    }
    {
        lexer::Lexer lexer("(a + 1)");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_expression(), "括号因子解析成功");
        const std::string expected =
            "LPARENT (\n"
            "IDENFR a\n"
            "<因子>\n"
            "<项>\n"
            "PLUS +\n"
            "INTCON 1\n"
            "<无符号整数>\n"
            "<整数>\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n"
            "RPARENT )\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n";
        check(out.str() == expected, "括号表达式输出正确");
    }
    {
        lexer::Lexer lexer("a[1][2]");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_expression(), "二维数组元素因子解析成功");
        check(parser.at_end(), "二维数组元素消费完所有单词");
        check(out.str().find("RBRACK ]\n<因子>\n<项>\n<表达式>") != std::string::npos,
              "二维数组下标与因子标记正确");
    }
    {
        lexer::Lexer lexer("f(1, 2)");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_expression(), "函数调用因子解析成功");
        check(out.str().find(
                  "<值参数表>\nRPARENT )\n<有返回值函数调用语句>\n<因子>\n<项>\n<表达式>") !=
                  std::string::npos,
              "函数调用因子标记顺序正确");
    }
    {
        lexer::Lexer lexer("f()");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_expression(), "空实参函数调用解析成功");
        check(out.str().find("<值参数表>\nRPARENT )\n<有返回值函数调用语句>") !=
                  std::string::npos,
              "空值参数表也会输出");
    }
    {
        lexer::Lexer lexer("'x'");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_expression(), "字符因子解析成功");
        check(out.str() == "CHARCON x\n<因子>\n<项>\n<表达式>\n",
              "字符因子输出正确");
    }
    {
        lexer::Lexer lexer("*");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.parse_expression(), "非法因子表达式解析失败");
    }
    {
        lexer::Lexer lexer("a = 1;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "赋值语句解析成功");
        const std::string expected =
            "IDENFR a\n"
            "ASSIGN =\n"
            "INTCON 1\n"
            "<无符号整数>\n"
            "<整数>\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n"
            "<赋值语句>\n"
            "SEMICN ;\n"
            "<语句>\n";
        check(out.str() == expected, "赋值语句与成分名顺序正确");
    }
    {
        lexer::Lexer lexer("a[1] = 2;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "数组元素赋值解析成功");
        check(out.str().find("RBRACK ]\nASSIGN =\nINTCON 2") != std::string::npos,
              "数组下标后接赋值");
    }
    {
        lexer::Lexer lexer("scanf(x);");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "读语句解析成功");
        const std::string expected =
            "SCANFTK scanf\n"
            "LPARENT (\n"
            "IDENFR x\n"
            "RPARENT )\n"
            "<读语句>\n"
            "SEMICN ;\n"
            "<语句>\n";
        check(out.str() == expected, "读语句输出正确");
    }
    {
        lexer::Lexer lexer("printf(\"Hi\");");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "字符串写语句解析成功");
        const std::string expected =
            "PRINTFTK printf\n"
            "LPARENT (\n"
            "STRCON Hi\n"
            "<字符串>\n"
            "RPARENT )\n"
            "<写语句>\n"
            "SEMICN ;\n"
            "<语句>\n";
        check(out.str() == expected, "字符串写语句输出正确");
    }
    {
        lexer::Lexer lexer("printf(\"x\", 1);");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "字符串加表达式写语句解析成功");
        check(out.str().find("STRCON x\n<字符串>\nCOMMA ,\nINTCON 1") !=
                  std::string::npos,
              "字符串与表达式之间输出逗号与表达式标记");
    }
    {
        lexer::Lexer lexer("printf(1);");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "表达式写语句解析成功");
        check(out.str().find("LPARENT (\nINTCON 1") != std::string::npos,
              "表达式写语句直接解析表达式");
    }
    {
        lexer::Lexer lexer("return;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "无表达式返回语句解析成功");
        const std::string expected =
            "RETURNTK return\n"
            "<返回语句>\n"
            "SEMICN ;\n"
            "<语句>\n";
        check(out.str() == expected, "无表达式返回语句输出正确");
    }
    {
        lexer::Lexer lexer("return(1);");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "带表达式返回语句解析成功");
        check(out.str().find("<表达式>\nRPARENT )\n<返回语句>") != std::string::npos,
              "返回表达式后输出右括号与返回语句标记");
    }
    {
        lexer::Lexer lexer(";");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "空语句解析成功");
        check(out.str() == "SEMICN ;\n<语句>\n", "空语句输出正确");
    }
    {
        lexer::Lexer lexer("if (a < b) x = 1;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "单分支条件语句解析成功");
        const std::string expected =
            "IFTK if\n"
            "LPARENT (\n"
            "IDENFR a\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n"
            "LSS <\n"
            "IDENFR b\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n"
            "<条件>\n"
            "RPARENT )\n"
            "IDENFR x\n"
            "ASSIGN =\n"
            "INTCON 1\n"
            "<无符号整数>\n"
            "<整数>\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n"
            "<赋值语句>\n"
            "SEMICN ;\n"
            "<语句>\n"
            "<条件语句>\n";
        check(out.str() == expected, "条件语句与成分名顺序正确");
    }
    {
        lexer::Lexer lexer("if (a == 1) x = 1; else x = 2;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "if-else 条件语句解析成功");
        check(out.str().find("SEMICN ;\n<语句>\nELSETK else") != std::string::npos,
              "else 位于 then 语句之后");
        const std::string tail = "<条件语句>\n";
        check(out.str().size() >= tail.size() &&
                  out.str().compare(out.str().size() - tail.size(), tail.size(), tail) == 0,
              "条件语句标记位于最后");
    }
    {
        lexer::Lexer lexer("if (a < b) if (c < d) x = 1; else x = 2;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "嵌套条件语句解析成功");
        const std::string text = out.str();
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = text.find("<条件语句>", pos)) != std::string::npos) {
            ++count;
            pos += 1;
        }
        check(count == 2, "嵌套条件语句输出两个条件语句标记");
    }
    {
        lexer::Lexer lexer("if (a) x = 1;");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.parse_statement(), "缺少关系运算符时条件语句解析失败");
    }
    {
        lexer::Lexer lexer("if (a < b) { x = 1; }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "分支为块语句的条件语句解析成功");
        check(out.str().find("<语句列>\nRBRACE }\n<语句>\n<条件语句>") !=
                  std::string::npos,
              "块语句分支的标记顺序正确");
    }
    {
        lexer::Lexer lexer("while (a < b) x = 1;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "while 循环解析成功");
        const std::string expected =
            "WHILETK while\n"
            "LPARENT (\n"
            "IDENFR a\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n"
            "LSS <\n"
            "IDENFR b\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n"
            "<条件>\n"
            "RPARENT )\n"
            "IDENFR x\n"
            "ASSIGN =\n"
            "INTCON 1\n"
            "<无符号整数>\n"
            "<整数>\n"
            "<因子>\n"
            "<项>\n"
            "<表达式>\n"
            "<赋值语句>\n"
            "SEMICN ;\n"
            "<语句>\n"
            "<循环语句>\n";
        check(out.str() == expected, "while 循环与成分名顺序正确");
    }
    {
        lexer::Lexer lexer("for (i = 1; i < 10; i = i + 1) x = 1;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "for 循环解析成功");
        const std::string text = out.str();
        check(text.find("FORTK for\nLPARENT (\nIDENFR i\nASSIGN =\nINTCON 1") !=
                  std::string::npos,
              "for 初始化部分输出正确");
        check(text.find("<条件>\nSEMICN ;\nIDENFR i\nASSIGN =\nIDENFR i\nPLUS +\n"
                        "INTCON 1\n<无符号整数>\n<步长>\nRPARENT )") !=
                  std::string::npos,
              "for 步长部分输出正确");
        const std::string tail = "<语句>\n<循环语句>\n";
        check(text.size() >= tail.size() &&
                  text.compare(text.size() - tail.size(), tail.size(), tail) == 0,
              "for 循环标记位于最后");
    }
    {
        lexer::Lexer lexer("for (i = 10; i > 0; i = i - 2) x = 1;");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "负步长 for 循环解析成功");
        check(out.str().find("MINU -\nINTCON 2\n<无符号整数>\n<步长>") !=
                  std::string::npos,
              "负步长输出正确");
    }
    {
        lexer::Lexer lexer("while (a < b) { x = 1; }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "循环体为块语句的 while 解析成功");
        check(out.str().find("<语句列>\nRBRACE }\n<语句>\n<循环语句>") !=
                  std::string::npos,
              "块循环体的标记顺序正确");
    }
    {
        lexer::Lexer lexer("while (a) x = 1;");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.parse_statement(), "循环条件缺少关系运算符时失败");
    }
    {
        lexer::Lexer lexer("for (i = 1 i < 10; i = i + 1) x = 1;");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.parse_statement(), "for 缺少分号时解析失败");
    }
    {
        lexer::Lexer lexer("switch (x) { case 1: x = 1; default: x = 2; }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "情况语句解析成功");
        const std::string text = out.str();
        check(text.find("SWITCHTK switch\nLPARENT (\nIDENFR x") != std::string::npos,
              "switch 表达式输出正确");
        check(text.find("CASETK case\nINTCON 1\n<无符号整数>\n<整数>\n<常量>\nCOLON :") !=
                  std::string::npos,
              "case 常量与冒号输出正确");
        check(text.find("<语句>\n<情况子语句>\n<情况表>\nDEFAULTTK default\nCOLON :") !=
                  std::string::npos,
              "情况表后接缺省语句");
        const std::string tail = "<语句>\n<缺省>\nRBRACE }\n<情况语句>\n";
        check(text.size() >= tail.size() &&
                  text.compare(text.size() - tail.size(), tail.size(), tail) == 0,
              "情况语句标记位于最后");
    }
    {
        lexer::Lexer lexer(
            "switch (x) { case 1: x = 1; case 2: x = 2; default: x = 3; }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "多个情况子语句解析成功");
        const std::string text = out.str();
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = text.find("<情况子语句>", pos)) != std::string::npos) {
            ++count;
            pos += 1;
        }
        check(count == 2, "两个 case 输出两个情况子语句标记");
    }
    {
        lexer::Lexer lexer("switch (x) { case 'a': x = 1; default: x = 2; }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "字符常量 case 解析成功");
        check(out.str().find("CHARCON a\n<常量>\nCOLON :") != std::string::npos,
              "字符 case 输出常量标记");
    }
    {
        lexer::Lexer lexer("switch (x) { case 1: x = 1; }");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.parse_statement(), "缺少 default 时情况语句解析失败");
    }
    {
        lexer::Lexer lexer("switch (x) { default: x = 1; }");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.parse_statement(), "缺少 case 时情况语句解析失败");
    }
    {
        lexer::Lexer lexer("switch (x) { case 1: { x = 1; } default: x = 2; }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_statement(), "块语句作为 case 分支解析成功");
        check(out.str().find("<语句列>\nRBRACE }\n<语句>\n<情况子语句>") !=
                  std::string::npos,
              "块分支的标记顺序正确");
    }
    {
        lexer::Lexer lexer("switch (x) { case 1 x = 1; default: x = 2; }");
        std::ostringstream out;
        TestParser parser(lexer, &out);
        check(!parser.parse_statement(), "case 缺少冒号时解析失败");
    }
    {
        lexer::Lexer lexer(
            "void show(int x){ }\n"
            "void main(){ show(1); }\n");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_program(), "无返回值函数调用语句解析成功");
        check(out.str().find("<值参数表>\nRPARENT )\n<无返回值函数调用语句>\n"
                             "SEMICN ;\n<语句>") != std::string::npos,
              "无返回值调用标记正确");
    }
    {
        lexer::Lexer lexer(
            "int f(){ return(1); }\n"
            "void main(){ f(); }\n");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_program(), "有返回值函数调用语句解析成功");
        check(out.str().find("<值参数表>\nRPARENT )\n<有返回值函数调用语句>\n"
                             "SEMICN ;\n<语句>") != std::string::npos,
              "有返回值调用标记正确");
    }
    {
        lexer::Lexer lexer(
            "void g(int a,int b){ }\n"
            "void main(){ g(1,2); }\n");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_program(), "多实参函数调用语句解析成功");
        check(out.str().find("COMMA ,") != std::string::npos,
              "多实参之间输出逗号");
    }
    {
        lexer::Lexer lexer(
            "void h(){ }\n"
            "void main(){ h(); }\n");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_program(), "空实参函数调用语句解析成功");
        check(out.str().find("<值参数表>\nRPARENT )\n<无返回值函数调用语句>") !=
                  std::string::npos,
              "空值参数表的调用标记正确");
    }
    {
        lexer::Lexer lexer("void main(){ foo(1); }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_program(), "未定义函数调用按有返回值处理");
        check(out.str().find("<有返回值函数调用语句>") != std::string::npos,
              "未定义函数输出有返回值标记");
    }
    {
        lexer::Lexer lexer(
            "void Show(int x){ }\n"
            "void main(){ show(1); }\n");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_program(), "函数名大小写不敏感查找成功");
        check(out.str().find("<无返回值函数调用语句>") != std::string::npos,
              "大小写不同的调用仍识别为无返回值");
    }
    {
        lexer::Lexer lexer(
            "const int a = 1;\n"
            "const int A = 2;\n"
            "void main(){}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& diagnostic) {
                              diagnostics.push_back(diagnostic);
                          },
                          true);

        check(parser.parse_program(), "重定义程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& diagnostic : diagnostics) {
            if (diagnostic.code == 'b' && diagnostic.line == 2) {
                found = true;
            }
        }
        check(found, "大小写不同的同名常量报 b");
    }
    {
        lexer::Lexer lexer(
            "void main(){\n"
            "int x;\n"
            "int X;\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& diagnostic) {
                              diagnostics.push_back(diagnostic);
                          },
                          true);

        check(parser.parse_program(), "同名变量程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& diagnostic : diagnostics) {
            if (diagnostic.code == 'b' && diagnostic.line == 3) {
                found = true;
            }
        }
        check(found, "同一作用域同名变量报 b");
    }
    {
        lexer::Lexer lexer(
            "int x;\n"
            "void f(){\n"
            "int x;\n"
            "}\n"
            "void main(){}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& diagnostic) {
                              diagnostics.push_back(diagnostic);
                          },
                          true);

        check(parser.parse_program(), "跨作用域同名程序解析成功");
        bool found = false;
        for (const common::Diagnostic& diagnostic : diagnostics) {
            if (diagnostic.code == 'b') {
                found = true;
            }
        }
        check(!found, "不同作用域同名变量不报 b");
    }
    {
        lexer::Lexer lexer(
            "int f(int a, int A){\n"
            "return(a);\n"
            "}\n"
            "void main(){}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& diagnostic) {
                              diagnostics.push_back(diagnostic);
                          },
                          true);

        check(parser.parse_program(), "重名参数程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& diagnostic : diagnostics) {
            if (diagnostic.code == 'b' && diagnostic.line == 1) {
                found = true;
            }
        }
        check(found, "同名参数报 b");
    }
    {
        lexer::Lexer lexer(
            "int f(){ return(1); }\n"
            "int F(){ return(2); }\n"
            "void main(){}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& diagnostic) {
                              diagnostics.push_back(diagnostic);
                          },
                          true);

        check(parser.parse_program(), "重名函数程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& diagnostic : diagnostics) {
            if (diagnostic.code == 'b' && diagnostic.line == 2) {
                found = true;
            }
        }
        check(found, "同名函数报 b");
    }
    {
        lexer::Lexer lexer("void main(){\nx = 1;\n}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "未定义变量程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'c' && d.line == 2) {
                found = true;
            }
        }
        check(found, "未定义赋值目标报 c");
    }
    {
        lexer::Lexer lexer("void main(){\nint x;\nx = y;\n}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "未定义右值程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'c' && d.line == 3) {
                found = true;
            }
        }
        check(found, "表达式中未定义名字报 c");
    }
    {
        lexer::Lexer lexer("void main(){\nprintf(x);\n}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "未定义写参数程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'c' && d.line == 2) {
                found = true;
            }
        }
        check(found, "printf 参数未定义报 c");
    }
    {
        lexer::Lexer lexer("void main(){\nscanf(x);\n}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "未定义读参数程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'c' && d.line == 2) {
                found = true;
            }
        }
        check(found, "scanf 参数未定义报 c");
    }
    {
        lexer::Lexer lexer("void main(){\nf(1);\n}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "未定义函数调用程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'c' && d.line == 2) {
                found = true;
            }
        }
        check(found, "未定义函数调用报 c");
    }
    {
        lexer::Lexer lexer("void main(){\nx = 1;\nint x;\n}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "先用后声明程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'c' && d.line == 2) {
                found = true;
            }
        }
        check(found, "先用后声明报 c");
    }
    {
        lexer::Lexer lexer(
            "int x;\n"
            "int f(){ return(1); }\n"
            "void main(){\n"
            "x = 1;\n"
            "printf(x);\n"
            "scanf(x);\n"
            "f();\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "已定义名字程序解析成功");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'c') {
                found = true;
            }
        }
        check(!found, "已定义名字不报 c");
    }
    {
        lexer::Lexer lexer(
            "int f(int a, int b){\n"
            "return(a+b);\n"
            "}\n"
            "void main(){\n"
            "f(1);\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "实参过少程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'd' && d.line == 5) {
                found = true;
            }
        }
        check(found, "实参少于形参报 d");
    }
    {
        lexer::Lexer lexer(
            "int f(int a, int b){\n"
            "return(a+b);\n"
            "}\n"
            "void main(){\n"
            "f(1, 2, 3);\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "实参过多程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'd' && d.line == 5) {
                found = true;
            }
        }
        check(found, "实参多于形参报 d");
    }
    {
        lexer::Lexer lexer(
            "int f(int a, int b){\n"
            "return(a+b);\n"
            "}\n"
            "void main(){\n"
            "f(1, 2);\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "参数个数正确程序解析成功");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'd') {
                found = true;
            }
        }
        check(!found, "参数个数正确不报 d");
    }
    {
        lexer::Lexer lexer(
            "void g(){\n"
            "}\n"
            "void main(){\n"
            "g(1);\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "空形参函数带实参程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'd' && d.line == 4) {
                found = true;
            }
        }
        check(found, "空形参函数带实参报 d");
    }
    {
        lexer::Lexer lexer(
            "int f(int a, int b){\n"
            "return(a+b);\n"
            "}\n"
            "void main(){\n"
            "int x;\n"
            "x = f(1);\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "表达式内实参过少程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'd' && d.line == 6) {
                found = true;
            }
        }
        check(found, "表达式内函数调用实参不匹配报 d");
    }
    {
        lexer::Lexer lexer(
            "void g(int a){\n"
            "}\n"
            "void main(){\n"
            "g('x');\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "字符实参传整型形参程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'e' && d.line == 4) {
                found = true;
            }
        }
        check(found, "字符实参传整型形参报 e");
    }
    {
        lexer::Lexer lexer(
            "void g(char a){\n"
            "}\n"
            "void main(){\n"
            "g(1);\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "整型实参传字符形参程序仍可继续解析");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'e' && d.line == 4) {
                found = true;
            }
        }
        check(found, "整型实参传字符形参报 e");
    }
    {
        lexer::Lexer lexer(
            "void g(int a){\n"
            "}\n"
            "void main(){\n"
            "g(1);\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "整型实参传整型形参程序解析成功");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'e') {
                found = true;
            }
        }
        check(!found, "整型实参传整型形参不报 e");
    }
    {
        lexer::Lexer lexer(
            "void g(char a){\n"
            "}\n"
            "void main(){\n"
            "g('x');\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "字符实参传字符形参程序解析成功");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'e') {
                found = true;
            }
        }
        check(!found, "字符实参传字符形参不报 e");
    }
    {
        lexer::Lexer lexer(
            "void g(char a){\n"
            "}\n"
            "void main(){\n"
            "g('x' + 1);\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "字符参与运算后解析成功");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'e' && d.line == 4) {
                found = true;
            }
        }
        check(found, "字符参与运算结果为整型并报 e");
    }
    {
        lexer::Lexer lexer(
            "void g(int a){\n"
            "}\n"
            "void main(){\n"
            "char c;\n"
            "g((c));\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "括号字符表达式程序解析成功");
        bool found = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'e') {
                found = true;
            }
        }
        check(!found, "括号内字符视为整型，不报 e");
    }
    {
        lexer::Lexer lexer(
            "void g(int a){\n"
            "}\n"
            "void main(){\n"
            "g();\n"
            "}\n");
        std::ostringstream out;
        std::vector<common::Diagnostic> diagnostics;
        TestParser parser(lexer, &out,
                          [&diagnostics](const common::Diagnostic& d) {
                              diagnostics.push_back(d);
                          },
                          true);
        check(parser.parse_program(), "参数个数错误程序仍可继续解析");
        bool has_e = false;
        bool has_d = false;
        for (const common::Diagnostic& d : diagnostics) {
            if (d.code == 'e') {
                has_e = true;
            }
            if (d.code == 'd') {
                has_d = true;
            }
        }
        check(has_d && !has_e, "参数个数错误优先报 d，不报 e");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nchar c;\nif (c == 1) {\n}\n}\n", parsed);
        check(parsed, "字符条件程序仍可继续解析");
        check(has_error(diagnostics, 'f', 3), "char 与 int 比较报 f");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nif ('a' == 1) {\n}\n}\n", parsed);
        check(parsed, "字符字面量条件程序仍可继续解析");
        check(has_error(diagnostics, 'f', 2), "字符字面量与 int 比较报 f");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nchar c;\nif (1 == c) {\n}\n}\n", parsed);
        check(parsed, "右侧字符条件程序仍可继续解析");
        check(has_error(diagnostics, 'f', 3), "int 与 char 比较报 f");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint a, b;\nif (a < b) {\n}\n}\n", parsed);
        check(parsed, "整型条件程序解析成功");
        check(!has_any_error(diagnostics, 'f'), "整型条件不报 f");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nchar c;\nif ((c) == 1) {\n}\n}\n", parsed);
        check(parsed, "括号字符条件程序解析成功");
        check(!has_any_error(diagnostics, 'f'), "括号内字符视为整型，不报 f");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nchar c;\nwhile (c < 1) {\n}\n}\n", parsed);
        check(parsed, "字符 while 条件程序仍可继续解析");
        check(has_error(diagnostics, 'f', 3), "while 条件含字符报 f");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint i;\nchar c;\nfor (i = 0; c < 1; i = i + 1) {\n}\n}\n",
            parsed);
        check(parsed, "字符 for 条件程序仍可继续解析");
        check(has_error(diagnostics, 'f', 4), "for 条件含字符报 f");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nchar a, b;\nif (a == b) {\n}\n}\n", parsed);
        check(parsed, "字符间比较程序仍可继续解析");
        check(has_error(diagnostics, 'f', 3), "两个字符比较报 f");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void f(){\nreturn(1);\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "无返回值函数带表达式 return 程序仍可继续解析");
        check(has_error(diagnostics, 'g', 2), "void 函数 return(表达式) 报 g");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void f(){\nreturn();\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "无返回值函数空 return() 程序仍可继续解析");
        check(has_error(diagnostics, 'g', 2), "void 函数 return() 报 g");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void f(){\nreturn;\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "无返回值函数 return; 程序解析成功");
        check(!has_any_error(diagnostics, 'g'), "void 函数 return; 不报 g");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int f(){\nreturn(1);\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "有返回值函数 return(1) 程序解析成功");
        check(!has_any_error(diagnostics, 'g'), "有返回值函数不报 g");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nreturn(1);\n}\n", parsed);
        check(parsed, "主函数带表达式 return 程序仍可继续解析");
        check(has_error(diagnostics, 'g', 2), "主函数 return(表达式) 报 g");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void f(){\nint x;\nif (x > 0) return(1);\n}\nvoid main(){\n}\n",
            parsed);
        check(parsed, "分支内带表达式 return 程序仍可继续解析");
        check(has_error(diagnostics, 'g', 3), "void 函数分支内 return(表达式) 报 g");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int f(){\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "缺少 return 的有返回值函数程序仍可继续解析");
        check(has_error(diagnostics, 'h', 2), "缺少 return 报 h 且行号为右花括号");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int f(){\nreturn;\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "无表达式 return 的有返回值函数程序仍可继续解析");
        check(has_error(diagnostics, 'h', 2), "有返回值函数 return; 报 h");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int f(){\nreturn();\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "空括号 return 的有返回值函数程序仍可继续解析");
        check(has_error(diagnostics, 'h', 2), "有返回值函数 return() 报 h");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int f(){\nreturn(1);\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "正常 return 的有返回值函数解析成功");
        check(!has_any_error(diagnostics, 'h'), "int 函数 return(1) 不报 h");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "char f(){\nreturn(1);\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "返回类型不匹配程序仍可继续解析");
        check(has_error(diagnostics, 'h', 2), "char 函数 return(1) 报 h");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int f(){\nreturn('a');\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "返回字符到整型函数程序仍可继续解析");
        check(has_error(diagnostics, 'h', 2), "int 函数 return('a') 报 h");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "char f(){\nreturn('a');\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "字符函数返回字符解析成功");
        check(!has_any_error(diagnostics, 'h'), "char 函数 return('a') 不报 h");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int f(){\nif (1 < 2) return(1);\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "分支内存在 return 的函数解析成功");
        check(!has_any_error(diagnostics, 'h'), "存在一条有效 return 即不报 h");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int f(){\nreturn(1);\nreturn;\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "有效与无效 return 并存程序仍可继续解析");
        check(has_error(diagnostics, 'h', 3), "无效 return; 报 h");
        check(!has_error(diagnostics, 'h', 4), "已有有效 return 时右花括号不报 h");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2];\nvoid main(){\nint x;\nx = a['x'];\n}\n", parsed);
        check(parsed, "字符下标表达式程序仍可继续解析");
        check(has_error(diagnostics, 'i', 4), "表达式中字符下标报 i");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2];\nvoid main(){\na['x'] = 1;\n}\n", parsed);
        check(parsed, "赋值左侧字符下标程序仍可继续解析");
        check(has_error(diagnostics, 'i', 3), "赋值左侧字符下标报 i");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2][2];\nvoid main(){\nint x;\nx = a['x'][1];\n}\n", parsed);
        check(parsed, "二维数组第一维字符下标程序仍可继续解析");
        check(has_error(diagnostics, 'i', 4), "第一维字符下标报 i");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2][2];\nvoid main(){\nint x;\nx = a[1]['x'];\n}\n", parsed);
        check(parsed, "二维数组第二维字符下标程序仍可继续解析");
        check(has_error(diagnostics, 'i', 4), "第二维字符下标报 i");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2];\nvoid main(){\nchar c;\nint x;\nx = a[c];\n}\n", parsed);
        check(parsed, "字符变量下标程序仍可继续解析");
        check(has_error(diagnostics, 'i', 5), "字符变量下标报 i");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2];\nvoid main(){\nint x;\nx = a[1];\n}\n", parsed);
        check(parsed, "整型下标程序解析成功");
        check(!has_any_error(diagnostics, 'i'), "整型下标不报 i");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2];\nvoid main(){\nint x;\nx = a['x' + 1];\n}\n", parsed);
        check(parsed, "字符参与运算的下标程序解析成功");
        check(!has_any_error(diagnostics, 'i'), "字符参与运算后为整型，不报 i");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2];\nvoid main(){\nchar c;\nint x;\nx = a[(c)];\n}\n", parsed);
        check(parsed, "括号字符下标程序解析成功");
        check(!has_any_error(diagnostics, 'i'), "括号内字符视为整型，不报 i");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "const int N = 1;\nvoid main(){\nN = 2;\n}\n", parsed);
        check(parsed, "常量赋值程序仍可继续解析");
        check(has_error(diagnostics, 'j', 3), "给常量赋值报 j");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "const int N = 1;\nvoid main(){\nscanf(N);\n}\n", parsed);
        check(parsed, "常量读入程序仍可继续解析");
        check(has_error(diagnostics, 'j', 3), "scanf 常量报 j");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int N = 1;\nvoid main(){\nN = 2;\n}\n", parsed);
        check(parsed, "变量赋值程序解析成功");
        check(!has_any_error(diagnostics, 'j'), "变量赋值不报 j");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int N = 1;\nvoid main(){\nscanf(N);\n}\n", parsed);
        check(parsed, "变量读入程序解析成功");
        check(!has_any_error(diagnostics, 'j'), "scanf 变量不报 j");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "const char C = 'a';\nvoid main(){\nC = 'b';\n}\n", parsed);
        check(parsed, "字符常量赋值程序仍可继续解析");
        check(has_error(diagnostics, 'j', 3), "给字符常量赋值报 j");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "const int N = 1;\nvoid main(){\nint x;\nx = N + 1;\n}\n", parsed);
        check(parsed, "常量参与表达式程序解析成功");
        check(!has_any_error(diagnostics, 'j'), "常量参与表达式不报 j");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void f(){\nconst int N = 1;\nN = 2;\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "局部常量赋值程序仍可继续解析");
        check(has_error(diagnostics, 'j', 3), "局部常量赋值报 j");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2];\nvoid main(){\na[0] = 1;\n}\n", parsed);
        check(parsed, "数组元素赋值程序解析成功");
        check(!has_any_error(diagnostics, 'j'), "数组元素赋值不报 j");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nscanf(x)\nx = 1;\n}\n", parsed);
        check(parsed, "读语句缺分号程序仍可继续解析");
        check(has_error(diagnostics, 'k', 3), "读语句缺分号报 k");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nprintf(x)\nx = 1;\n}\n", parsed);
        check(parsed, "写语句缺分号程序仍可继续解析");
        check(has_error(diagnostics, 'k', 3), "写语句缺分号报 k");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int f(){\nreturn(1)\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "返回语句缺分号程序仍可继续解析");
        check(has_error(diagnostics, 'k', 2), "返回语句缺分号报 k");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void g(){\n}\nvoid main(){\ng()\n}\n", parsed);
        check(parsed, "函数调用缺分号程序仍可继续解析");
        check(has_error(diagnostics, 'k', 4), "函数调用缺分号报 k");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint i;\nfor (i = 0 i < 3; i = i + 1) {\n}\n}\n",
            parsed);
        check(parsed, "for 初始化缺分号程序仍可继续解析");
        check(has_error(diagnostics, 'k', 3), "for 初始化缺分号报 k");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint i;\nfor (i = 0; i < 3 i = i + 1) {\n}\n}\n",
            parsed);
        check(parsed, "for 条件缺分号程序仍可继续解析");
        check(has_error(diagnostics, 'k', 3), "for 条件缺分号报 k");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "const int A = 1\nint x;\nvoid main(){\n}\n", parsed);
        check(parsed, "常量定义缺分号程序仍可继续解析");
        check(has_error(diagnostics, 'k', 1), "常量定义缺分号报 k");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x\nx = 1;\n}\n", parsed);
        check(parsed, "变量定义缺分号程序仍可继续解析");
        check(has_error(diagnostics, 'k', 2), "变量定义缺分号报 k");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nx = 1;\n}\n", parsed);
        check(parsed, "分号完整程序解析成功");
        check(!has_any_error(diagnostics, 'k'), "分号完整不报 k");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nif (x < 1 {\n}\n}\n", parsed);
        check(parsed, "if 缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'l', 3), "if 缺右括号报 l");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nwhile (x < 1 {\n}\n}\n", parsed);
        check(parsed, "while 缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'l', 3), "while 缺右括号报 l");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint i;\nfor (i = 0; i < 3; i = i + 1 {\n}\n}\n",
            parsed);
        check(parsed, "for 缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'l', 3), "for 缺右括号报 l");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nswitch (x {\ncase 1:\nx = 1;\ndefault:\nx = 2;\n}\n}\n",
            parsed);
        check(parsed, "switch 缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'l', 3), "switch 缺右括号报 l");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int f(int a {\nreturn(1);\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "函数定义缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'l', 1), "函数定义缺右括号报 l");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main( {\n}\n", parsed);
        check(parsed, "主函数缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'l', 1), "主函数缺右括号报 l");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void g(){\n}\nvoid main(){\ng(1;\n}\n", parsed);
        check(parsed, "函数调用缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'l', 4), "函数调用缺右括号报 l");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int f(){\nreturn(1;\n}\nvoid main(){\n}\n", parsed);
        check(parsed, "return 缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'l', 2), "return 缺右括号报 l");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nx = (1 + 2;\n}\n", parsed);
        check(parsed, "括号表达式缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'l', 3), "括号表达式缺右括号报 l");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nscanf(x;\n}\n", parsed);
        check(parsed, "scanf 缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'l', 3), "scanf 缺右括号报 l");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nx = (1 + 2);\n}\n", parsed);
        check(parsed, "括号完整程序解析成功");
        check(!has_any_error(diagnostics, 'l'), "括号完整不报 l");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2;\nvoid main(){\n}\n", parsed);
        check(parsed, "一维数组定义缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'm', 1), "一维数组定义缺右中括号报 m");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2[3];\nvoid main(){\n}\n", parsed);
        check(parsed, "二维数组第一维缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'm', 1), "二维数组第一维缺右中括号报 m");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2][3;\nvoid main(){\n}\n", parsed);
        check(parsed, "二维数组第二维缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'm', 1), "二维数组第二维缺右中括号报 m");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint a[2;\n}\n", parsed);
        check(parsed, "局部数组定义缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'm', 2), "局部数组定义缺右中括号报 m");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2];\nvoid main(){\nint x;\nx = a[1;\n}\n", parsed);
        check(parsed, "数组元素表达式缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'm', 4), "数组元素表达式缺右中括号报 m");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2];\nvoid main(){\na[1 = 2;\n}\n", parsed);
        check(parsed, "数组元素赋值缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'm', 3), "数组元素赋值缺右中括号报 m");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2][2];\nvoid main(){\nint x;\nx = a[1][2;\n}\n", parsed);
        check(parsed, "二维数组元素第二维缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'm', 4), "二维数组元素第二维缺右中括号报 m");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2 = {1, 2};\nvoid main(){\n}\n", parsed);
        check(parsed, "数组初始化缺右括号程序仍可继续解析");
        check(has_error(diagnostics, 'm', 1), "数组初始化缺右中括号报 m");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2][2];\nvoid main(){\nint x;\nx = a[1][1];\n}\n", parsed);
        check(parsed, "数组下标完整程序解析成功");
        check(!has_any_error(diagnostics, 'm'), "数组下标完整不报 m");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2] = {1, 2, 3};\nvoid main(){\n}\n", parsed);
        check(parsed, "一维数组初始化过多程序仍可继续解析");
        check(has_error(diagnostics, 'n', 1), "一维数组初始化过多报 n");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2] = {1};\nvoid main(){\n}\n", parsed);
        check(parsed, "一维数组初始化过少程序仍可继续解析");
        check(has_error(diagnostics, 'n', 1), "一维数组初始化过少报 n");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2] = {1, 2};\nvoid main(){\n}\n", parsed);
        check(parsed, "一维数组初始化个数正确程序解析成功");
        check(!has_any_error(diagnostics, 'n'), "一维数组初始化个数正确不报 n");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2][2] = {{1, 2}};\nvoid main(){\n}\n", parsed);
        check(parsed, "二维数组行数不足程序仍可继续解析");
        check(has_error(diagnostics, 'n', 1), "二维数组行数不足报 n");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2][2] = {{1, 2}, {3}};\nvoid main(){\n}\n", parsed);
        check(parsed, "二维数组列数不足程序仍可继续解析");
        check(has_error(diagnostics, 'n', 1), "二维数组列数不足报 n");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2][2] = {{1, 2}, {3, 4}};\nvoid main(){\n}\n", parsed);
        check(parsed, "二维数组初始化个数正确程序解析成功");
        check(!has_any_error(diagnostics, 'n'), "二维数组初始化个数正确不报 n");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint a[2] = {1};\n}\n", parsed);
        check(parsed, "局部数组初始化个数错误程序仍可继续解析");
        check(has_error(diagnostics, 'n', 2), "局部数组初始化个数错误报 n");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "char a[2] = {'a', 'b'};\nvoid main(){\n}\n", parsed);
        check(parsed, "字符数组初始化个数正确程序解析成功");
        check(!has_any_error(diagnostics, 'n'), "字符数组初始化个数正确不报 n");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x = 'a';\n}\n", parsed);
        check(parsed, "int 变量初始化字符常量程序仍可继续解析");
        check(has_error(diagnostics, 'o', 2), "int 变量初始化字符常量报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nchar c = 1;\n}\n", parsed);
        check(parsed, "char 变量初始化整型常量程序仍可继续解析");
        check(has_error(diagnostics, 'o', 2), "char 变量初始化整型常量报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x = 1;\n}\n", parsed);
        check(parsed, "int 变量初始化整型常量程序解析成功");
        check(!has_any_error(diagnostics, 'o'), "int 变量初始化整型常量不报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nchar c = 'a';\n}\n", parsed);
        check(parsed, "char 变量初始化字符常量程序解析成功");
        check(!has_any_error(diagnostics, 'o'), "char 变量初始化字符常量不报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2] = {1, 'a'};\nvoid main(){\n}\n", parsed);
        check(parsed, "int 数组初始化含字符常量程序仍可继续解析");
        check(has_error(diagnostics, 'o', 1), "int 数组初始化含字符常量报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "char a[2] = {'a', 1};\nvoid main(){\n}\n", parsed);
        check(parsed, "char 数组初始化含整型常量程序仍可继续解析");
        check(has_error(diagnostics, 'o', 1), "char 数组初始化含整型常量报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2] = {1, 2};\nvoid main(){\n}\n", parsed);
        check(parsed, "int 数组初始化类型一致程序解析成功");
        check(!has_any_error(diagnostics, 'o'), "int 数组类型一致不报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "int a[2][2] = {{1, 2}, {3, 'a'}};\nvoid main(){\n}\n", parsed);
        check(parsed, "二维数组初始化含字符常量程序仍可继续解析");
        check(has_error(diagnostics, 'o', 1), "二维数组初始化含字符常量报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nswitch (x) {\ncase 'a':\nx = 1;\ndefault:\nx = 2;\n}\n}\n",
            parsed);
        check(parsed, "int switch 使用字符 case 程序仍可继续解析");
        check(has_error(diagnostics, 'o', 4), "int switch 字符 case 报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nchar c;\nswitch (c) {\ncase 1:\nc = 'a';\ndefault:\nc = 'b';\n}\n}\n",
            parsed);
        check(parsed, "char switch 使用整型 case 程序仍可继续解析");
        check(has_error(diagnostics, 'o', 4), "char switch 整型 case 报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nswitch (x) {\ncase 1:\nx = 1;\ndefault:\nx = 2;\n}\n}\n",
            parsed);
        check(parsed, "int switch 整型 case 程序解析成功");
        check(!has_any_error(diagnostics, 'o'), "int switch 整型 case 不报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nchar c;\nswitch (c) {\ncase 'a':\nc = 'b';\ndefault:\nc = 'c';\n}\n}\n",
            parsed);
        check(parsed, "char switch 字符 case 程序解析成功");
        check(!has_any_error(diagnostics, 'o'), "char switch 字符 case 不报 o");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nswitch (x) {\ncase 1:\nx = 1;\n}\n}\n", parsed);
        check(parsed, "switch 缺 default 程序仍可继续解析");
        check(has_error(diagnostics, 'p', 6), "switch 缺 default 报 p");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nswitch (x) {\ncase 1:\nx = 1;\ndefault:\nx = 2;\n}\n}\n",
            parsed);
        check(parsed, "switch 含 default 程序解析成功");
        check(!has_any_error(diagnostics, 'p'), "switch 含 default 不报 p");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nswitch (x) {\ncase 1:\nx = 1;\ncase 2:\nx = 2;\n}\n}\n",
            parsed);
        check(parsed, "多 case 缺 default 程序仍可继续解析");
        check(has_error(diagnostics, 'p', 8), "多 case 缺 default 报 p");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void main(){\nint x;\nswitch (x) {\ncase 1:\nswitch (x) {\ncase 2:\nx = 2;\n}\ndefault:\nx = 3;\n}\n}\n",
            parsed);
        check(parsed, "嵌套 switch 缺 default 程序仍可继续解析");
        check(has_error(diagnostics, 'p', 8), "内层 switch 缺 default 报 p");
    }
    {
        bool parsed = false;
        const std::vector<common::Diagnostic> diagnostics = collect_errors(
            "void f(){\nint x;\nswitch (x) {\ncase 1:\nx = 1;\n}\n}\nvoid main(){\n}\n",
            parsed);
        check(parsed, "函数内 switch 缺 default 程序仍可继续解析");
        check(has_error(diagnostics, 'p', 6), "函数内 switch 缺 default 报 p");
    }
    {
        lexer::Lexer lexer(
            "const int N = -5;\nint a;\nchar b;\nint m[2][3];\n"
            "void main(){\nint c;\n}\n");
        std::ostringstream out;
        TestParser parser(lexer, &out, nullptr, true);
        check(parser.parse_program(), "地址分配程序解析成功");

        const semantic::Symbol* a = parser.symbols().lookup("a");
        const semantic::Symbol* b = parser.symbols().lookup("b");
        const semantic::Symbol* m = parser.symbols().lookup("m");
        const semantic::Symbol* n = parser.symbols().lookup("N");
        check(a != nullptr && a->address == 0 &&
                  a->type == semantic::DataType::Int,
              "全局 int 变量地址为 0");
        check(b != nullptr && b->address == 1 &&
                  b->type == semantic::DataType::Char,
              "全局 char 变量地址为 1");
        check(m != nullptr && m->address == 2 &&
                  m->kind == semantic::SymbolKind::Array,
              "二维数组起始地址为 2");
        check(m != nullptr && m->dimension_sizes.size() == 2 &&
                  m->dimension_sizes[0] == 2 && m->dimension_sizes[1] == 3,
              "二维数组维度大小保留");
        check(m != nullptr && semantic::element_address(*m, 1, 2) == 7,
              "m[1][2] 行优先地址为 7");
        check(parser.next_address() == 9, "局部变量分配后地址计数为 9");
        check(n != nullptr && n->kind == semantic::SymbolKind::Constant &&
                  n->has_value && n->int_value == -5,
              "整型常量值保留");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code = generate(
            "const int N = 5;\nint a;\nchar c;\nvoid main(){\n}\n", parsed);
        check(parsed, "无初始化说明程序解析成功");
        check(code.size() == 1 && code[0].op == pcode::OpCode::End,
              "无初始化的说明只生成 END");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("int a = 3;\nvoid main(){\n}\n", parsed);
        check(parsed, "简单变量初始化程序解析成功");
        check(code.size() == 3 && code[0].op == pcode::OpCode::Lit &&
                  code[0].operand == 3 && code[1].op == pcode::OpCode::Sto &&
                  code[1].operand == 0 && code[2].op == pcode::OpCode::End,
              "简单变量初始化生成 LIT 3 / STO 0 / END");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("int a[2] = {1, 2};\nvoid main(){\n}\n", parsed);
        check(parsed, "一维数组初始化程序解析成功");
        check(code.size() == 5, "一维数组初始化指令数为 5");
        check(code[0].op == pcode::OpCode::Lit && code[0].operand == 1 &&
                  code[1].op == pcode::OpCode::Sto && code[1].operand == 0,
              "a[0] 初始化生成 LIT 1 / STO 0");
        check(code[2].op == pcode::OpCode::Lit && code[2].operand == 2 &&
                  code[3].op == pcode::OpCode::Sto && code[3].operand == 1,
              "a[1] 初始化生成 LIT 2 / STO 1");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code = generate(
            "int m[2][2] = {{1, 2}, {3, 4}};\nvoid main(){\n}\n", parsed);
        check(parsed, "二维数组初始化程序解析成功");
        check(code.size() == 9, "二维数组初始化指令数为 9");
        check(code[4].operand == 3 && code[5].operand == 2 &&
                  code[6].operand == 4 && code[7].operand == 3,
              "二维数组按行优先写入地址 2、3");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("char c = 'a';\nvoid main(){\n}\n", parsed);
        check(parsed, "字符变量初始化程序解析成功");
        check(code.size() == 3 && code[0].op == pcode::OpCode::Lit &&
                  code[0].operand == 97 && code[1].op == pcode::OpCode::Sto &&
                  code[1].operand == 0,
              "字符变量初始化生成 ASCII 码 LIT 97");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("void main(){\nint x = 4;\n}\n", parsed);
        check(parsed, "局部变量初始化程序解析成功");
        check(code.size() == 3 && code[0].op == pcode::OpCode::Lit &&
                  code[0].operand == 4 && code[1].op == pcode::OpCode::Sto &&
                  code[1].operand == 0,
              "局部变量初始化生成 LIT 4 / STO 0");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("int x;\nvoid main(){\nx = 2 + 3 * 4;\n}\n", parsed);
        check(parsed && code.size() == 7, "表达式赋值指令数为 7");
        check(code[0].op == pcode::OpCode::Lit && code[0].operand == 2 &&
                  code[1].op == pcode::OpCode::Lit && code[1].operand == 3 &&
                  code[2].op == pcode::OpCode::Lit && code[2].operand == 4,
              "表达式操作数按后序生成");
        check(code[3].op == pcode::OpCode::Opr &&
                  code[3].operand ==
                      static_cast<int>(pcode::OprCode::Mul) &&
                  code[4].op == pcode::OpCode::Opr &&
                  code[4].operand == static_cast<int>(pcode::OprCode::Add),
              "乘加运算生成正确");
        check(code[5].op == pcode::OpCode::Sto && code[5].operand == 0 &&
                  code[6].op == pcode::OpCode::End,
              "赋值生成 STO 0");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("int x;\nvoid main(){\nx = -5;\n}\n", parsed);
        check(parsed && code.size() == 4, "负号赋值指令数为 4");
        check(code[0].op == pcode::OpCode::Lit && code[0].operand == 5 &&
                  code[1].op == pcode::OpCode::Opr &&
                  code[1].operand == static_cast<int>(pcode::OprCode::Neg),
              "负号生成 LIT 5 / NEG");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("int x;\nvoid main(){\nx = 8 / 2;\n}\n", parsed);
        check(parsed && code.size() == 5, "除法赋值指令数为 5");
        check(code[2].op == pcode::OpCode::Opr &&
                  code[2].operand == static_cast<int>(pcode::OprCode::Div),
              "除法生成 DIV");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("int x;\nvoid main(){\nx = (2 + 3) * 4;\n}\n", parsed);
        check(parsed && code.size() == 7, "括号表达式指令数为 7");
        check(code[2].op == pcode::OpCode::Opr &&
                  code[2].operand == static_cast<int>(pcode::OprCode::Add) &&
                  code[4].op == pcode::OpCode::Opr &&
                  code[4].operand == static_cast<int>(pcode::OprCode::Mul),
              "括号优先：先加后乘");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code = generate(
            "const int N = 5;\nint x;\nvoid main(){\nx = N + 1;\n}\n", parsed);
        check(parsed && code.size() == 5, "常量参与运算指令数为 5");
        check(code[0].op == pcode::OpCode::Lit && code[0].operand == 5 &&
                  code[1].op == pcode::OpCode::Lit && code[1].operand == 1,
              "常量引用生成 LIT 5");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code = generate(
            "int a[2];\nvoid main(){\nint x;\nx = a[1];\n}\n", parsed);
        check(parsed && code.size() == 6, "一维数组读取指令数为 6");
        check(code[0].op == pcode::OpCode::Lit && code[0].operand == 0 &&
                  code[1].op == pcode::OpCode::Lit && code[1].operand == 1 &&
                  code[2].op == pcode::OpCode::Opr &&
                  code[2].operand == static_cast<int>(pcode::OprCode::Add) &&
                  code[3].op == pcode::OpCode::Ldi,
              "一维数组读取生成 基址+下标 / LDI");
        check(code[4].op == pcode::OpCode::Sto && code[4].operand == 2,
              "数组元素存入局部变量地址 2");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code = generate(
            "int a[2];\nvoid main(){\na[1] = 7;\n}\n", parsed);
        check(parsed && code.size() == 6, "一维数组赋值指令数为 6");
        check(code[0].op == pcode::OpCode::Lit && code[0].operand == 0 &&
                  code[1].op == pcode::OpCode::Lit && code[1].operand == 1 &&
                  code[2].op == pcode::OpCode::Opr && code[3].op == pcode::OpCode::Lit &&
                  code[3].operand == 7 && code[4].op == pcode::OpCode::Sti,
              "一维数组赋值生成 地址 / LIT 7 / STI");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code = generate(
            "int m[2][2];\nvoid main(){\nint x;\nx = m[1][0];\n}\n", parsed);
        check(parsed && code.size() == 10, "二维数组读取指令数为 10");
        check(code[0].operand == 0 && code[1].operand == 1 &&
                  code[2].operand == 2 && code[3].op == pcode::OpCode::Opr &&
                  code[3].operand == static_cast<int>(pcode::OprCode::Mul) &&
                  code[4].op == pcode::OpCode::Opr &&
                  code[4].operand == static_cast<int>(pcode::OprCode::Add) &&
                  code[5].operand == 0 && code[6].op == pcode::OpCode::Opr &&
                  code[6].operand == static_cast<int>(pcode::OprCode::Add) &&
                  code[7].op == pcode::OpCode::Ldi,
              "二维数组地址 = base + i*cols + j / LDI");
        check(code[8].op == pcode::OpCode::Sto && code[8].operand == 4,
              "二维数组元素存入局部变量地址 4");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code = generate(
            "int m[2][2];\nvoid main(){\nm[1][0] = 7;\n}\n", parsed);
        check(parsed && code.size() == 10, "二维数组赋值指令数为 10");
        check(code[7].op == pcode::OpCode::Lit && code[7].operand == 7 &&
                  code[8].op == pcode::OpCode::Sti,
              "二维数组赋值生成 LIT 7 / STI");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code = generate(
            "const char C = 'a';\nint x;\nvoid main(){\nx = C + 1;\n}\n", parsed);
        check(parsed && code.size() == 5, "字符常量运算指令数为 5");
        check(code[0].op == pcode::OpCode::Lit && code[0].operand == 97,
              "字符常量引用生成 ASCII 码 LIT 97");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("int a;\nint x;\nvoid main(){\nx = a;\n}\n", parsed);
        check(parsed && code.size() == 3, "变量读取指令数为 3");
        check(code[0].op == pcode::OpCode::Lod && code[0].operand == 0 &&
                  code[1].op == pcode::OpCode::Sto && code[1].operand == 1,
              "变量读取生成 LOD 0 / STO 1");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("void main(){\nprintf(\"Hello World\");\n}\n", parsed);
        check(parsed && code.size() == 3, "字符串写语句指令数为 3");
        check(code[0].op == pcode::OpCode::WrtStr &&
                  code[0].text == "Hello World" &&
                  code[1].op == pcode::OpCode::Opr &&
                  code[1].operand ==
                      static_cast<int>(pcode::OprCode::WrtNl),
              "字符串写语句生成 WRT_STR / WRT_NL");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("void main(){\nint x;\nscanf(x);\n}\n", parsed);
        check(parsed && code.size() == 3, "整数读语句指令数为 3");
        check(code[0].op == pcode::OpCode::Opr &&
                  code[0].operand ==
                      static_cast<int>(pcode::OprCode::ReadInt) &&
                  code[1].op == pcode::OpCode::Sto && code[1].operand == 0,
              "整数读语句生成 READ_INT / STO 0");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("void main(){\nchar c;\nscanf(c);\n}\n", parsed);
        check(parsed && code.size() == 3, "字符读语句指令数为 3");
        check(code[0].op == pcode::OpCode::Opr &&
                  code[0].operand ==
                      static_cast<int>(pcode::OprCode::ReadChar) &&
                  code[1].op == pcode::OpCode::Sto && code[1].operand == 0,
              "字符读语句生成 READ_CHAR / STO 0");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("void main(){\nint x;\nprintf(x);\n}\n", parsed);
        check(parsed && code.size() == 4, "整数写语句指令数为 4");
        check(code[0].op == pcode::OpCode::Lod && code[0].operand == 0 &&
                  code[1].op == pcode::OpCode::Opr &&
                  code[1].operand ==
                      static_cast<int>(pcode::OprCode::WrtInt) &&
                  code[2].op == pcode::OpCode::Opr &&
                  code[2].operand ==
                      static_cast<int>(pcode::OprCode::WrtNl),
              "整数写语句生成 LOD / WRT_INT / WRT_NL");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("void main(){\nchar c;\nprintf(c);\n}\n", parsed);
        check(parsed && code.size() == 4, "字符写语句指令数为 4");
        check(code[0].op == pcode::OpCode::Lod && code[1].op == pcode::OpCode::Opr &&
                  code[1].operand ==
                      static_cast<int>(pcode::OprCode::WrtChar),
              "字符写语句生成 WRT_CHAR");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code = generate(
            "void main(){\nint x;\nprintf(\"x=\", x);\n}\n", parsed);
        check(parsed && code.size() == 5, "字符串+表达式写语句指令数为 5");
        check(code[0].op == pcode::OpCode::WrtStr && code[0].text == "x=" &&
                  code[1].op == pcode::OpCode::Lod && code[1].operand == 0 &&
                  code[2].op == pcode::OpCode::Opr &&
                  code[2].operand ==
                      static_cast<int>(pcode::OprCode::WrtInt) &&
                  code[3].op == pcode::OpCode::Opr &&
                  code[3].operand ==
                      static_cast<int>(pcode::OprCode::WrtNl),
              "字符串+表达式写语句顺序正确");
    }
    {
        bool parsed = false;
        const std::vector<pcode::Instruction> code =
            generate("void main(){\nprintf('a');\n}\n", parsed);
        check(parsed && code.size() == 4, "字符常量写语句指令数为 4");
        check(code[0].op == pcode::OpCode::Lit && code[0].operand == 97 &&
                  code[1].op == pcode::OpCode::Opr &&
                  code[1].operand ==
                      static_cast<int>(pcode::OprCode::WrtChar) &&
                  code[2].op == pcode::OpCode::Opr &&
                  code[2].operand ==
                      static_cast<int>(pcode::OprCode::WrtNl),
              "字符常量写语句生成 LIT 97 / WRT_CHAR / WRT_NL");
    }

    if (failures == 0) {
        std::cout << "全部通过\n";
        return 0;
    }
    std::cout << failures << " 项失败\n";
    return 1;
}
