#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "common/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

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
    using parser::Parser::parse_condition;
    using parser::Parser::parse_conditional_statement;
    using parser::Parser::parse_declaration_header;
    using parser::Parser::parse_expression;
    using parser::Parser::parse_factor;
    using parser::Parser::parse_function_call;
    using parser::Parser::parse_function_definition;
    using parser::Parser::parse_main_function;
    using parser::Parser::parse_parameter_list;
    using parser::Parser::parse_read_statement;
    using parser::Parser::parse_return_statement;
    using parser::Parser::parse_statement;
    using parser::Parser::parse_term;
    using parser::Parser::parse_value_parameter_list;
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
                  diagnostics[0].column == 3,
              "语法错误位置为当前单词");
        check(diagnostics.size() == 1 && diagnostics[0].code == '\0',
              "语法错误类别码暂未分类");
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

    if (failures == 0) {
        std::cout << "全部通过\n";
        return 0;
    }
    std::cout << failures << " 项失败\n";
    return 1;
}
