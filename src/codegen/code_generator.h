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
    void emit_end();

    const std::vector<pcode::Instruction>& instructions() const { return code_; }
    void clear();

private:
    std::vector<pcode::Instruction> code_;
};

}  // namespace codegen
