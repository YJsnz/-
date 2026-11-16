// 诊断信息（实验三错误处理的公共结构）
#pragma once

#include <string>

namespace common {

struct Diagnostic {
    int line = 1;
    int column = 1;
    char code = 'a';  // 实验三错误类别码；词法错误统一为 'a'
    std::string message;
};

}  // namespace common
