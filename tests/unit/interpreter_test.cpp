#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "pcode/interpreter.h"

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

pcode::Instruction lit(long long value) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Lit;
    instruction.operand = value;
    return instruction;
}

pcode::Instruction lod(long long address) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Lod;
    instruction.operand = address;
    return instruction;
}

pcode::Instruction sto(long long address) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Sto;
    instruction.operand = address;
    return instruction;
}

pcode::Instruction opr(pcode::OprCode code) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Opr;
    instruction.operand = static_cast<long long>(code);
    return instruction;
}

pcode::Instruction wrt_str(const std::string& text) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::WrtStr;
    instruction.text = text;
    return instruction;
}

pcode::Instruction ldi() {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Ldi;
    return instruction;
}

pcode::Instruction sti() {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Sti;
    return instruction;
}

pcode::Instruction end() {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::End;
    return instruction;
}

std::string run(const std::vector<pcode::Instruction>& code,
                const std::string& input, bool& ok, std::string& error) {
    std::istringstream in(input);
    std::ostringstream out;
    pcode::Interpreter interpreter(code, in, out);
    ok = interpreter.run();
    error = interpreter.error();
    return out.str();
}

}  // namespace

int main() {
    bool ok = false;
    std::string error;

    {
        const std::string output = run({lit(2), lit(3), opr(pcode::OprCode::Add),
                                        lit(4), opr(pcode::OprCode::Mul),
                                        opr(pcode::OprCode::WrtInt),
                                        opr(pcode::OprCode::WrtNl), end()},
                                       "", ok, error);
        check(ok && output == "20\n", "加减乘除执行正确");
    }
    {
        const std::string output =
            run({lit(5), opr(pcode::OprCode::Neg),
                 opr(pcode::OprCode::WrtInt), opr(pcode::OprCode::WrtNl),
                 end()},
                "", ok, error);
        check(ok && output == "-5\n", "一元取负执行正确");
    }
    {
        const std::string output =
            run({lit(8), lit(2), opr(pcode::OprCode::Div), lit(3),
                 opr(pcode::OprCode::Sub), opr(pcode::OprCode::WrtInt),
                 opr(pcode::OprCode::WrtNl), end()},
                "", ok, error);
        check(ok && output == "1\n", "除法与减法执行正确");
    }
    {
        const std::string output =
            run({wrt_str("Hello World"), opr(pcode::OprCode::WrtNl), end()},
                "", ok, error);
        check(ok && output == "Hello World\n", "字符串输出正确");
    }
    {
        const std::string output =
            run({opr(pcode::OprCode::ReadInt), sto(0), lod(0),
                 opr(pcode::OprCode::WrtInt), opr(pcode::OprCode::WrtNl),
                 end()},
                "42\n", ok, error);
        check(ok && output == "42\n", "读入整数并输出正确");
    }
    {
        const std::string output =
            run({opr(pcode::OprCode::ReadChar), sto(0), lod(0),
                 opr(pcode::OprCode::WrtChar), opr(pcode::OprCode::WrtNl),
                 end()},
                "a\n", ok, error);
        check(ok && output == "a\n", "读入字符并输出正确");
    }
    {
        const std::string output =
            run({lit(2), lit(42), sti(), lit(2), ldi(),
                 opr(pcode::OprCode::WrtInt), opr(pcode::OprCode::WrtNl),
                 end()},
                "", ok, error);
        check(ok && output == "42\n", "间接存数/取数正确");
    }
    {
        const std::string output =
            run({lit(2), lit(3), opr(pcode::OprCode::Lss),
                 opr(pcode::OprCode::WrtInt), opr(pcode::OprCode::WrtNl),
                 end()},
                "", ok, error);
        check(ok && output == "1\n", "关系运算执行正确");
    }
    {
        const std::string output =
            run({lit(1), opr(pcode::OprCode::WrtInt), end(), lit(2),
                 opr(pcode::OprCode::WrtInt)},
                "", ok, error);
        check(ok && output == "1", "END 停止执行");
    }
    {
        const std::string output =
            run({lit(65), opr(pcode::OprCode::WrtChar),
                 opr(pcode::OprCode::WrtNl), end()},
                "", ok, error);
        check(ok && output == "A\n", "字符输出按 ASCII 打印");
    }
    {
        const std::string output =
            run({opr(pcode::OprCode::ReadInt), opr(pcode::OprCode::ReadInt),
                 opr(pcode::OprCode::Add), opr(pcode::OprCode::WrtInt),
                 opr(pcode::OprCode::WrtNl), end()},
                "1\n2\n", ok, error);
        check(ok && output == "3\n", "连续读入整数按空白分隔");
    }
    {
        run({lit(1), lit(0), opr(pcode::OprCode::Div), end()}, "", ok, error);
        check(!ok && !error.empty(), "除数为零时执行失败");
    }

    if (failures == 0) {
        std::cout << "全部通过\n";
        return 0;
    }
    std::cout << failures << " 项失败\n";
    return 1;
}
