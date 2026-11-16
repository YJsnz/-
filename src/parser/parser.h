// 递归下降语法分析框架（P2-1）
#pragma once

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

    // P2-1 临时实现：消费全部单词。P2-2 起替换为真实语法分析。
    bool parse_program();

    const lexer::Token& current() const { return current_; }
    bool at_end() const { return current_.kind == lexer::TokenKind::End; }
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

private:
    lexer::Lexer& lexer_;
    std::ostream* out_;
    SyntaxErrorCallback on_error_;
    lexer::Token current_;
    bool ok_ = true;
};

}  // namespace parser
