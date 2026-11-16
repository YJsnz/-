#!/usr/bin/env bash
# 功能验证脚本
# 用法：
#   scripts/verify.sh <用例ID>    运行单个用例
#   scripts/verify.sh all         运行 tests/cases 下全部用例
#   scripts/verify.sh stageN      运行 P<N>-* 用例（N=0..4）
#   scripts/verify.sh selftest    自检：用假编译器验证 PASS/FAIL 判定
# 环境变量：
#   COMPILER=<路径>   指定编译器并跳过构建（默认 build/compiler）
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CASES_DIR="tests/cases"
TMP_ROOT="tests/tmp"

if [ -n "${COMPILER:-}" ]; then
    COMPILER_OVERRIDE=1
else
    COMPILER="build/compiler"
    COMPILER_OVERRIDE=0
fi

PASS_COUNT=0
FAIL_COUNT=0

usage() {
    cat <<'EOF'
用法：
  scripts/verify.sh <用例ID>    运行单个用例，如 verify.sh P1-3
  scripts/verify.sh all         运行全部用例
  scripts/verify.sh stageN      运行 P<N>-* 用例，如 verify.sh stage1
  scripts/verify.sh selftest    自检（假编译器 + 假用例）

用例目录 tests/cases/<ID>/ 需包含：
  testfile.txt   输入源程序
  expect.txt     期望的结果文件内容
  stdin.txt      可选，标准输入
  meta.conf      可选，覆盖默认行为（每行 KEY=VALUE）：
                   OUTPUT=output.txt
                   ARGS=--dump-lexer
                   STDIN=stdin.txt
                   ALLOW_NONZERO=1

结果文件按用例 ID 前缀推断：
  P1-* / P2-* -> output.txt
  P3-*        -> error.txt
  P4-*        -> pcoderesult.txt

环境变量 COMPILER=<路径> 可指定编译器并跳过构建。
EOF
}

is_abs() {
    case "$1" in
        /*|[A-Za-z]:[/\\]*) return 0 ;;
        *) return 1 ;;
    esac
}

compiler_path() {
    if is_abs "$COMPILER"; then
        printf '%s' "$COMPILER"
    else
        printf '%s/%s' "$ROOT" "$COMPILER"
    fi
}

default_output() {
    case "$1" in
        P1-*|P2-*) echo "output.txt" ;;
        P3-*)      echo "error.txt" ;;
        P4-*)      echo "pcoderesult.txt" ;;
        *)         echo "output.txt" ;;
    esac
}

run_case() {
    local id="$1"
    local src="$CASES_DIR/$id"
    local work="$TMP_ROOT/$id"

    if [ ! -d "$src" ]; then
        echo "[FAIL] $id  用例目录不存在：$src"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 1
    fi

    local need
    for need in testfile.txt expect.txt; do
        if [ ! -f "$src/$need" ]; then
            echo "[FAIL] $id  缺少文件：$need"
            FAIL_COUNT=$((FAIL_COUNT + 1))
            return 1
        fi
    done

    local output args_str stdin_name allow_nonzero
    output="$(default_output "$id")"
    args_str=""
    stdin_name=""
    allow_nonzero=0
    if [ -f "$src/meta.conf" ]; then
        local k v
        while IFS='=' read -r k v; do
            case "$k" in
                ""|\#*) continue ;;
                OUTPUT) output="$v" ;;
                ARGS) args_str="$v" ;;
                STDIN) stdin_name="$v" ;;
                ALLOW_NONZERO) allow_nonzero="$v" ;;
            esac
        done < "$src/meta.conf"
    fi

    local -a args=()
    if [ -n "$args_str" ]; then
        read -r -a args <<< "$args_str"
    fi

    rm -rf "$work"
    mkdir -p "$work"
    cp "$src/testfile.txt" "$work/"

    local stdin_path="/dev/null"
    if [ -n "$stdin_name" ]; then
        if [ ! -f "$src/$stdin_name" ]; then
            echo "[FAIL] $id  缺少标准输入文件：$stdin_name"
            FAIL_COUNT=$((FAIL_COUNT + 1))
            return 1
        fi
        cp "$src/$stdin_name" "$work/stdin.txt"
        stdin_path="$ROOT/$work/stdin.txt"
    elif [ -f "$src/stdin.txt" ]; then
        cp "$src/stdin.txt" "$work/stdin.txt"
        stdin_path="$ROOT/$work/stdin.txt"
    fi

    local cpath
    cpath="$(compiler_path)"
    if [ ! -x "$cpath" ]; then
        echo "[FAIL] $id  编译器不可执行：$cpath（先运行 scripts/build.sh）"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 1
    fi

    ( cd "$work" && "$cpath" "${args[@]}" < "$stdin_path" > _stdout.txt 2> _stderr.txt )
    local rc=$?

    if [ "$rc" -ne 0 ] && [ "$allow_nonzero" != "1" ]; then
        echo "[FAIL] $id  编译器退出码 $rc"
        if [ -s "$work/_stderr.txt" ]; then
            head -n 10 "$work/_stderr.txt" | sed 's/^/        /'
        fi
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 1
    fi

    local actual="$work/$output"
    if [ ! -f "$actual" ]; then
        echo "[FAIL] $id  未生成结果文件：$output"
        if [ -s "$work/_stderr.txt" ]; then
            head -n 10 "$work/_stderr.txt" | sed 's/^/        /'
        fi
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 1
    fi

    if cmp -s "$actual" "$src/expect.txt"; then
        echo "[PASS] $id"
        PASS_COUNT=$((PASS_COUNT + 1))
        return 0
    fi

    echo "[FAIL] $id  输出与 expect.txt 不一致（$output）"
    diff -u --label "expected/$output" --label "actual/$output" \
        "$src/expect.txt" "$actual" | head -n 20 | sed 's/^/        /'
    FAIL_COUNT=$((FAIL_COUNT + 1))
    return 1
}

all_ids() {
    local d
    for d in "$CASES_DIR"/*/; do
        [ -d "$d" ] || continue
        basename "$d"
    done | sort
}

stage_ids() {
    local n="$1"
    all_ids | grep -E "^P${n}-" || true
}

ensure_build() {
    if [ "$COMPILER_OVERRIDE" -eq 1 ]; then
        echo "使用指定编译器：$COMPILER（跳过构建）"
        return 0
    fi
    ./scripts/build.sh
}

selftest() {
    local base="$TMP_ROOT/_selftest"
    rm -rf "$base"
    mkdir -p "$base/cases/SE1-fake" "$base/work"

    printf 'hello\nworld\n' > "$base/cases/SE1-fake/testfile.txt"
    printf 'HELLO\nWORLD\n' > "$base/cases/SE1-fake/expect.txt"

    cat > "$base/fake_compiler" <<'EOS'
#!/usr/bin/env bash
set -euo pipefail
tr '[:lower:]' '[:upper:]' < testfile.txt > output.txt
EOS
    chmod +x "$base/fake_compiler"

    local old_cases="$CASES_DIR"
    local old_tmp="$TMP_ROOT"
    local old_compiler="$COMPILER"
    CASES_DIR="$base/cases"
    TMP_ROOT="$base/work"
    COMPILER="$base/fake_compiler"
    PASS_COUNT=0
    FAIL_COUNT=0

    local ok=0

    echo "== 自检 1/2：正常用例应判 PASS =="
    if run_case SE1-fake; then
        ok=$((ok + 1))
    fi

    echo "== 自检 2/2：错误期望应判 FAIL =="
    printf 'WRONG\n' > "$base/cases/SE1-fake/expect.txt"
    if run_case SE1-fake; then
        :
    else
        ok=$((ok + 1))
    fi

    CASES_DIR="$old_cases"
    TMP_ROOT="$old_tmp"
    COMPILER="$old_compiler"
    rm -rf "$base"

    if [ "$ok" -eq 2 ]; then
        echo "自检通过：PASS/FAIL 判定正常"
        return 0
    fi
    echo "自检失败：$ok/2 项符合预期"
    return 1
}

cmd="${1:-}"
case "$cmd" in
    ""|-h|--help)
        usage
        exit 0
        ;;
    selftest)
        selftest
        exit $?
        ;;
    all)
        mapfile -t ids < <(all_ids)
        ;;
    stage*)
        n="${cmd#stage}"
        if ! [[ "$n" =~ ^[0-9]+$ ]]; then
            echo "错误：stageN 中的 N 必须是数字，例如 stage1" >&2
            exit 2
        fi
        mapfile -t ids < <(stage_ids "$n")
        ;;
    *)
        if [ ! -d "$CASES_DIR/$cmd" ]; then
            echo "错误：用例不存在：$CASES_DIR/$cmd" >&2
            exit 2
        fi
        ids=("$cmd")
        ;;
esac

if [ "${#ids[@]}" -eq 0 ]; then
    echo "没有找到用例（$CASES_DIR）"
    exit 0
fi

ensure_build || exit 1

for id in "${ids[@]}"; do
    run_case "$id" || true
done

echo "----------------------------------------"
echo "共 ${#ids[@]} 个用例：${PASS_COUNT} 通过，${FAIL_COUNT} 失败"
if [ "$FAIL_COUNT" -ne 0 ]; then
    exit 1
fi
