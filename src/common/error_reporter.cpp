#include "common/error_reporter.h"

#include <algorithm>

namespace common {

void ErrorReporter::report(const Diagnostic& diagnostic) {
    diagnostics_.push_back(diagnostic);
}

void ErrorReporter::report_lexical(int line, int column, const std::string& message) {
    Diagnostic diagnostic;
    diagnostic.line = line;
    diagnostic.column = column;
    diagnostic.code = 'a';
    diagnostic.message = message;
    diagnostics_.push_back(diagnostic);
}

void ErrorReporter::clear() {
    diagnostics_.clear();
}

std::vector<Diagnostic> ErrorReporter::sorted_unique_by_line() const {
    std::vector<Diagnostic> sorted = diagnostics_;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Diagnostic& a, const Diagnostic& b) {
                         return a.line < b.line;
                     });

    std::vector<Diagnostic> unique;
    int last_line = -1;
    for (const Diagnostic& diagnostic : sorted) {
        if (diagnostic.line == last_line) {
            continue;
        }
        unique.push_back(diagnostic);
        last_line = diagnostic.line;
    }
    return unique;
}

}  // namespace common
