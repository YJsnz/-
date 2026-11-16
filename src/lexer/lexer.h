// 词法分析器（P1-3：标识符与保留字）
#pragma once

#include <string>

#include "lexer/scanner.h"
#include "lexer/token.h"

namespace lexer {

class Lexer {
public:
    explicit Lexer(const std::string& source);

    // 返回下一个词法单元；文件尾返回 End，非法字符返回 Error。
    Token next_token();

private:
    Token scan_identifier_or_keyword();
    Token scan_number();
    Token scan_char_constant();
    Token scan_string_constant();
    Token scan_operator_or_delimiter();

    Scanner scanner_;
};

}  // namespace lexer
