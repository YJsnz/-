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

void CodeGenerator::emit_jmp(int target) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Jmp;
    instruction.operand = target;
    code_.push_back(instruction);
}

void CodeGenerator::emit_jpc(int target) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Jpc;
    instruction.operand = target;
    code_.push_back(instruction);
}

void CodeGenerator::emit_cal(int target) {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Cal;
    instruction.operand = target;
    code_.push_back(instruction);
}

void CodeGenerator::emit_ret() {
    pcode::Instruction instruction;
    instruction.op = pcode::OpCode::Ret;
    code_.push_back(instruction);
}

void CodeGenerator::patch(int index, long long operand) {
    if (index >= 0 && index < static_cast<int>(code_.size())) {
        code_[static_cast<std::size_t>(index)].operand = operand;
    }
}

void CodeGenerator::append(
    const std::vector<pcode::Instruction>& instructions) {
    code_.insert(code_.end(), instructions.begin(), instructions.end());
}

std::vector<pcode::Instruction> CodeGenerator::take_from(int index) {
    std::vector<pcode::Instruction> taken;
    if (index < 0 || index > static_cast<int>(code_.size())) {
        return taken;
    }
    taken.assign(code_.begin() + index, code_.end());
    code_.erase(code_.begin() + index, code_.end());
    return taken;
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
