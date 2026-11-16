# <功能ID> <功能名称>

<!--
复现文档模板（P0-5）

用法：
1. 复制本文件到 docs/repro/<功能ID>_<中文短名>/README.md
2. 创建 screenshots/ 目录，截图命名为 01-<场景>.png
3. 用 tools/capture.py 生成截图与上一级 run.log
4. 替换全部 <...> 占位符，删除本注释块
5. 提交前按 AGENTS.md §4.6 与附录 A 逐项检查
-->

## 1. 目的

<!-- 一句话说明本功能解决什么问题、对应指导书哪一节、对应 TODO 哪个 ID。 -->

## 2. 环境

- 日期：YYYY-MM-DD
- 系统：Windows 11
- 工具链：g++ 8.1.0 / Python 3.14
- 代码版本：`<git log --oneline -1 的输出>`

## 3. 前置条件

<!-- 需要存在的文件、需要先完成的阶段、需要注意的路径。 -->

## 4. 复现步骤

1. 构建：

```bash
scripts/build.sh
```

2. 准备输入：

```bash
cp tests/cases/<功能ID>/testfile.txt <运行目录>/
```

3. 运行（按阶段调整参数）：

```bash
build/compiler --dump-lexer
```

4. 比对：

```bash
scripts/verify.sh <功能ID>
```

## 5. 预期输出

```text
（贴出 expect.txt 或实际运行输出的完整内容）
```

## 6. 截图

![01-验证结果](screenshots/01-verify.png)

<!-- 生成命令示例：
python tools/capture.py \
  --title "<功能ID> <功能名称>" \
  --cmd "scripts/verify.sh <功能ID>" \
  --show-file tests/tmp/<功能ID>/output.txt \
  --out docs/repro/<功能ID>_<名称>/screenshots/01-verify.png
-->

## 7. 备注

<!-- 已知限制、后续阶段会改动的接口、与指导书的差异等。 -->
