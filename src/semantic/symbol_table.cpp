#include "semantic/symbol_table.h"

namespace semantic {

SymbolTable::SymbolTable() {
    scopes_.emplace_back();  // 全局作用域
}

void SymbolTable::enter_scope() {
    scopes_.emplace_back();
}

void SymbolTable::leave_scope() {
    if (scopes_.size() > 1) {
        scopes_.pop_back();
    }
}

std::string SymbolTable::normalize(const std::string& name) {
    std::string lowered;
    lowered.reserve(name.size());
    for (char c : name) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
        lowered.push_back(c);
    }
    return lowered;
}

bool SymbolTable::insert(const Symbol& symbol) {
    const std::string key = symbol.key.empty() ? normalize(symbol.name) : symbol.key;
    auto& scope = scopes_.back();
    if (scope.find(key) != scope.end()) {
        return false;
    }
    Symbol copy = symbol;
    copy.key = key;
    scope.emplace(key, copy);
    return true;
}

const Symbol* SymbolTable::lookup(const std::string& name) const {
    const std::string key = normalize(name);
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        const auto found = it->find(key);
        if (found != it->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

const Symbol* SymbolTable::lookup_current_scope(const std::string& name) const {
    const std::string key = normalize(name);
    const auto found = scopes_.back().find(key);
    if (found == scopes_.back().end()) {
        return nullptr;
    }
    return &found->second;
}

}  // namespace semantic
