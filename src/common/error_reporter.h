// 诊断收集器：实验一阶段只记录，不输出 error.txt。
#pragma once

#include <string>
#include <vector>

#include "common/diagnostic.h"

namespace common {

class ErrorReporter {
public:
    void report(const Diagnostic& diagnostic);

    // 词法错误的便捷入口：类别码固定为 'a'。
    void report_lexical(int line, int column, const std::string& message);

    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }
    bool has_errors() const { return !diagnostics_.empty(); }
    void clear();

private:
    std::vector<Diagnostic> diagnostics_;
};

}  // namespace common
