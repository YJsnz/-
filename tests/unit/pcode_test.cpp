#include <iostream>
#include <string>

#include "pcode/pcode.h"

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
    using pcode::OpCode;
    using pcode::OprCode;

    check(std::string(pcode::opcode_name(OpCode::Lit)) == "LIT", "LIT 名称");
    check(std::string(pcode::opcode_name(OpCode::Lod)) == "LOD", "LOD 名称");
    check(std::string(pcode::opcode_name(OpCode::Sto)) == "STO", "STO 名称");
    check(std::string(pcode::opcode_name(OpCode::Opr)) == "OPR", "OPR 名称");
    check(std::string(pcode::opcode_name(OpCode::WrtStr)) == "WRT_STR",
          "WRT_STR 名称");
    check(std::string(pcode::opcode_name(OpCode::Ldi)) == "LDI", "LDI 名称");
    check(std::string(pcode::opcode_name(OpCode::Sti)) == "STI", "STI 名称");
    check(std::string(pcode::opcode_name(OpCode::Jmp)) == "JMP", "JMP 名称");
    check(std::string(pcode::opcode_name(OpCode::Jpc)) == "JPC", "JPC 名称");
    check(std::string(pcode::opcode_name(OpCode::Cal)) == "CAL", "CAL 名称");
    check(std::string(pcode::opcode_name(OpCode::Ret)) == "RET", "RET 名称");
    check(std::string(pcode::opcode_name(OpCode::End)) == "END", "END 名称");

    check(std::string(pcode::opr_name(OprCode::Add)) == "ADD", "ADD 名称");
    check(std::string(pcode::opr_name(OprCode::Sub)) == "SUB", "SUB 名称");
    check(std::string(pcode::opr_name(OprCode::Mul)) == "MUL", "MUL 名称");
    check(std::string(pcode::opr_name(OprCode::Div)) == "DIV", "DIV 名称");
    check(std::string(pcode::opr_name(OprCode::Neg)) == "NEG", "NEG 名称");
    check(std::string(pcode::opr_name(OprCode::Eql)) == "EQL", "EQL 名称");
    check(std::string(pcode::opr_name(OprCode::Neq)) == "NEQ", "NEQ 名称");
    check(std::string(pcode::opr_name(OprCode::Lss)) == "LSS", "LSS 名称");
    check(std::string(pcode::opr_name(OprCode::Geq)) == "GEQ", "GEQ 名称");
    check(std::string(pcode::opr_name(OprCode::Gre)) == "GRE", "GRE 名称");
    check(std::string(pcode::opr_name(OprCode::Leq)) == "LEQ", "LEQ 名称");
    check(std::string(pcode::opr_name(OprCode::WrtInt)) == "WRT_INT",
          "WRT_INT 名称");
    check(std::string(pcode::opr_name(OprCode::WrtChar)) == "WRT_CHAR",
          "WRT_CHAR 名称");
    check(std::string(pcode::opr_name(OprCode::ReadInt)) == "READ_INT",
          "READ_INT 名称");
    check(std::string(pcode::opr_name(OprCode::ReadChar)) == "READ_CHAR",
          "READ_CHAR 名称");
    check(std::string(pcode::opr_name(OprCode::WrtNl)) == "WRT_NL",
          "WRT_NL 名称");
    check(std::string(pcode::opr_name(OprCode::Pop)) == "POP", "POP 名称");

    check(static_cast<int>(OprCode::Add) == 1, "ADD 编码为 1");
    check(static_cast<int>(OprCode::WrtNl) == 16, "WRT_NL 编码为 16");
    check(static_cast<int>(OprCode::Pop) == 17, "POP 编码为 17");

    pcode::Instruction instruction;
    check(instruction.op == OpCode::End && instruction.operand == 0 &&
              instruction.text.empty(),
          "默认指令为 END");

    if (failures == 0) {
        std::cout << "全部通过\n";
        return 0;
    }
    std::cout << failures << " 项失败\n";
    return 1;
}
