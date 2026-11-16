// 编译器主程序（P1-1 骨架，P2-1 接入语法分析）
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "common/error_reporter.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

namespace {

struct Options {
    bool dump_lexer = false;
    bool dump_parser = false;
    bool show_help = false;
};

const char* const kInputFile = "testfile.txt";
const char* const kOutputFile = "output.txt";

void print_usage(std::ostream& os) {
    os << "用法：compiler [选项]\n"
       << "  --dump-lexer    输出词法分析结果到 output.txt\n"
       << "  --dump-parser   输出单词与语法成分到 output.txt\n"
       << "  -h, --help      显示本帮助\n";
}

bool parse_args(int argc, char** argv, Options& options, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--dump-lexer") {
            options.dump_lexer = true;
        } else if (arg == "--dump-parser") {
            options.dump_parser = true;
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

    // 最终形态不输出调试信息；只有显式打开开关时才创建结果文件。
    if (!options.dump_lexer && !options.dump_parser) {
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
        std::cerr << "词法错误（" << diagnostic.line << ":" << diagnostic.column
                  << "）：" << diagnostic.message << "\n";
    });

    // --dump-parser 优先：输出单词 + 语法成分。
    if (options.dump_parser) {
        parser::Parser parser(lexer, &out,
                              [&errors](const common::Diagnostic& diagnostic) {
                                  errors.report(diagnostic);
                                  std::cerr << "语法错误（" << diagnostic.line << ":"
                                            << diagnostic.column << "）："
                                            << diagnostic.message << "\n";
                              });
        return parser.parse_program() ? 0 : 1;
    }

    for (;;) {
        const lexer::Token token = lexer.next_token();
        if (token.kind == lexer::TokenKind::End) {
            break;
        }
        if (token.kind == lexer::TokenKind::Error) {
            return 1;  // 已由 Lexer 回调打印
        }
        out << lexer::token_code(token.kind) << ' ' << token.lexeme << '\n';
    }
    return 0;
}
