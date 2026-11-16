// PCODE 代码生成器：收集指令序列
#pragma once

#include <string>
#include <vector>

#include "pcode/pcode.h"

namespace codegen {

class CodeGenerator {
public:
    void emit(pcode::Instruction instruction);
    void emit_lit(long long value);
    void emit_lod(int address);
    void emit_sto(int address);
    void emit_opr(pcode::OprCode code);
    void emit_wrt_str(const std::string& text);
    void emit_ldi();
    void emit_sti();
    void emit_jmp(int target);
    void emit_jpc(int target);
    void emit_cal(int target);
    void emit_ret();
    void emit_end();

    int position() const { return static_cast<int>(code_.size()); }
    void patch(int index, long long operand);
    void append(const std::vector<pcode::Instruction>& instructions);
    std::vector<pcode::Instruction> take_from(int index);

    const std::vector<pcode::Instruction>& instructions() const { return code_; }
    void clear();

private:
    std::vector<pcode::Instruction> code_;
};

}  // namespace codegen
