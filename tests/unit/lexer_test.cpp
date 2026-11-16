#include <iostream>
#include <string>
#include <vector>

#include "lexer/lexer.h"

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

lexer::Token first_token(const std::string& source) {
    lexer::Lexer lexer(source);
    return lexer.next_token();
}

std::vector<lexer::Token> all_tokens(const std::string& source) {
    lexer::Lexer lexer(source);
    std::vector<lexer::Token> tokens;
    for (;;) {
        const lexer::Token token = lexer.next_token();
        tokens.push_back(token);
        if (token.kind == lexer::TokenKind::End || token.kind == lexer::TokenKind::Error) {
            break;
        }
    }
    return tokens;
}

}  // namespace

int main() {
    {
        const lexer::Token t = first_token("abc");
        check(t.kind == lexer::TokenKind::Identifier && t.lexeme == "abc",
              "abc 识别为标识符");
        check(t.line == 1 && t.column == 1, "标识符位置为 1:1");
    }
    {
        check(first_token("a1_b2").kind == lexer::TokenKind::Identifier,
              "标识符可包含数字与下划线");
        check(first_token("_x").kind == lexer::TokenKind::Identifier,
              "下划线可作为首字符");
    }
    {
        const lexer::Token t = first_token("coNst");
        check(t.kind == lexer::TokenKind::KeywordConst, "coNst 识别为 const 保留字");
        check(t.lexeme == "coNst", "保留字原文原样保留");
    }
    {
        check(first_token("CONST").kind == lexer::TokenKind::KeywordConst,
              "大写 CONST 识别为保留字");
        check(first_token("iF").kind == lexer::TokenKind::KeywordIf,
              "混合大小写 iF 识别为 if");
    }
    {
        check(first_token("constx").kind == lexer::TokenKind::Identifier,
              "constx 不是保留字");
        check(first_token("if1").kind == lexer::TokenKind::Identifier,
              "if1 不是保留字");
        check(first_token("else_").kind == lexer::TokenKind::Identifier,
              "else_ 不是保留字");
    }
    {
        struct KeywordCase {
            const char* text;
            lexer::TokenKind kind;
        };
        const KeywordCase keywords[] = {
            {"const", lexer::TokenKind::KeywordConst},
            {"int", lexer::TokenKind::KeywordInt},
            {"char", lexer::TokenKind::KeywordChar},
            {"void", lexer::TokenKind::KeywordVoid},
            {"main", lexer::TokenKind::KeywordMain},
            {"if", lexer::TokenKind::KeywordIf},
            {"else", lexer::TokenKind::KeywordElse},
            {"switch", lexer::TokenKind::KeywordSwitch},
            {"case", lexer::TokenKind::KeywordCase},
            {"default", lexer::TokenKind::KeywordDefault},
            {"while", lexer::TokenKind::KeywordWhile},
            {"for", lexer::TokenKind::KeywordFor},
            {"scanf", lexer::TokenKind::KeywordScanf},
            {"printf", lexer::TokenKind::KeywordPrintf},
            {"return", lexer::TokenKind::KeywordReturn},
        };

        bool all_lower = true;
        bool all_upper = true;
        for (const KeywordCase& kc : keywords) {
            if (first_token(kc.text).kind != kc.kind) {
                all_lower = false;
            }
            std::string upper(kc.text);
            for (char& c : upper) {
                if (c >= 'a' && c <= 'z') {
                    c = static_cast<char>(c - 'a' + 'A');
                }
            }
            if (first_token(upper).kind != kc.kind) {
                all_upper = false;
            }
        }
        check(all_lower, "15 个保留字小写全部识别");
        check(all_upper, "15 个保留字大写全部识别");
    }
    {
        const std::vector<lexer::Token> tokens = all_tokens("  int x\n  if");
        check(tokens.size() == 4, "位置测试得到 3 个单词和一个 End");
        check(tokens.size() == 4 && tokens[0].kind == lexer::TokenKind::KeywordInt &&
                  tokens[0].line == 1 && tokens[0].column == 3,
              "int 位于 1:3");
        check(tokens.size() == 4 && tokens[1].kind == lexer::TokenKind::Identifier &&
                  tokens[1].line == 1 && tokens[1].column == 7,
              "x 位于 1:7");
        check(tokens.size() == 4 && tokens[2].kind == lexer::TokenKind::KeywordIf &&
                  tokens[2].line == 2 && tokens[2].column == 3,
              "if 位于 2:3");
        check(tokens.size() == 4 && tokens[3].kind == lexer::TokenKind::End,
              "末尾返回 End");
    }
    {
        const lexer::Token t = first_token("@");
        check(t.kind == lexer::TokenKind::Error, "非法字符返回 Error");
        check(t.line == 1 && t.column == 1 && t.lexeme == "@",
              "Error 记录位置与原字符");
    }
    {
        const lexer::Token t = first_token("0");
        check(t.kind == lexer::TokenKind::IntConstant && t.lexeme == "0",
              "0 识别为整数常量");
        check(t.line == 1 && t.column == 1, "整数位置为 1:1");
    }
    {
        const lexer::Token t = first_token("001");
        check(t.kind == lexer::TokenKind::IntConstant && t.lexeme == "001",
              "前导零原样保留（001）");
        check(first_token("12345").lexeme == "12345", "多位整数完整保留");
    }
    {
        const std::vector<lexer::Token> tokens = all_tokens("123abc");
        check(tokens.size() == 3, "123abc 拆成 2 个单词和一个 End");
        check(tokens.size() == 3 && tokens[0].kind == lexer::TokenKind::IntConstant &&
                  tokens[0].lexeme == "123",
              "数字在字母前停止（123）");
        check(tokens.size() == 3 && tokens[1].kind == lexer::TokenKind::Identifier &&
                  tokens[1].lexeme == "abc",
              "数字后的字母成为标识符");
    }
    {
        const std::vector<lexer::Token> tokens = all_tokens("  42\n07");
        check(tokens.size() == 3 && tokens[0].kind == lexer::TokenKind::IntConstant &&
                  tokens[0].line == 1 && tokens[0].column == 3,
              "42 位于 1:3");
        check(tokens.size() == 3 && tokens[1].kind == lexer::TokenKind::IntConstant &&
                  tokens[1].lexeme == "07" && tokens[1].line == 2 && tokens[1].column == 1,
              "07 位于 2:1 且保留前导零");
    }
    {
        check(std::string(lexer::token_code(lexer::TokenKind::IntConstant)) == "INTCON",
              "整数类别码为 INTCON");
    }
    {
        const char* allowed[] = {"'_'", "'+'", "'-'", "'*'", "'/'", "'a'", "'Z'", "'0'"};
        const char* contents[] = {"_", "+", "-", "*", "/", "a", "Z", "0"};
        bool all_ok = true;
        for (std::size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); ++i) {
            const lexer::Token t = first_token(allowed[i]);
            if (t.kind != lexer::TokenKind::CharConstant || t.lexeme != contents[i]) {
                all_ok = false;
            }
        }
        check(all_ok, "允许的字符常量全部识别");
    }
    {
        const lexer::Token t = first_token("'_'");
        check(t.kind == lexer::TokenKind::CharConstant && t.lexeme == "_",
              "字符常量输出不含引号");
        check(t.line == 1 && t.column == 1, "字符常量位置为 1:1");
    }
    {
        const lexer::Token t = first_token("  'x'");
        check(t.kind == lexer::TokenKind::CharConstant && t.lexeme == "x" &&
                  t.line == 1 && t.column == 3,
              "字符常量位于 1:3");
    }
    {
        check(first_token("''").kind == lexer::TokenKind::Error, "空字符常量报错");
        check(first_token("'ab'").kind == lexer::TokenKind::Error, "多字符常量报错");
        check(first_token("'@'").kind == lexer::TokenKind::Error, "非法内容报错");
        check(first_token("'a").kind == lexer::TokenKind::Error, "缺少结束引号报错");
    }
    {
        const std::vector<lexer::Token> tokens = all_tokens("'a' b");
        check(tokens.size() == 3 && tokens[0].kind == lexer::TokenKind::CharConstant &&
                  tokens[0].lexeme == "a",
              "字符常量后接标识符：第一个词法单元正确");
        check(tokens.size() == 3 && tokens[1].kind == lexer::TokenKind::Identifier &&
                  tokens[1].lexeme == "b",
              "字符常量后接标识符：第二个词法单元正确");
    }
    {
        check(std::string(lexer::token_code(lexer::TokenKind::CharConstant)) == "CHARCON",
              "字符常量类别码为 CHARCON");
    }
    {
        const lexer::Token t = first_token("\"Hello World\"");
        check(t.kind == lexer::TokenKind::StringConstant && t.lexeme == "Hello World",
              "字符串内容原样保存（含空格）");
        check(t.line == 1 && t.column == 1, "字符串位置为 1:1");
    }
    {
        check(first_token("\"\"").kind == lexer::TokenKind::Error, "空字符串报错");
        check(first_token("\"abc").kind == lexer::TokenKind::Error, "缺少结束引号报错");
        check(first_token("\"a\nb\"").kind == lexer::TokenKind::Error,
              "字符串中的换行报错");
        check(first_token("\"a\tb\"").kind == lexer::TokenKind::Error,
              "字符串中的制表符报错");
        check(first_token("\"\xC3\xA9\"").kind == lexer::TokenKind::Error,
              "非 ASCII 字符报错");
    }
    {
        const lexer::Token t = first_token("\" \"");
        check(t.kind == lexer::TokenKind::StringConstant && t.lexeme == " ",
              "单个空格是合法字符串");
        check(first_token("\"#\"").lexeme == "#", "ASCII 35 边界字符合法");
        check(first_token("\"~\"").lexeme == "~", "ASCII 126 边界字符合法");
    }
    {
        // 输入为 "a\"b"：反斜杠不是转义，字符串在第二个双引号处结束
        const lexer::Token t = first_token("\"a\\\"b\"");
        check(t.kind == lexer::TokenKind::StringConstant && t.lexeme == "a\\",
              "反斜杠不做转义处理");
    }
    {
        const lexer::Token t = first_token("  \"x\"");
        check(t.kind == lexer::TokenKind::StringConstant && t.lexeme == "x" &&
                  t.line == 1 && t.column == 3,
              "字符串位于 1:3");
    }
    {
        const std::vector<lexer::Token> tokens = all_tokens("\"hi\" x");
        check(tokens.size() == 3 && tokens[0].kind == lexer::TokenKind::StringConstant &&
                  tokens[0].lexeme == "hi",
              "字符串后接标识符：第一个词法单元正确");
        check(tokens.size() == 3 && tokens[1].kind == lexer::TokenKind::Identifier &&
                  tokens[1].lexeme == "x",
              "字符串后接标识符：第二个词法单元正确");
    }
    {
        check(std::string(lexer::token_code(lexer::TokenKind::StringConstant)) == "STRCON",
              "字符串类别码为 STRCON");
    }
    {
        const std::vector<lexer::Token> tokens = all_tokens("<= >= == !=");
        check(tokens.size() == 5, "四个双字符运算符 + End");
        check(tokens.size() == 5 && tokens[0].kind == lexer::TokenKind::LessEqual &&
                  tokens[0].lexeme == "<=",
              "<= 识别为 LEQ");
        check(tokens.size() == 5 && tokens[1].kind == lexer::TokenKind::GreaterEqual &&
                  tokens[1].lexeme == ">=",
              ">= 识别为 GEQ");
        check(tokens.size() == 5 && tokens[2].kind == lexer::TokenKind::Equal &&
                  tokens[2].lexeme == "==",
              "== 识别为 EQL");
        check(tokens.size() == 5 && tokens[3].kind == lexer::TokenKind::NotEqual &&
                  tokens[3].lexeme == "!=",
              "!= 识别为 NEQ");
    }
    {
        const std::vector<lexer::Token> tokens = all_tokens("< > = !");
        check(tokens.size() == 4 && tokens[0].kind == lexer::TokenKind::Less &&
                  tokens[0].lexeme == "<",
              "单字符 < 识别为 LSS");
        check(tokens.size() == 4 && tokens[1].kind == lexer::TokenKind::Greater &&
                  tokens[1].lexeme == ">",
              "单字符 > 识别为 GRE");
        check(tokens.size() == 4 && tokens[2].kind == lexer::TokenKind::Assign &&
                  tokens[2].lexeme == "=",
              "单字符 = 识别为 ASSIGN");
        check(tokens.size() == 4 && tokens[3].kind == lexer::TokenKind::Error,
              "单独的 ! 报错");
    }
    {
        const std::vector<lexer::Token> tokens = all_tokens("+ - * / ; , ( ) [ ] { } :");
        const lexer::TokenKind kinds[] = {
            lexer::TokenKind::Plus,      lexer::TokenKind::Minus,
            lexer::TokenKind::Star,      lexer::TokenKind::Slash,
            lexer::TokenKind::Semicolon, lexer::TokenKind::Comma,
            lexer::TokenKind::LParen,    lexer::TokenKind::RParen,
            lexer::TokenKind::LBracket,  lexer::TokenKind::RBracket,
            lexer::TokenKind::LBrace,    lexer::TokenKind::RBrace,
            lexer::TokenKind::Colon,
        };
        bool all_ok = tokens.size() == 14;  // 13 个符号 + End
        for (std::size_t i = 0; all_ok && i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
            if (tokens[i].kind != kinds[i]) {
                all_ok = false;
            }
        }
        check(all_ok, "全部单字符运算符与界符识别");
    }
    {
        const std::vector<lexer::Token> tokens = all_tokens("a<=b");
        check(tokens.size() == 4, "a<=b 得到 3 个词法单元 + End");
        check(tokens.size() == 4 && tokens[1].kind == lexer::TokenKind::LessEqual &&
                  tokens[1].line == 1 && tokens[1].column == 2,
              "<= 位于 1:2");
    }
    {
        const std::vector<lexer::Token> tokens = all_tokens("===");
        check(tokens.size() == 3 && tokens[0].kind == lexer::TokenKind::Equal &&
                  tokens[1].kind == lexer::TokenKind::Assign,
              "=== 拆为 == 与 =");
    }
    {
        check(std::string(lexer::token_code(lexer::TokenKind::LessEqual)) == "LEQ",
              "<= 类别码为 LEQ");
        check(std::string(lexer::token_code(lexer::TokenKind::Plus)) == "PLUS",
              "+ 类别码为 PLUS");
        check(std::string(lexer::token_code(lexer::TokenKind::Semicolon)) == "SEMICN",
              "; 类别码为 SEMICN");
        check(std::string(lexer::token_code(lexer::TokenKind::Colon)) == "COLON",
              ": 类别码为 COLON");
    }
    {
        check(std::string(lexer::token_code(lexer::TokenKind::Identifier)) == "IDENFR",
              "标识符类别码为 IDENFR");
        check(std::string(lexer::token_code(lexer::TokenKind::KeywordConst)) == "CONSTTK",
              "const 类别码为 CONSTTK");
        check(std::string(lexer::token_code(lexer::TokenKind::KeywordReturn)) == "RETURNTK",
              "return 类别码为 RETURNTK");
    }
    {
        check(first_token("").kind == lexer::TokenKind::End, "空输入立即返回 End");
    }

    if (failures == 0) {
        std::cout << "全部通过\n";
        return 0;
    }
    std::cout << failures << " 项失败\n";
    return 1;
}
