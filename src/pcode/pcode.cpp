#include "pcode/pcode.h"

namespace pcode {

const char* opcode_name(OpCode op) {
    switch (op) {
        case OpCode::Lit:
            return "LIT";
        case OpCode::Lod:
            return "LOD";
        case OpCode::Sto:
            return "STO";
        case OpCode::Opr:
            return "OPR";
        case OpCode::WrtStr:
            return "WRT_STR";
        case OpCode::Ldi:
            return "LDI";
        case OpCode::Sti:
            return "STI";
        case OpCode::Jmp:
            return "JMP";
        case OpCode::Jpc:
            return "JPC";
        case OpCode::Cal:
            return "CAL";
        case OpCode::Ret:
            return "RET";
        case OpCode::End:
            return "END";
    }
    return "?";
}

const char* opr_name(OprCode op) {
    switch (op) {
        case OprCode::Add:
            return "ADD";
        case OprCode::Sub:
            return "SUB";
        case OprCode::Mul:
            return "MUL";
        case OprCode::Div:
            return "DIV";
        case OprCode::Neg:
            return "NEG";
        case OprCode::Eql:
            return "EQL";
        case OprCode::Neq:
            return "NEQ";
        case OprCode::Lss:
            return "LSS";
        case OprCode::Geq:
            return "GEQ";
        case OprCode::Gre:
            return "GRE";
        case OprCode::Leq:
            return "LEQ";
        case OprCode::WrtInt:
            return "WRT_INT";
        case OprCode::WrtChar:
            return "WRT_CHAR";
        case OprCode::ReadInt:
            return "READ_INT";
        case OprCode::ReadChar:
            return "READ_CHAR";
        case OprCode::WrtNl:
            return "WRT_NL";
        case OprCode::Pop:
            return "POP";
    }
    return "?";
}

}  // namespace pcode
