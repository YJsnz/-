#include "lexer/scanner.h"

namespace lexer {
namespace {

// 把 CRLF 与单独的 CR 统一成 '\n'，避免行号重复递增。
std::string normalize_newlines(const std::string& source) {
    std::string out;
    out.reserve(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\r') {
            if (i + 1 < source.size() && source[i + 1] == '\n') {
                ++i;
            }
            out.push_back('\n');
        } else {
            out.push_back(source[i]);
        }
    }
    return out;
}

}  // namespace

Scanner::Scanner(const std::string& source) : source_(normalize_newlines(source)) {}

char Scanner::advance() {
    if (at_end()) {
        return '\0';
    }
    const char c = source_[offset_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

void Scanner::skip_whitespace() {
    while (!at_end()) {
        const char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
            advance();
        } else {
            break;
        }
    }
}

}  // namespace lexer
