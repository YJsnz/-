#include "codegen/code_generator.h"

namespace codegen {

void CodeGenerator::emit(pcode::Instruction instruction) {
    code_.push_back(std::move(instruction));
}

void CodeGenerator::emit_lit(long long value) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Lit;
    instruction.operand = value;
    code_.push_back(instruction);
}

void CodeGenerator::emit_lod(int address) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Lod;
    instruction.operand = address;
    code_.push_back(instruction);
}

void CodeGenerator::emit_sto(int address) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Sto;
    instruction.operand = address;
    code_.push_back(instruction);
}

void CodeGenerator::emit_opr(pcode::OprCode code) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Opr;
    instruction.operand = static_cast<long long>(code);
    code_.push_back(instruction);
}

void CodeGenerator::emit_wrt_str(const std::string& text) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::WrtStr;
    instruction.text = text;
    code_.push_back(instruction);
}

void CodeGenerator::emit_ldi() {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Ldi;
    code_.push_back(instruction);
}

void CodeGenerator::emit_sti() {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Sti;
    code_.push_back(instruction);
}

void CodeGenerator::emit_end() {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::End;
    code_.push_back(instruction);
}

void CodeGenerator::clear() {
    code_.clear();
}

}  // namespace codegen
