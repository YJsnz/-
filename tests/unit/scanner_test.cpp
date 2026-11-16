#include <iostream>
#include <string>

#include "lexer/scanner.h"

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

}  // namespace

int main() {
    {
        lexer::Scanner s("  \t\r\n  int a;");
        s.skip_whitespace();
        check(!s.at_end(), "跳过空白后未到文件尾");
        check(s.peek() == 'i', "跳过空白后定位到 i");
        check(s.line() == 2, "CRLF 只计一行（line == 2）");
        check(s.column() == 3, "跳过空白后 column == 3");
    }
    {
        lexer::Scanner s("abc\ndef");
        check(s.peek() == 'a' && s.line() == 1 && s.column() == 1, "初始位置为 1:1");
        s.advance();
        s.advance();
        check(s.column() == 3, "行内前进时列号递增");
        s.advance();  // c
        s.advance();  // 换行
        check(s.line() == 2 && s.column() == 1, "换行后行号 +1、列号复位");
        check(s.peek() == 'd', "换行后定位到下一行首字符");
    }
    {
        lexer::Scanner s("a\rb");
        s.advance();  // a
        s.advance();  // CR -> LF
        check(s.line() == 2 && s.column() == 1, "单独的 CR 也计为换行");
    }
    {
        lexer::Scanner s("ab");
        check(s.peek_next() == 'b', "peek_next 返回下一个字符");
        s.advance();
        check(s.peek_next() == '\0', "peek_next 越过文件尾返回 '\\0'");
    }
    {
        lexer::Scanner s("");
        check(s.at_end(), "空文件立即到达文件尾");
        check(s.peek() == '\0', "文件尾 peek 返回 '\\0'");
        check(s.advance() == '\0' && s.offset() == 0, "文件尾 advance 不移动游标");
    }
    {
        lexer::Scanner s("a b\tc");
        s.advance();
        s.skip_whitespace();
        check(s.peek() == 'b', "空格被跳过");
        s.advance();
        s.skip_whitespace();
        check(s.peek() == 'c' && s.column() == 5, "制表符按单字符推进列号");
    }
    {
        lexer::Scanner s("  x");
        check(s.line() == 1 && s.column() == 1, "构造后位置为 1:1");
        s.skip_whitespace();
        check(s.peek() == 'x' && s.column() == 3, "前导空格后 column == 3");
    }

    if (failures == 0) {
        std::cout << "全部通过\n";
        return 0;
    }
    std::cout << failures << " 项失败\n";
    return 1;
}
