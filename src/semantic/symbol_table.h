// 符号表：作用域、种类、类型、数组维度与函数签名（P3-1）
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace semantic {

enum class SymbolKind {
    Constant,
    Variable,
    Array,
    Function,
};

enum class DataType {
    Unknown,
    Int,
    Char,
    Void,
};

struct Symbol {
    std::string name;  // 原样名字
    std::string key;   // 小写名字（查找用）
    SymbolKind kind = SymbolKind::Variable;
    DataType type = DataType::Int;
    int dimensions = 0;       // 数组维数（0/1/2）
    bool has_value = false;   // 常量是否有已知值
    long long int_value = 0;  // 常量整数值
    char char_value = 0;      // 常量字符值
    std::vector<DataType> parameter_types;  // 函数参数类型
};

// 作用域按进入顺序保存；查找从最内层向外。
class SymbolTable {
public:
    SymbolTable();

    void enter_scope();
    void leave_scope();
    int scope_depth() const { return static_cast<int>(scopes_.size()); }

    // 插入到当前作用域；同作用域重名（不区分大小写）返回 false。
    bool insert(const Symbol& symbol);

    // 从当前作用域向外查找；未找到返回 nullptr。
    const Symbol* lookup(const std::string& name) const;

    // 只在当前作用域查找。
    const Symbol* lookup_current_scope(const std::string& name) const;

    static std::string normalize(const std::string& name);

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
};

}  // namespace semantic
