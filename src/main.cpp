// 编译器主程序（P1-1 骨架，P2-1 接入语法分析）
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "codegen/code_generator.h"
#include "common/error_reporter.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "pcode/interpreter.h"

namespace {

struct Options {
    bool dump_lexer = false;
    bool dump_parser = false;
    bool check = false;
    bool gen = false;
    bool show_help = false;
};

const char* const kInputFile = "testfile.txt";
const char* const kOutputFile = "output.txt";
const char* const kErrorFile = "error.txt";
const char* const kPcodeResultFile = "pcoderesult.txt";

void print_usage(std::ostream& os) {
    os << "用法：compiler [选项]\n"
       << "  --dump-lexer    输出词法分析结果到 output.txt\n"
       << "  --dump-parser   输出单词与语法成分到 output.txt\n"
       << "  --check         检查语法/语义错误，输出 error.txt\n"
       << "  --gen           生成 PCODE 并解释执行，输出 pcoderesult.txt\n"
       << "  -h, --help      显示本帮助\n";
}

bool parse_args(int argc, char** argv, Options& options, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--dump-lexer") {
            options.dump_lexer = true;
        } else if (arg == "--dump-parser") {
            options.dump_parser = true;
        } else if (arg == "--check") {
            options.check = true;
        } else if (arg == "--gen") {
            options.gen = true;
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

    // 代码生成模式：生成 PCODE，解释执行，结果写 pcoderesult.txt。
    if (options.gen) {
        common::ErrorReporter errors;
        lexer::Lexer lexer(source, [&errors](const common::Diagnostic& diagnostic) {
            errors.report(diagnostic);
        });
        codegen::CodeGenerator generator;
        parser::Parser parser(
            lexer, nullptr,
            [&errors](const common::Diagnostic& diagnostic) {
                errors.report(diagnostic);
            },
            false, &generator);
        if (!parser.parse_program() || errors.has_errors()) {
            return 1;
        }

        std::ofstream result(kPcodeResultFile,
                             std::ios::binary | std::ios::trunc);
        if (!result) {
            std::cerr << "错误：无法创建结果文件 " << kPcodeResultFile << "\n";
            return 1;
        }
        pcode::Interpreter interpreter(generator.instructions(), std::cin,
                                       result);
        if (!interpreter.run()) {
            std::cerr << "PCODE 执行错误：" << interpreter.error() << "\n";
            return 1;
        }
        return 0;
    }

    // 错误检查模式：只写 error.txt，不输出词法/语法调试信息。
    if (options.check) {
        common::ErrorReporter errors;
        lexer::Lexer lexer(source, [&errors](const common::Diagnostic& diagnostic) {
            errors.report(diagnostic);
        });
        parser::Parser parser(
            lexer, nullptr,
            [&errors](const common::Diagnostic& diagnostic) {
                errors.report(diagnostic);
            },
            true);
        parser.parse_program();

        std::ofstream err_out(kErrorFile, std::ios::binary | std::ios::trunc);
        if (!err_out) {
            std::cerr << "错误：无法创建错误输出文件 " << kErrorFile << "\n";
            return 1;
        }
        for (const common::Diagnostic& diagnostic : errors.sorted_unique_by_line()) {
            err_out << diagnostic.line << ' ' << diagnostic.code << '\n';
        }
        return errors.has_errors() ? 1 : 0;
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
