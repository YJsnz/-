#include "common/error_reporter.h"

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

}  // namespace common
