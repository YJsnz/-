# 编译原理实验（2026）

按《2026年编译原理实验指导书》分阶段实现一个小型编译器：

**词法分析 → 语法分析 → 错误处理 → 代码生成**

- 实现语言：C++17（MinGW-w64 g++）
- 构建脚本：`scripts/build.sh`
- 开发规范：[AGENTS.md](AGENTS.md)
- 远程仓库：<https://github.com/YJsnz/-.git>

## 当前进度

**阶段 0（基础设施）已完成**：目录结构、构建脚本、验证脚本、截图工具、复现文档模板、官方样例、项目说明。

**实验三（错误处理）进行中**：P3-1～P3-4 已完成，下一次开发：P3-5。

> 当前 `--dump-parser` 已接通语法分析框架（暂时只输出单词）；`--dump-lexer` 仍输出实验一的词法结果。

## 快速开始

### 环境要求

- Windows 11 + Git Bash
- g++ 8.1 及以上（需支持 C++17）
- Python 3 + Pillow（仅截图工具需要）

### 构建与运行

```bash
./scripts/build.sh
./build/compiler                 # 仅检查输入是否可读
./build/compiler --dump-lexer    # 输出词法结果到 output.txt
./build/compiler --dump-parser   # 输出单词与语法成分到 output.txt
./build/compiler --check         # 检查错误并写 error.txt
```

构建产物为 `build/compiler`，该目录已被 `.gitignore` 排除。

### 验证

```bash
./scripts/verify.sh selftest   # 验证脚本自检（假编译器）
./scripts/verify.sh all        # 运行全部用例（当前有 P1 全部、P2-8、P2-13、P3-2～P3-4）
./scripts/verify.sh stage1     # 运行 P1-* 用例
./scripts/test_unit.sh scanner # 运行扫描器单元测试
./scripts/test_unit.sh lexer   # 运行词法分析单元测试
```

### 生成截图

```bash
python tools/capture.py \
  --title "P1-3 标识符与关键字" \
  --cmd "scripts/verify.sh P1-3" \
  --show-file tests/tmp/P1-3/output.txt \
  --out docs/repro/P1-3_标识符与关键字/screenshots/01-verify.png
```

截图与同源的 `run.log` 会写入 `docs/repro/<功能ID>_<名称>/`。

## 目录结构

```text
编译原理/
├── AGENTS.md                      # 开发规范（阶段、TODO、验收、提交）
├── README.md                      # 本文件
├── 2026年编译原理实验指导书.pdf    # 唯一事实来源
├── scripts/
│   ├── build.sh                   # 构建：src/ → build/compiler
│   ├── verify.sh                  # 验证：单用例 / all / stageN / selftest
│   └── test_unit.sh               # 单元测试：tests/unit/<模块>_test.cpp
├── tools/
│   └── capture.py                 # 运行命令并生成截图 + 原始日志
├── src/
│   ├── main.cpp                   # 主程序骨架（读入、开关、输出文件）
│   ├── common/
│   │   ├── diagnostic.h           # 诊断信息结构
│   │   └── error_reporter.h / .cpp # 诊断收集器（实验一只记录）
│   ├── lexer/
│   │   ├── scanner.h / .cpp       # 字符扫描器：游标、行列号、空白、换行
│   │   ├── token.h / .cpp         # 词法单元与类别码
│   │   └── lexer.h / .cpp         # 词法分析器与错误回调
│   ├── parser/
│   │   └── parser.h / .cpp        # 递归下降框架：预读、输出、语法错误回调
│   └── semantic/
│       └── symbol_table.h / .cpp  # 作用域栈与符号表（P3-1）
├── tests/
│   ├── official/                  # 指导书官方样例（exp1~exp4）
│   ├── cases/                     # 功能用例（当前含 P1 全部、P2-8、P2-13、P3-2～P3-4）
│   ├── unit/                      # 单元测试（scanner 等）
│   └── tmp/                       # 运行临时目录（不提交）
├── docs/
│   ├── repro/                     # 每个功能的复现文档、截图与日志
│   │   ├── _TEMPLATE/             # 复现文档模板
│   │   └── P0-*_*/                # 已完成功能的文档
│   └── pcode.md                   # PCODE 指令集（P4-1 创建）
└── build/                         # 构建产物（不提交）
```

## 四个实验

| 阶段 | 实验 | 输入 | 输出文件 | 说明 |
|---|---|---|---|---|
| 一 | 词法分析 | `testfile.txt` | `output.txt` | 每行 `类别码 原文` |
| 二 | 语法分析 | `testfile.txt` | `output.txt` | 单词 + 各语法成分结束处 `<成分名>` |
| 三 | 错误处理 | `testfile.txt` | `error.txt` | 每行 `行号 类别码`（a–p） |
| 四 | 代码生成 | `testfile.txt` + 标准输入 | `pcoderesult.txt` | 生成并解释执行 PCODE |

## 文档导航

- [开发规范 AGENTS.md](AGENTS.md) — 阶段划分、TODO 清单、五步闭环、验收标准、提交规范
- [官方样例说明](tests/official/README.md) — 来源页码、文件映射与转录说明
- [复现文档模板](docs/repro/_TEMPLATE/README.md) — 新功能的文档结构
- 已完成的阶段 0 功能文档：
  - [P0-1 目录结构](docs/repro/P0-1_目录结构/README.md)
  - [P0-2 构建脚本](docs/repro/P0-2_构建脚本/README.md)
  - [P0-3 验证脚本](docs/repro/P0-3_验证脚本/README.md)
  - [P0-4 截图工具](docs/repro/P0-4_截图工具/README.md)
  - [P0-5 复现文档模板](docs/repro/P0-5_复现文档模板/README.md)
  - [P0-6 官方样例](docs/repro/P0-6_官方样例/README.md)
  - [P0-7 项目说明](docs/repro/P0-7_项目说明/README.md)
- 实验一：
  - [P1-1 主程序骨架](docs/repro/P1-1_主程序骨架/README.md)
  - [P1-2 字符扫描器](docs/repro/P1-2_字符扫描器/README.md)
  - [P1-3 标识符与保留字](docs/repro/P1-3_标识符与保留字/README.md)
  - [P1-4 无符号整数](docs/repro/P1-4_无符号整数/README.md)
  - [P1-5 字符常量](docs/repro/P1-5_字符常量/README.md)
  - [P1-6 字符串常量](docs/repro/P1-6_字符串常量/README.md)
  - [P1-7 运算符与界符](docs/repro/P1-7_运算符与界符/README.md)
  - [P1-8 单词结构](docs/repro/P1-8_单词结构/README.md)
  - [P1-9 错误接口](docs/repro/P1-9_错误接口/README.md)
  - [P1-10 官方样例验收](docs/repro/P1-10_官方样例验收/README.md)
  - [P1-11 阶段验收](docs/repro/P1-11_阶段验收/README.md)
- 实验二：
  - [P2-1 语法分析框架](docs/repro/P2-1_语法框架/README.md)
  - [P2-2 常量说明](docs/repro/P2-2_常量说明/README.md)
  - [P2-3 变量说明](docs/repro/P2-3_变量说明/README.md)
  - [P2-4 函数定义](docs/repro/P2-4_函数定义/README.md)
  - [P2-5 主函数与程序](docs/repro/P2-5_主函数与程序/README.md)
  - [P2-6 复合语句](docs/repro/P2-6_复合语句/README.md)
  - [P2-7 表达式与因子](docs/repro/P2-7_表达式与因子/README.md)
  - [P2-8 语句与官方 exp2](docs/repro/P2-8_语句/README.md)
  - [P2-9 条件语句](docs/repro/P2-9_条件语句/README.md)
  - [P2-10 循环语句](docs/repro/P2-10_循环语句/README.md)
  - [P2-11 情况语句](docs/repro/P2-11_情况语句/README.md)
  - [P2-12 函数调用语句](docs/repro/P2-12_函数调用语句/README.md)
  - [P2-13 阶段验收](docs/repro/P2-13_阶段验收/README.md)
- 实验三：
  - [P3-1 符号表](docs/repro/P3-1_符号表/README.md)
  - [P3-2 错误输出](docs/repro/P3-2_错误输出/README.md)
  - [P3-3 词法错误](docs/repro/P3-3_词法错误/README.md)
  - [P3-4 名字重定义](docs/repro/P3-4_名字重定义/README.md)

## 开发约定（摘要）

- **一个功能 = 一次可验收的最小增量 = 一个中文提交**；
- 每个功能按五步闭环完成：预演验证 → 实现 → 自动化验证 → 截图与复现文档 → 提交；
- 每完成一个功能都要更新 `AGENTS.md` 的 TODO 勾选；
- 阶段验收以指导书官方样例的输出逐行一致为准；
- 详见 [AGENTS.md](AGENTS.md)。

## 已知限制

- 实验一（词法分析）已完成并打标签 `stage1`；
- `tests/cases/` 目前有 P1-1 骨架用例与 P1-3～P1-11 词法用例；
- 实验二（语法分析）已完成（`stage2`）；实验三已支持词法错误 a、分隔符错误与名字重定义 b（P3-4），其余语义错误待 P3-5～P3-20；代码生成尚未实现；
- PCODE 指令集尚未设计（P4-1）。
