# PCODE 指令集（P4-1）

本文件定义本编译器使用的目标代码 PCODE。采用**栈式虚拟机**模型，参考 Pascal-S 指令集并按本课程实验需求精简。

## 1. 内存模型

- 一维内存 `mem[0..N-1]`，每个单元保存一个有符号整数（char 以 ASCII 码保存）。
- 实验四的测试程序只包含常量/变量说明、`scanf`、`printf`、赋值，无函数与数组，因此使用单一全局数据区。
- 变量按声明顺序分配地址，从 0 开始递增；常量不占运行时地址，引用时直接生成 `LIT`。

## 2. 指令格式

一条指令由操作码、可选操作数与可选字符串组成：

```text
LIT   n          把整数 n 压栈
LOD   a          把 mem[a] 压栈
STO   a          弹出栈顶并写入 mem[a]
OPR   code       执行运算/输入输出操作（见下表）
WRT_STR "text"   原样输出字符串 text
END              结束程序
```

## 3. OPR 子操作码

| 编码 | 名称 | 含义 |
|---|---|---|
| 1 | ADD | 弹出 b、a，压入 a + b |
| 2 | SUB | 压入 a - b |
| 3 | MUL | 压入 a × b |
| 4 | DIV | 压入 a ÷ b（整除） |
| 5 | NEG | 一元取负 |
| 6 | EQL | a == b → 1/0 |
| 7 | NEQ | a != b → 1/0 |
| 8 | LSS | a < b → 1/0 |
| 9 | GEQ | a >= b → 1/0 |
| 10 | GRE | a > b → 1/0 |
| 11 | LEQ | a <= b → 1/0 |
| 12 | WRT_INT | 弹出并按整数输出 |
| 13 | WRT_CHAR | 弹出并按字符输出（ASCII） |
| 14 | READ_INT | 从标准输入读一个整数并压栈 |
| 15 | READ_CHAR | 从标准输入读一个非空白字符并压栈 |
| 16 | WRT_NL | 输出换行 |

> 关系运算与 `READ_CHAR` 为后续扩展预留；实验四测试程序只需要整数运算与读写。

## 4. 输入输出约定

- `scanf(x)`：生成 `OPR READ_INT`（或 `READ_CHAR`）后接 `STO x`；输入数据每项一行，按空白分隔读取。
- `printf`：
  - `printf("s")`：`WRT_STR "s"`，语句结束前 `OPR WRT_NL`；
  - `printf("s", e)`：`WRT_STR "s"` → e 的代码 → `OPR WRT_INT/WRT_CHAR` → `WRT_NL`；
  - `printf(e)`：e 的代码 → `OPR WRT_INT/WRT_CHAR` → `WRT_NL`。
- 字符型表达式输出字符，整型表达式输出整数。
- 所有结果写入 `pcoderesult.txt`；PCODE 本身不写入结果文件（调试时可另行开启）。

## 5. 代码生成约定

- 表达式采用后序生成：先左操作数、再右操作数，最后生成运算符。
- 一元 `+` 不生成代码；一元 `-` 生成 `OPR NEG`。
- 常量引用直接生成 `LIT`；变量引用生成 `LOD`。
- 赋值：先生成右侧表达式，再 `STO` 到左侧变量地址。
- 字符常量生成其 ASCII 码的 `LIT`。

## 6. 示例

源程序：

```c
int test;
void main(){
    printf("Hello World");
    scanf(test);
    printf(test);
}
```

标准输入：

```text
1837
```

生成的 PCODE（解释执行，不提交）：

```text
WRT_STR "Hello World"
OPR WRT_NL
OPR READ_INT
STO 0
LOD 0
OPR WRT_INT
OPR WRT_NL
END
```

解释执行结果（`pcoderesult.txt`）：

```text
Hello World
1837
```

## 7. 解释器行为

- 顺序执行；`END` 停止。
- 栈初始为空；执行过程中不检查下溢/上溢（测试程序保证正确）。
- 读取整数时跳过空白；读取字符时读取下一个非空白字符。
- 输出直接写入结果流，`WRT_NL` 写换行。
