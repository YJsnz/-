#include <iostream>
#include <string>

#include "codegen/code_generator.h"

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
    codegen::CodeGenerator generator;
    check(generator.instructions().empty(), "初始指令为空");

    generator.emit_lit(5);
    check(generator.instructions().size() == 1 &&
              generator.instructions()[0].op == pcode::OpCode::Lit &&
              generator.instructions()[0].operand == 5,
          "LIT 指令生成正确");

    generator.emit_lod(2);
    check(generator.instructions().size() == 2 &&
              generator.instructions()[1].op == pcode::OpCode::Lod &&
              generator.instructions()[1].operand == 2,
          "LOD 指令生成正确");

    generator.emit_sto(3);
    check(generator.instructions().size() == 3 &&
              generator.instructions()[2].op == pcode::OpCode::Sto &&
              generator.instructions()[2].operand == 3,
          "STO 指令生成正确");

    generator.emit_opr(pcode::OprCode::Add);
    check(generator.instructions().size() == 4 &&
              generator.instructions()[3].op == pcode::OpCode::Opr &&
              generator.instructions()[3].operand == 1,
          "OPR 指令生成正确");

    generator.emit_wrt_str("Hello");
    check(generator.instructions().size() == 5 &&
              generator.instructions()[4].op == pcode::OpCode::WrtStr &&
              generator.instructions()[4].text == "Hello",
          "WRT_STR 指令生成正确");

    generator.emit_ldi();
    check(generator.instructions().size() == 6 &&
              generator.instructions()[5].op == pcode::OpCode::Ldi,
          "LDI 指令生成正确");

    generator.emit_sti();
    check(generator.instructions().size() == 7 &&
              generator.instructions()[6].op == pcode::OpCode::Sti,
          "STI 指令生成正确");

    generator.emit_end();
    check(generator.instructions().size() == 8 &&
              generator.instructions()[7].op == pcode::OpCode::End,
          "END 指令生成正确");

    generator.clear();
    check(generator.instructions().empty(), "清空后指令为空");

    pcode::Instruction custom;
    custom.op = pcode::OpCode::Lit;
    custom.operand = 9;
    generator.emit(custom);
    check(generator.instructions().size() == 1 &&
              generator.instructions()[0].operand == 9,
          "通用 emit 正确");

    if (failures == 0) {
        std::cout << "全部通过\n";
        return 0;
    }
    std::cout << failures << " 项失败\n";
    return 1;
}
