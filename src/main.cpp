// 编译器主程序骨架（P1-1）
// - 读取当前目录下的 testfile.txt
// - 仅在 --dump-lexer 打开时创建 output.txt（默认关闭）
// - 词法分析将在 P1-2 / P1-3 接入
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "common/error_reporter.h"
#include "lexer/lexer.h"

namespace {

struct Options {
    bool dump_lexer = false;
    bool show_help = false;
};

const char* const kInputFile = "testfile.txt";
const char* const kOutputFile = "output.txt";

void print_usage(std::ostream& os) {
    os << "用法：compiler [选项]\n"
       << "  --dump-lexer   输出词法分析结果到 output.txt（默认关闭）\n"
       << "  -h, --help     显示本帮助\n";
}

bool parse_args(int argc, char** argv, Options& options, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--dump-lexer") {
            options.dump_lexer = true;
        } else if (arg == "-h" || arg == "--help") {
            options.show_help = true;
        } else {
            error = "未知参数：" + arg;
            return false;
        }
    }
    return true;
}

bool read_source(const std::string& path, std::string& source, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "无法打开输入文件 " + path;
        return false;
    }
    source.assign(std::istreambuf_iterator<char>(in),
                  std::istreambuf_iterator<char>());
    if (in.bad()) {
        error = "读取输入文件失败：" + path;
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    std::string error;

    if (!parse_args(argc, argv, options, error)) {
        std::cerr << "错误：" << error << "\n";
        print_usage(std::cerr);
        return 2;
    }

    if (options.show_help) {
        print_usage(std::cout);
        return 0;
    }

    std::string source;
    if (!read_source(kInputFile, source, error)) {
        std::cerr << "错误：" << error << "\n";
        return 1;
    }

    // 最终形态不输出词法调试信息；只有显式打开开关时才创建结果文件。
    if (!options.dump_lexer) {
        return 0;
    }

    std::ofstream out(kOutputFile, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "错误：无法创建输出文件 " << kOutputFile << "\n";
        return 1;
    }

    common::ErrorReporter errors;
    lexer::Lexer lexer(source, [&errors](const common::Diagnostic& diagnostic) {
        errors.report(diagnostic);
    });

    for (;;) {
        const lexer::Token token = lexer.next_token();
        if (token.kind == lexer::TokenKind::End) {
            break;
        }
        if (token.kind == lexer::TokenKind::Error) {
            std::cerr << "词法错误（" << token.line << ":" << token.column
                      << "）：" << token.message << "\n";
            return 1;
        }
        out << lexer::token_code(token.kind) << ' ' << token.lexeme << '\n';
    }
    return 0;
}
