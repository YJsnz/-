// PCODE 栈式解释器
#pragma once

#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "pcode/pcode.h"

namespace pcode {

class Interpreter {
public:
    Interpreter(std::vector<Instruction> code, std::istream& in, std::ostream& out);

    // 顺序执行直到 END 或出错；出错返回 false，原因见 error()。
    bool run();
    const std::string& error() const { return error_; }

private:
    long long pop();
    void push(long long value);
    bool address_ok(long long address) const;

    std::vector<Instruction> code_;
    std::istream& in_;
    std::ostream& out_;
    std::vector<long long> memory_;
    std::vector<long long> stack_;
    std::string error_;
};

}  // namespace pcode
