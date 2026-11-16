#!/usr/bin/env bash
# 构建脚本：编译 src/ 下全部 .cpp，输出 build/compiler
# 用法：scripts/build.sh
# 可通过环境变量覆盖：CXX、CXXFLAGS
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17 -Wall -Wextra -O2}"
INCLUDES="-Isrc"
OUT_DIR="build"
OUT="$OUT_DIR/compiler"

mkdir -p "$OUT_DIR"

mapfile -t SOURCES < <(find src -type f -name '*.cpp' | sort)
if [ "${#SOURCES[@]}" -eq 0 ]; then
    echo "错误：src/ 下没有 .cpp 源文件" >&2
    exit 1
fi

echo "编译器    = $("$CXX" --version | head -n 1)"
echo "源文件数  = ${#SOURCES[@]}"
echo "输出      = $OUT"

# CXXFLAGS 需要按空格拆分成多个参数
# shellcheck disable=SC2086
"$CXX" $CXXFLAGS $INCLUDES "${SOURCES[@]}" -o "$OUT"

echo "构建成功：$OUT"
