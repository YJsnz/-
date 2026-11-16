#!/usr/bin/env bash
# 编译并运行单元测试
# 用法：scripts/test_unit.sh <名称>   例如 scanner
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17 -Wall -Wextra -O2}"
OUT_DIR="build/unit"

name="${1:-}"
if [ -z "$name" ]; then
    echo "用法：scripts/test_unit.sh <名称>   例如 scanner" >&2
    exit 2
fi

src="tests/unit/${name}_test.cpp"
if [ ! -f "$src" ]; then
    echo "错误：找不到单元测试 $src" >&2
    exit 2
fi

mapfile -t SOURCES < <(find src -type f -name '*.cpp' ! -name 'main.cpp' | sort)
mkdir -p "$OUT_DIR"
out="$OUT_DIR/${name}_test"

echo "编译 $src"
# CXXFLAGS 需要按空格拆分成多个参数
# shellcheck disable=SC2086
"$CXX" $CXXFLAGS -Isrc "$src" "${SOURCES[@]}" -o "$out"

echo "运行 $out"
"$out"
