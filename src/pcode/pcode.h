// PCODE 指令定义（P4-1）
#pragma once

#include <string>

namespace pcode {

enum class OpCode {
    Lit,      // 压入整数
    Lod,      // 取变量
    Sto,      // 存变量
    Opr,      // 运算/输入输出
    WrtStr,   // 输出字符串
    End,      // 结束
};

enum class OprCode {
    Add = 1,
    Sub,
    Mul,
    Div,
    Neg,
    Eql,
    Neq,
    Lss,
    Geq,
    Gre,
    Leq,
    WrtInt,
    WrtChar,
    ReadInt,
    ReadChar,
    WrtNl,
};

struct Instruction {
    OpCode op = OpCode::End;
    long long operand = 0;  // Lit/Lod/Sto 的数值/地址；Opr 的 OprCode
    std::string text;       // WrtStr 的字符串
};

const char* opcode_name(OpCode op);
const char* opr_name(OprCode op);

}  // namespace pcode
