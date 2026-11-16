// 词法分析器（P1-9 起支持错误回调）
#pragma once

#include <functional>
#include <string>

#include "common/diagnostic.h"
#include "lexer/scanner.h"
#include "lexer/token.h"

namespace lexer {

class Lexer {
public:
    // 词法错误回调：非法字符、非法字符/字符串常量等。
    using ErrorCallback = std::function<void(const common::Diagnostic&)>;

    explicit Lexer(const std::string& source, ErrorCallback on_error = {});

    // 返回下一个词法单元；文件尾返回 End，非法输入返回 Error。
    Token next_token();

private:
    // 从 start 到当前游标的源码片段。
    std::string slice_from(std::size_t start) const;

    Token scan_identifier_or_keyword();
    Token scan_number();
    Token scan_char_constant();
    Token scan_string_constant();
    Token scan_operator_or_delimiter();

    // 词法错误恢复：跳到闭合引号或换行，避免吞掉后续语句。
    void skip_char_literal_tail();
    void skip_string_literal_tail();

    Scanner scanner_;
    ErrorCallback on_error_;
};

}  // namespace lexer
