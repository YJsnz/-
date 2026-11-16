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
    using parser::Parser::parse_declaration_header;
    using parser::Parser::parse_function_definition;
    using parser::Parser::parse_main_function;
    using parser::Parser::parse_parameter_list;
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
            "RPARENT )\n"
            "SEMICN ;\n"
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
        lexer::Lexer lexer("void g(){ if (x) { } }");
        std::ostringstream out;
        TestParser parser(lexer, &out);

        check(parser.parse_function_definition(), "嵌套花括号的函数体解析成功");
        check(parser.at_end(), "嵌套函数体消费完所有单词");
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

    if (failures == 0) {
        std::cout << "全部通过\n";
        return 0;
    }
    std::cout << failures << " 项失败\n";
    return 1;
}
