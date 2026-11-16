// 字符扫描器（P1-2）
// 负责：游标前进、行号/列号维护、空白跳过、文件尾判断。
// 构造时把 CRLF 与单独的 CR 统一为 '\n'，保证行号只加一次。
#pragma once

#include <cstddef>
#include <string>

namespace lexer {

class Scanner {
public:
    explicit Scanner(const std::string& source);

    // 是否已到达文件尾。
    bool at_end() const { return offset_ >= source_.size(); }

    // 当前字符；文件尾返回 '\0'。不移动游标。
    char peek() const { return at_end() ? '\0' : source_[offset_]; }

    // 下一个字符；越界返回 '\0'。不移动游标。
    char peek_next() const {
        return offset_ + 1 < source_.size() ? source_[offset_ + 1] : '\0';
    }

    // 返回当前字符并前进；文件尾返回 '\0' 且不移动。
    char advance();

    // 跳过空格、制表符、换行、回车、垂直制表、换页。
    void skip_whitespace();

    int line() const { return line_; }
    int column() const { return column_; }
    std::size_t offset() const { return offset_; }
    const std::string& source() const { return source_; }

private:
    std::string source_;  // 已统一换行为 '\n'
    std::size_t offset_ = 0;
    int line_ = 1;
    int column_ = 1;
};

}  // namespace lexer
