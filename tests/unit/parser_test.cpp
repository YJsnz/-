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

    if (failures == 0) {
        std::cout << "全部通过\n";
        return 0;
    }
    std::cout << failures << " 项失败\n";
    return 1;
}
