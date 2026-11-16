# 编译原理实验（2026）

按《2026年编译原理实验指导书》分阶段实现一个小型编译器：

**词法分析 → 语法分析 → 错误处理 → 代码生成**

- 实现语言：C++17（MinGW-w64 g++）
- 构建脚本：`scripts/build.sh`
- 开发规范：[AGENTS.md](AGENTS.md)
- 远程仓库：<https://github.com/YJsnz/-.git>

## 当前进度

**阶段 0（基础设施）已完成**：目录结构、构建脚本、验证脚本、截图工具、复现文档模板、官方样例、项目说明。

**实验一（词法分析）尚未开始**，下一次开发：P1-1。

> 当前 `build/compiler` 只是占位入口，运行输出 `compiler skeleton: build OK (P1-1 will add lexer)`。

## 快速开始

### 环境要求

- Windows 11 + Git Bash
- g++ 8.1 及以上（需支持 C++17）
- Python 3 + Pillow（仅截图工具需要）

### 构建与运行

```bash
./scripts/build.sh
./build/compiler
```

构建产物为 `build/compiler`，该目录已被 `.gitignore` 排除。

### 验证

```bash
./scripts/verify.sh selftest   # 验证脚本自检（假编译器）
./scripts/verify.sh all        # 运行全部用例（当前还没有正式用例）
./scripts/verify.sh stage1     # 运行 P1-* 用例
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
│   └── verify.sh                  # 验证：单用例 / all / stageN / selftest
├── tools/
│   └── capture.py                 # 运行命令并生成截图 + 原始日志
├── src/
│   └── main.cpp                   # 入口（当前为占位桩，P1-1 替换）
├── tests/
│   ├── official/                  # 指导书官方样例（exp1~exp4）
│   ├── cases/                     # 功能用例（P1-10 起添加）
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

## 开发约定（摘要）

- **一个功能 = 一次可验收的最小增量 = 一个中文提交**；
- 每个功能按五步闭环完成：预演验证 → 实现 → 自动化验证 → 截图与复现文档 → 提交；
- 每完成一个功能都要更新 `AGENTS.md` 的 TODO 勾选；
- 阶段验收以指导书官方样例的输出逐行一致为准；
- 详见 [AGENTS.md](AGENTS.md)。

## 已知限制

- 词法分析、语法分析、错误处理、代码生成均尚未实现；
- `tests/cases/` 目前为空，`verify.sh all` 只验证脚本自身；
- PCODE 指令集尚未设计（P4-1）。
