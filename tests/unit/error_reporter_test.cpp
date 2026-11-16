#include <iostream>
#include <string>

#include "common/error_reporter.h"

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
    common::ErrorReporter reporter;

    check(!reporter.has_errors(), "初始无错误");
    check(reporter.diagnostics().empty(), "初始诊断列表为空");

    reporter.report_lexical(3, 5, "未识别的字符 '@'");
    check(reporter.has_errors(), "报告后 has_errors 为真");
    check(reporter.diagnostics().size() == 1, "记录一条诊断");

    const common::Diagnostic& first = reporter.diagnostics()[0];
    check(first.line == 3 && first.column == 5, "行列号正确");
    check(first.code == 'a', "词法错误类别码为 a");
    check(first.message == "未识别的字符 '@'", "消息保留");

    common::Diagnostic other;
    other.line = 7;
    other.column = 2;
    other.code = 'k';
    other.message = "缺少分号";
    reporter.report(other);
    check(reporter.diagnostics().size() == 2, "可记录其他类别码");
    check(reporter.diagnostics()[1].code == 'k', "类别码 k 保留");

    reporter.clear();
    check(!reporter.has_errors(), "clear 后无错误");
    check(reporter.diagnostics().empty(), "clear 后列表为空");

    {
        common::ErrorReporter sorted_reporter;
        common::Diagnostic first;
        first.line = 5;
        first.code = 'c';
        sorted_reporter.report(first);
        common::Diagnostic second;
        second.line = 2;
        second.code = 'b';
        sorted_reporter.report(second);
        common::Diagnostic third;
        third.line = 2;
        third.code = 'd';
        sorted_reporter.report(third);
        common::Diagnostic fourth;
        fourth.line = 3;
        fourth.code = 'k';
        sorted_reporter.report(fourth);

        const std::vector<common::Diagnostic> sorted =
            sorted_reporter.sorted_unique_by_line();
        check(sorted.size() == 3, "按行去重后保留 3 条");
        check(sorted.size() == 3 && sorted[0].line == 2 && sorted[0].code == 'b',
              "同行保留首个错误");
        check(sorted.size() == 3 && sorted[1].line == 3 && sorted[2].line == 5,
              "错误按行号升序排列");
    }

    if (failures == 0) {
        std::cout << "全部通过\n";
        return 0;
    }
    std::cout << failures << " 项失败\n";
    return 1;
}
