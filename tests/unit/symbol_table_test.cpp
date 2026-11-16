#include <iostream>
#include <string>

#include "semantic/symbol_table.h"

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
    semantic::SymbolTable table;

    check(table.lookup("a") == nullptr, "空表查找返回空");
    check(table.lookup_current_scope("a") == nullptr, "空表当前作用域查找返回空");
    check(table.scope_depth() == 1, "初始只有全局作用域");

    semantic::Symbol variable;
    variable.name = "Alpha";
    variable.kind = semantic::SymbolKind::Variable;
    variable.type = semantic::DataType::Int;
    check(table.insert(variable), "插入全局变量成功");
    check(!table.insert(variable), "同作用域重名插入失败");

    const semantic::Symbol* found = table.lookup("alpha");
    check(found != nullptr && found->type == semantic::DataType::Int,
          "大小写不敏感查找成功");
    check(found != nullptr && found->name == "Alpha", "保留原始名字");

    table.enter_scope();
    check(table.scope_depth() == 2, "进入作用域后深度为 2");
    check(table.lookup_current_scope("Alpha") == nullptr, "内层当前作用域看不到外层");

    semantic::Symbol inner;
    inner.name = "alpha";
    inner.type = semantic::DataType::Char;
    check(table.insert(inner), "内层可定义同名变量");
    check(table.lookup("alpha") != nullptr &&
              table.lookup("alpha")->type == semantic::DataType::Char,
          "内层变量遮蔽外层");
    table.leave_scope();
    check(table.lookup("alpha") != nullptr &&
              table.lookup("alpha")->type == semantic::DataType::Int,
          "离开作用域后恢复外层变量");

    semantic::Symbol function;
    function.name = "f";
    function.kind = semantic::SymbolKind::Function;
    function.type = semantic::DataType::Int;
    function.parameter_types = {semantic::DataType::Int, semantic::DataType::Char};
    check(table.insert(function), "插入函数符号");
    const semantic::Symbol* fn = table.lookup("f");
    check(fn != nullptr && fn->kind == semantic::SymbolKind::Function &&
              fn->type == semantic::DataType::Int && fn->parameter_types.size() == 2,
          "函数签名保留");

    semantic::Symbol array;
    array.name = "m";
    array.kind = semantic::SymbolKind::Array;
    array.type = semantic::DataType::Int;
    array.dimensions = 2;
    table.insert(array);
    check(table.lookup("m") != nullptr && table.lookup("m")->dimensions == 2,
          "数组维度保留");

    semantic::Symbol constant;
    constant.name = "N";
    constant.kind = semantic::SymbolKind::Constant;
    constant.type = semantic::DataType::Int;
    constant.has_value = true;
    constant.int_value = 42;
    table.insert(constant);
    check(table.lookup("n") != nullptr && table.lookup("n")->int_value == 42,
          "常量值保留");

    check(semantic::SymbolTable::normalize("AbC_1") == "abc_1", "名字归一化为小写");

    if (failures == 0) {
        std::cout << "全部通过\n";
        return 0;
    }
    std::cout << failures << " 项失败\n";
    return 1;
}
