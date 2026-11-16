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
    Ldi,      // 间接取数：弹出地址，压入 mem[地址]
    Sti,      // 间接存数：弹出值、地址，写入 mem[地址]
    Jmp,      // 无条件跳转到指令下标
    Jpc,      // 弹出条件，为 0 时跳转
    Cal,      // 调用函数：返回地址入调用栈，跳转到入口
    Ret,      // 函数返回：弹出调用栈中的返回地址
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
    Pop,
};

struct Instruction {
    OpCode op = OpCode::End;
    long long operand = 0;  // Lit/Lod/Sto 的数值/地址；Opr 的 OprCode
    std::string text;       // WrtStr 的字符串
};

const char* opcode_name(OpCode op);
const char* opr_name(OprCode op);

}  // namespace pcode
