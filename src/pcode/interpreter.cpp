#include "pcode/interpreter.h"

#include <cstddef>
#include <utility>

namespace pcode {

namespace {
constexpr std::size_t kMemorySize = 4096;
}

Interpreter::Interpreter(std::vector<Instruction> code, std::istream& in,
                         std::ostream& out)
    : code_(std::move(code)), in_(in), out_(out), memory_(kMemorySize, 0) {}

long long Interpreter::pop() {
    if (stack_.empty()) {
        error_ = "操作数栈下溢";
        return 0;
    }
    const long long value = stack_.back();
    stack_.pop_back();
    return value;
}

void Interpreter::push(long long value) { stack_.push_back(value); }

bool Interpreter::address_ok(long long address) const {
    return address >= 0 && address < static_cast<long long>(memory_.size());
}

bool Interpreter::jump_ok(long long target) const {
    return target >= 0 && target < static_cast<long long>(code_.size());
}

bool Interpreter::run() {
    std::size_t pc = 0;
    while (pc < code_.size()) {
        const Instruction& instruction = code_[pc];
        bool advance_pc = true;


        switch (instruction.op) {
            case OpCode::Lit:
                push(instruction.operand);
                break;
            case OpCode::Lod: {
                if (!address_ok(instruction.operand)) {
                    error_ = "取数地址越界";
                    return false;
                }
                push(memory_[static_cast<std::size_t>(instruction.operand)]);
                break;
            }
            case OpCode::Sto: {
                if (!address_ok(instruction.operand)) {
                    error_ = "存数地址越界";
                    return false;
                }
                memory_[static_cast<std::size_t>(instruction.operand)] = pop();
                if (!error_.empty()) {
                    return false;
                }
                break;
            }
            case OpCode::Opr: {
                const OprCode code = static_cast<OprCode>(instruction.operand);
                switch (code) {
                    case OprCode::Add: {
                        const long long b = pop();
                        const long long a = pop();
                        push(a + b);
                        break;
                    }
                    case OprCode::Sub: {
                        const long long b = pop();
                        const long long a = pop();
                        push(a - b);
                        break;
                    }
                    case OprCode::Mul: {
                        const long long b = pop();
                        const long long a = pop();
                        push(a * b);
                        break;
                    }
                    case OprCode::Div: {
                        const long long b = pop();
                        const long long a = pop();
                        if (b == 0) {
                            error_ = "除数为零";
                            return false;
                        }
                        push(a / b);
                        break;
                    }
                    case OprCode::Neg:
                        push(-pop());
                        break;
                    case OprCode::Eql: {
                        const long long b = pop();
                        const long long a = pop();
                        push(a == b ? 1 : 0);
                        break;
                    }
                    case OprCode::Neq: {
                        const long long b = pop();
                        const long long a = pop();
                        push(a != b ? 1 : 0);
                        break;
                    }
                    case OprCode::Lss: {
                        const long long b = pop();
                        const long long a = pop();
                        push(a < b ? 1 : 0);
                        break;
                    }
                    case OprCode::Geq: {
                        const long long b = pop();
                        const long long a = pop();
                        push(a >= b ? 1 : 0);
                        break;
                    }
                    case OprCode::Gre: {
                        const long long b = pop();
                        const long long a = pop();
                        push(a > b ? 1 : 0);
                        break;
                    }
                    case OprCode::Leq: {
                        const long long b = pop();
                        const long long a = pop();
                        push(a <= b ? 1 : 0);
                        break;
                    }
                    case OprCode::WrtInt:
                        out_ << pop();
                        break;
                    case OprCode::WrtChar:
                        out_ << static_cast<char>(pop() & 0xFF);
                        break;
                    case OprCode::ReadInt: {
                        long long value = 0;
                        if (!(in_ >> value)) {
                            value = 0;
                        }
                        push(value);
                        break;
                    }
                    case OprCode::ReadChar: {
                        char value = 0;
                        in_ >> std::ws;
                        if (!in_.get(value)) {
                            value = 0;
                        }
                        push(static_cast<unsigned char>(value));
                        break;
                    }
                    case OprCode::WrtNl:
                        out_ << '\n';
                        break;
                    case OprCode::Pop:
                        pop();
                        break;
                }
                if (!error_.empty()) {
                    return false;
                }
                break;
            }
            case OpCode::WrtStr:
                out_ << instruction.text;
                break;
            case OpCode::Ldi: {
                const long long address = pop();
                if (!error_.empty()) {
                    return false;
                }
                if (!address_ok(address)) {
                    error_ = "间接取数地址越界";
                    return false;
                }
                push(memory_[static_cast<std::size_t>(address)]);
                break;
            }
            case OpCode::Sti: {
                const long long value = pop();
                const long long address = pop();
                if (!error_.empty()) {
                    return false;
                }
                if (!address_ok(address)) {
                    error_ = "间接存数地址越界";
                    return false;
                }
                memory_[static_cast<std::size_t>(address)] = value;
                break;
            }
            case OpCode::Jmp:
                if (!jump_ok(instruction.operand)) {
                    error_ = "跳转地址越界";
                    return false;
                }
                pc = static_cast<std::size_t>(instruction.operand);
                advance_pc = false;
                break;
            case OpCode::Jpc: {
                const long long condition = pop();
                if (!error_.empty()) {
                    return false;
                }
                if (condition == 0) {
                    if (!jump_ok(instruction.operand)) {
                        error_ = "条件跳转地址越界";
                        return false;
                    }
                    pc = static_cast<std::size_t>(instruction.operand);
                    advance_pc = false;
                }
                break;
            }
            case OpCode::Cal:
                if (!jump_ok(instruction.operand)) {
                    error_ = "调用地址越界";
                    return false;
                }
                call_stack_.push_back(static_cast<int>(pc + 1));
                pc = static_cast<std::size_t>(instruction.operand);
                advance_pc = false;
                break;
            case OpCode::Ret:
                if (call_stack_.empty()) {
                    error_ = "返回栈为空";
                    return false;
                }
                pc = static_cast<std::size_t>(call_stack_.back());
                call_stack_.pop_back();
                advance_pc = false;
                break;
            case OpCode::End:
                return true;
        }

        if (advance_pc) {
            ++pc;
        }
    }
    return true;
}

}  // namespace pcode
