#!/usr/bin/env python3
"""运行命令并渲染 PNG 截图，同时写出原始日志。

特性：
- 真实执行命令，捕获 stdout / stderr / 退出码，保证图文一致；
- 中英文混排：ASCII 用等宽字体，含中文的行自动切换中文字体；
- 支持展示结果文件内容（--show-file），满足实验截图的验收要求；
- 日志默认写入 docs/repro/<ID>/run.log（当 --out 位于 screenshots/ 下）。

用法示例：
    python tools/capture.py \
        --title "P1-3 标识符与关键字" \
        --cmd "scripts/verify.sh P1-3" \
        --show-file tests/tmp/P1-3/output.txt \
        --out docs/repro/P1-3_标识符与关键字/screenshots/01-verify.png
"""
import argparse
import datetime
import os
import shutil
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

# ---------------------------------------------------------------- 字体

MONO_CANDIDATES = [
    r"C:\Windows\Fonts\consola.ttf",
    r"C:\Windows\Fonts\Consolas.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/System/Library/Fonts/Menlo.ttc",
]
CJK_CANDIDATES = [
    r"C:\Windows\Fonts\msyh.ttc",
    r"C:\Windows\Fonts\simsun.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/System/Library/Fonts/PingFang.ttc",
]
CJK_BOLD_CANDIDATES = [
    r"C:\Windows\Fonts\msyhbd.ttc",
    r"C:\Windows\Fonts\simhei.ttf",
] + CJK_CANDIDATES

FONT_MONO = None
FONT_CJK = None
FONT_CJK_BOLD = None

# ---------------------------------------------------------------- 外观

BG = (30, 30, 30)
FG = (212, 212, 212)
PROMPT = (106, 153, 85)
CMDC = (220, 220, 170)
META = (140, 140, 140)
ACCENT = (86, 156, 214)
OKC = (106, 153, 85)
ERRC = (244, 135, 113)
FILEC = (156, 220, 254)

MARGIN = 36
LINE_H = 24
BODY_SIZE = 16
TITLE_SIZE = 26
META_SIZE = 14
DEFAULT_WIDTH = 1240
MAX_WIDTH = 1400


def first_existing(paths):
    for p in paths:
        if p and os.path.exists(p):
            return p
    return None


def font_for(text, size, bold=False):
    has_cjk = any(ord(c) > 127 for c in text)
    if bold:
        path = FONT_CJK_BOLD or FONT_CJK or FONT_MONO
    elif has_cjk:
        path = FONT_CJK or FONT_MONO
    else:
        path = FONT_MONO
    if not path:
        return ImageFont.load_default()
    return ImageFont.truetype(path, size)


# ---------------------------------------------------------------- 执行与读取


def decode(data):
    if not data:
        return ""
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return data.decode("gbk", errors="replace")


def run_command(cmd, cwd, bash, timeout):
    try:
        p = subprocess.run([bash, "-lc", cmd], cwd=cwd, capture_output=True,
                           timeout=timeout)
        return decode(p.stdout), decode(p.stderr), p.returncode
    except subprocess.TimeoutExpired as exc:
        out = decode(exc.stdout or b"")
        return out, f"命令超时（超过 {timeout:g} 秒）", 124
    except OSError as exc:
        return "", f"命令启动失败：{exc}", 126


def read_text(path):
    try:
        with open(path, "rb") as f:
            return decode(f.read())
    except OSError as exc:
        return f"<读取失败：{exc}>"


def git_commit(cwd):
    try:
        p = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=cwd,
                           capture_output=True, timeout=5)
        return decode(p.stdout).strip() or "(无提交)"
    except Exception:
        return "(无提交)"


# ---------------------------------------------------------------- 排版


def wrap_line(text, font, max_w, draw):
    if text == "":
        return [""]
    lines = []
    cur = ""
    for ch in text:
        if draw.textlength(cur + ch, font=font) <= max_w:
            cur += ch
        else:
            lines.append(cur)
            cur = ch
    lines.append(cur)
    return lines


def add_block(blocks, kind, text, content_w, draw, cont_prefix=""):
    text = text.replace("\t", "    ")
    first = True
    for raw in text.splitlines() or [""]:
        f = font_for(raw, BODY_SIZE)
        for piece in wrap_line(raw, f, content_w, draw):
            if not first and cont_prefix:
                piece = cont_prefix + piece
            blocks.append((kind, piece, f))
            first = False


def default_log_path(out_path):
    out_abs = os.path.abspath(out_path)
    out_dir = os.path.dirname(out_abs)
    if os.path.basename(out_dir) == "screenshots":
        return os.path.join(os.path.dirname(out_dir), "run.log")
    return os.path.splitext(out_abs)[0] + ".log"


# ---------------------------------------------------------------- 主流程


def main():
    global FONT_MONO, FONT_CJK, FONT_CJK_BOLD

    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

    ap = argparse.ArgumentParser(description="运行命令并渲染 PNG 截图 + 原始日志")
    ap.add_argument("--title", required=True, help="截图标题")
    ap.add_argument("--out", required=True, help="输出 PNG 路径")
    ap.add_argument("--log", default=None, help="原始日志路径（默认按 AGENTS.md 规则推导）")
    ap.add_argument("--cmd", action="append", default=[], help="要执行的命令，可重复")
    ap.add_argument("--show-file", action="append", default=[],
                    help="在截图中展示的文件内容，可重复")
    ap.add_argument("--cwd", default=os.getcwd(), help="命令工作目录")
    ap.add_argument("--timeout", type=float, default=120.0, help="单条命令超时秒数")
    ap.add_argument("--width", type=int, default=DEFAULT_WIDTH, help="图片宽度（≤1400）")
    ap.add_argument("--font", default=None, help="ASCII 等宽字体路径")
    ap.add_argument("--cjk-font", default=None, help="中文字体路径")
    args = ap.parse_args()

    if not args.cmd and not args.show_file:
        ap.error("至少提供一条 --cmd 或 --show-file")

    FONT_MONO = args.font or first_existing(MONO_CANDIDATES)
    FONT_CJK = args.cjk_font or first_existing(CJK_CANDIDATES)
    FONT_CJK_BOLD = args.cjk_font or first_existing(CJK_BOLD_CANDIDATES)
    if not FONT_MONO:
        print("警告：未找到等宽字体，使用 PIL 默认字体", file=sys.stderr)
    if not FONT_CJK:
        print("警告：未找到中文字体，中文可能显示为方框", file=sys.stderr)

    cwd = os.path.abspath(args.cwd)
    if not os.path.isdir(cwd):
        print(f"错误：工作目录不存在：{cwd}", file=sys.stderr)
        return 1

    bash = shutil.which("bash") or r"C:\Program Files\Git\usr\bin\bash.exe"
    if not (os.path.exists(bash) or shutil.which(bash)):
        print("错误：找不到 bash，请先安装 Git Bash", file=sys.stderr)
        return 1

    width = max(600, min(args.width, MAX_WIDTH))
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    commit = git_commit(cwd)

    log_path = args.log or default_log_path(args.out)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)) or ".", exist_ok=True)
    os.makedirs(os.path.dirname(os.path.abspath(log_path)) or ".", exist_ok=True)

    results = []
    for cmd in args.cmd:
        out, err, code = run_command(cmd, cwd, bash, args.timeout)
        results.append((cmd, out, err, code))

    files = [(p, read_text(p)) for p in args.show_file]

    # -------- 日志
    log_parts = [
        f"# {args.title}",
        f"# 时间: {now}",
        f"# 提交: {commit}",
        f"# 目录: {cwd}",
        "# 工具: tools/capture.py",
        "",
    ]
    for cmd, out, err, code in results:
        log_parts.append(f"$ {cmd}")
        if out:
            log_parts.append(out.rstrip("\n"))
        if err.strip():
            log_parts += ["[stderr]", err.rstrip("\n")]
        log_parts.append(f"[exit {code}]")
        log_parts.append("")
    for path, content in files:
        log_parts += [f"# 文件：{path}", content.rstrip("\n"), ""]

    with open(log_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(log_parts))

    # -------- 排版
    probe = Image.new("RGB", (10, 10))
    pd = ImageDraw.Draw(probe)
    content_w = width - MARGIN * 2

    blocks = []
    for cmd, out, err, code in results:
        add_block(blocks, "cmd", cmd, content_w, pd, cont_prefix="  ")
        if out:
            add_block(blocks, "out", out.rstrip("\n"), content_w, pd)
        if err.strip():
            add_block(blocks, "err", err.rstrip("\n"), content_w, pd)
        add_block(blocks, "exit", f"[exit {code}]", content_w, pd)
        blocks.append(("blank", "", font_for("", BODY_SIZE)))
    for path, content in files:
        add_block(blocks, "file", f"# 文件：{path}", content_w, pd)
        add_block(blocks, "out", content.rstrip("\n"), content_w, pd)
        blocks.append(("blank", "", font_for("", BODY_SIZE)))

    header_h = 26 + 36 + 26 + 10
    footer_h = 24 + 16
    height = header_h + len(blocks) * LINE_H + footer_h

    img = Image.new("RGB", (width, height), BG)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, width, 4], fill=ACCENT)

    title_f = font_for(args.title, TITLE_SIZE, bold=True)
    meta = f"{now}    commit {commit}    {cwd}"
    meta_f = font_for(meta, META_SIZE)

    y = 26
    d.text((MARGIN, y), args.title, font=title_f, fill=FG)
    y += 38
    d.text((MARGIN, y), meta, font=meta_f, fill=META)
    y += 34

    for kind, text, f in blocks:
        if kind == "cmd":
            d.text((MARGIN, y), "$ ", font=f, fill=PROMPT)
            d.text((MARGIN + d.textlength("$ ", font=f), y), text, font=f, fill=CMDC)
        elif kind == "out":
            d.text((MARGIN, y), text, font=f, fill=FG)
        elif kind == "err":
            d.text((MARGIN, y), text, font=f, fill=ERRC)
        elif kind == "file":
            d.text((MARGIN, y), text, font=f, fill=FILEC)
        elif kind == "exit":
            d.text((MARGIN, y), text, font=f, fill=OKC if text == "[exit 0]" else ERRC)
        y += LINE_H

    y += 10
    footer = "由 tools/capture.py 生成（真实命令输出）"
    d.text((MARGIN, y), footer, font=font_for(footer, META_SIZE), fill=META)

    img.save(args.out)

    failed = sum(1 for _, _, _, code in results if code != 0)
    print(f"PNG -> {args.out}")
    print(f"LOG -> {log_path}")
    print(f"完成：{len(results)} 条命令，{failed} 条非零退出")
    return 0


if __name__ == "__main__":
    sys.exit(main())
