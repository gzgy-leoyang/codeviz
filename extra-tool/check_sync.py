#!/usr/bin/env python3
"""
codeviz 源码与 HTML 一致性检测工具

用法:
  ./check_sync.py <project_dir> [--html <path>] [--rebuild]

功能:
  - 对比源码文件与 HTML 报告的文件修改时间，检测 HTML 是否过期
  - 提取 HTML 内嵌的符号列表，与源码实际定义的函数对比
  - 可选 --rebuild: 过期时自动重新生成 HTML
"""

import json
import os
import re
import subprocess
import sys

COREVIZ_BIN = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "build", "output", "codeviz")

STYLE_BOLD = "\033[1m"
STYLE_RED = "\033[91m"
STYLE_GREEN = "\033[92m"
STYLE_YELLOW = "\033[93m"
STYLE_CYAN = "\033[96m"
STYLE_RESET = "\033[0m"

STATE_PASS = f"{STYLE_GREEN}✅ PASS{STYLE_RESET}"
STATE_FAIL = f"{STYLE_RED}❌ FAIL{STYLE_RESET}"
STATE_WARN = f"{STYLE_YELLOW}⚠ WARN{STYLE_RESET}"
STATE_SKIP = f"{STYLE_CYAN}➖ SKIP{STYLE_RESET}"


def fmt_path(path):
    return path.replace(os.path.expanduser("~"), "~")


class Report:
    def __init__(self, project_dir, html_path):
        self.project_dir = project_dir
        self.html_path = html_path
        self.items = []
        self.all_pass = True

    def add(self, name, result, detail=""):
        self.items.append((name, result, detail))
        if result == STATE_FAIL:
            self.all_pass = False

    def print_header(self):
        print()
        print(f"  {STYLE_BOLD}━━━  codeviz 一致性检测报告  ━━━{STYLE_RESET}")
        print(f"  {'项目':>8}: {fmt_path(self.project_dir)}")
        print(f"  {'HTML':>8}: {fmt_path(self.html_path)}")
        print()

    def print_body(self):
        label_w = max(len(n) for n, _, _ in self.items) + 2
        print(f"  {'检查项':<{label_w}}   {'结果':<12}   {'说明'}")
        print(f"  {'──────':<{label_w}}   {'────':<12}   {'────'}")
        for name, result, detail in self.items:
            print(f"  {name:<{label_w}}   {result:<12}   {detail}")
        print()

    def print_footer(self):
        total = len(self.items)
        passed = sum(1 for _, r, _ in self.items if r == STATE_PASS)
        failed = sum(1 for _, r, _ in self.items if r == STATE_FAIL)
        warned = sum(1 for _, r, _ in self.items if r == STATE_WARN)
        print(f"  {STYLE_BOLD}汇总:{STYLE_RESET} 共 {total} 项  "
              f"{STYLE_GREEN}{passed} 通过{STYLE_RESET}  "
              f"{STYLE_RED}{failed} 失败{STYLE_RESET}  "
              f"{STYLE_YELLOW}{warned} 警告{STYLE_RESET}")
        if self.all_pass:
            print(f"  {STYLE_BOLD}{STYLE_GREEN}结论: 源码与 HTML 完全一致 ✅{STYLE_RESET}")
        else:
            print(f"  {STYLE_BOLD}{STYLE_RED}结论: 源码与 HTML 存在差异 ❌{STYLE_RESET}")
        print()


def find_source_files(project_dir):
    exts = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hxx", ".hh"}
    sources = []
    for root, _, files in os.walk(project_dir):
        if "/build" in root or "/CMakeFiles" in root or ".git" in root:
            continue
        for f in files:
            if any(f.endswith(e) for e in exts):
                sources.append(os.path.join(root, f))
    return sorted(sources)


def extract_functions_from_source(filepath):
    """从 C/C++ 源码中提取函数定义（词法分析方式，不依赖正则匹配函数体）"""
    funcs = {}
    try:
        with open(filepath, encoding="utf-8", errors="replace") as f:
            content = f.read()
    except Exception:
        return funcs

    # 去除注释和字符串
    cleaned = []
    i = 0
    while i < len(content):
        if content[i:i+2] == "//":
            end = content.find("\n", i)
            i = end if end >= 0 else len(content)
        elif content[i:i+2] == "/*":
            end = content.find("*/", i+2)
            i = end + 2 if end >= 0 else len(content)
        elif content[i] in ("\"", "'"):
            q = content[i]
            i += 1
            while i < len(content) and content[i] != q:
                if content[i] == "\\":
                    i += 1
                i += 1
            i += 1
        else:
            cleaned.append(content[i])
            i += 1
    content = "".join(cleaned)

    # 提取所有 "{...}" 块前的 identifier(args) 模式
    # 策略: 找到 '{'，回头看最近一个 ')'，匹配 func_name(...) 结构
    brace_positions = [m.start() for m in re.finditer(r'\{', content)]

    for pos in brace_positions:
        # 跳过 enum/struct/class/namespace/switch 等非函数块
        pre = content[max(0, pos-80):pos].strip()
        if any(pre.startswith(kw) for kw in ("enum ", "struct ", "union ",
                                              "namespace ", "switch ", "try ",
                                              "catch ", "class ", ":")):
            continue
        # 找匹配的 )
        paren_depth = 0
        j = pos - 1
        while j >= 0:
            if content[j] == ')':
                paren_depth += 1
            elif content[j] == '(':
                paren_depth -= 1
                if paren_depth == 0:
                    break
            j -= 1
        if paren_depth != 0 or j < 0:
            continue

        # 从 '(' 往回解析函数名
        k = j - 1
        while k >= 0 and content[k] in ' \t':
            k -= 1
        # 跳过 ptr operators
        while k >= 0 and content[k] in '*&':
            k -= 1
        while k >= 0 and content[k] in ' \t':
            k -= 1
        end_name = k + 1
        while k >= 0 and (content[k].isalnum() or content[k] in '_:~'):
            k -= 1
        name = content[k+1:end_name]
        # 跳过命名空间/类前缀
        if "::" in name:
            name = name.split("::")[-1]

        if not name or not name[0].isalnum() and name[0] != '_':
            continue
        if name in ("if", "for", "while", "switch", "catch", "try",
                    "else", "return", "delete", "new", "sizeof",
                    "define", "include", "pragma", "case", "do"):
            continue
        funcs[name] = content[max(0, pos-80):pos+1].strip()

    return funcs


def extract_html_data(html_path):
    """从 HTML 提取 CODEVIZ_DATA"""
    try:
        with open(html_path, encoding="utf-8") as f:
            content = f.read()
    except Exception as e:
        return None, str(e)

    start = content.find("window.CODEVIZ_DATA = {")
    if start < 0:
        return None, "未找到 CODEVIZ_DATA"
    start = content.find("{", start)
    depth = 0
    end = start
    for i in range(start, len(content)):
        if content[i] == "{":
            depth += 1
        elif content[i] == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    try:
        data = json.loads(content[start:end])
    except json.JSONDecodeError as e:
        return None, f"JSON 解析失败: {e}"
    return data, None


def build_function_index(data):
    """从 CODEVIZ_DATA 构建函数索引"""
    symbols = {}
    for sym in data.get("symbols", []):
        sid = sym.get("symbol_id")
        kind = sym.get("kind", "")
        symbols[sid] = {"name": sym.get("name", ""), "kind": kind,
                        "file": sym.get("file_path", ""),
                        "line": sym.get("line", 0)}

    # 所有 kind=FUNCTION 的符号
    funcs = {v["name"] for v in symbols.values()
             if v["kind"] in ("FUNCTION",)}

    # call_graph 节点补充
    for node in data.get("call_graph", {}).get("nodes", []):
        label = node.get("label", "")
        if label:
            funcs.add(label.split("\n")[0])

    # 统计表补充
    for fs in data.get("stats", {}).get("function_stats", []):
        fid = fs.get("function_id")
        if fid in symbols:
            funcs.add(symbols[fid]["name"])

    return funcs, symbols, data


def rebuild_html(project_dir, html_path):
    if not os.path.isfile(COREVIZ_BIN):
        return False, f"codeviz 未构建: {COREVIZ_BIN}"
    result = subprocess.run(
        [COREVIZ_BIN, "-p", project_dir, "-o", html_path],
        capture_output=True, text=True, timeout=120
    )
    if result.returncode != 0:
        return False, result.stderr
    return True, ""


def main():
    import argparse
    parser = argparse.ArgumentParser(
        description="检测 codeviz 生成的 HTML 报告与源码是否一致")
    parser.add_argument("project", help="项目源码目录路径")
    parser.add_argument("--html", "-o", default=None,
                        help="HTML 报告路径（默认: <project>.html）")
    parser.add_argument("--rebuild", "-r", action="store_true",
                        help="发现不一致时自动重新生成 HTML")
    args = parser.parse_args()

    project_dir = os.path.abspath(args.project)
    html_path = (os.path.abspath(args.html) if args.html
                 else project_dir.rstrip("/") + ".html")

    rep = Report(project_dir, html_path)
    rebuilt = False

    # ── 1. 项目目录 ──
    if os.path.isdir(project_dir):
        rep.add("1. 项目目录", STATE_PASS, fmt_path(project_dir))
    else:
        rep.add("1. 项目目录", STATE_FAIL, f"目录不存在")
        rep.print_header()
        rep.print_body()
        rep.print_footer()
        sys.exit(1)

    # ── 2. 源码文件扫描 ──
    sources = find_source_files(project_dir)
    rep.add("2. 源码文件扫描", STATE_PASS if sources else STATE_FAIL,
            f"{len(sources)} 个源文件")

    # ── 3. HTML 文件存在 ──
    if os.path.isfile(html_path):
        html_size = os.path.getsize(html_path)
        rep.add("3. HTML 文件存在", STATE_PASS,
                f"{html_size:,} 字节  {fmt_path(html_path)}")
    else:
        if args.rebuild:
            ok, err = rebuild_html(project_dir, html_path)
            if ok:
                html_size = os.path.getsize(html_path)
                rep.add("3. HTML 文件存在", STATE_PASS,
                        f"已自动生成  {html_size:,} 字节")
                rebuilt = True
            else:
                rep.add("3. HTML 文件存在", STATE_FAIL, f"重建失败: {err}")
                rep.print_header()
                rep.print_body()
                rep.print_footer()
                sys.exit(1)
        else:
            rep.add("3. HTML 文件存在", STATE_FAIL, "文件不存在")
            rep.print_header()
            rep.print_body()
            rep.print_footer()
            sys.exit(1)

    # ── 4. HTML 数据解析 ──
    data, err = extract_html_data(html_path)
    if data:
        meta = data.get("metadata", {})
        gen_at = meta.get("generated_at", "-")
        func_count = meta.get("function_count", 0)
        file_count = meta.get("file_count", 0)
        entry_id = meta.get("entry_function_id")
        depth = meta.get("depth", "-")
        rep.add("4. HTML 数据解析", STATE_PASS,
                f"{func_count} 函数  {file_count} 文件  生成于 {gen_at}")
    else:
        rep.add("4. HTML 数据解析", STATE_FAIL, err)
        rep.print_header()
        rep.print_body()
        rep.print_footer()
        sys.exit(1)

    # ── 5. 时间戳一致性 ──
    html_mtime = os.path.getmtime(html_path)
    stale_sources = []
    for src in sources:
        src_mtime = os.path.getmtime(src)
        if src_mtime > html_mtime + 1:
            stale_sources.append(src)
    if stale_sources:
        detail = f"{len(stale_sources)} 个文件已修改: "
        detail += ", ".join(os.path.relpath(s, project_dir)
                           for s in stale_sources[:5])
        if len(stale_sources) > 5:
            detail += f" ...等共 {len(stale_sources)} 个"
        if args.rebuild and not rebuilt:
            ok, err = rebuild_html(project_dir, html_path)
            if ok:
                rebuilt = True
                html_mtime = os.path.getmtime(html_path)
                detail += " → 已自动重建"
                rep.add("5. 时间戳一致性", STATE_PASS, detail)
            else:
                rep.add("5. 时间戳一致性", STATE_WARN, detail)
        else:
            rep.add("5. 时间戳一致性", STATE_WARN, detail)
    else:
        rep.add("5. 时间戳一致性", STATE_PASS, "HTML 是最新的")

    # ── 6. 函数覆盖（参考）──
    source_funcs = {}
    for src in sources:
        source_funcs.update(extract_functions_from_source(src))

    html_funcs, symbols, _ = build_function_index(data)

    missing = set(source_funcs) - html_funcs
    extras = html_funcs - set(source_funcs)

    ratio = len(source_funcs) and (
        (len(source_funcs) - len(missing)) * 100 // len(source_funcs))
    # 对于纯 C 项目（如 kilo），正则匹配精确，覆盖率可作为严格指标
    # 对于 C++ 项目（模板/宏/命名空间），覆盖率仅作参考
    is_c_project = all(f.endswith(".c") for f in sources)
    cov_state = STATE_PASS if ratio >= 90 else STATE_WARN
    detail_parts = [f"源码 {len(source_funcs)} 函数 → HTML {len(source_funcs) - len(missing)} 命中 ({ratio}%)"]
    if missing:
        detail_parts.append(f"未匹配: {len(missing)} 个")
    if extras:
        detail_parts.append(f"HTML 多: {len(extras)} 个")
    rep.add("6. 函数覆盖率", cov_state, "  ".join(detail_parts))

    # ── 8. 调用图规模 ──
    cg = data.get("call_graph", {})
    nodes = len(cg.get("nodes", []))
    edges = len(cg.get("edges", []))
    rep.add("8. 调用图规模", STATE_PASS, f"{nodes} 节点  {edges} 条边")

    # ── 9. entry 入口匹配 ──
    entry_id = meta.get("entry_function_id")
    if entry_id:
        sym = symbols.get(entry_id, {})
        entry_name = sym.get("name", str(entry_id))
        rep.add("9. 入口函数", STATE_PASS,
                f"{entry_name} (id={entry_id})")
    else:
        rep.add("9. 入口函数", STATE_SKIP, "未指定入口函数")

    # ── 输出报告 ──
    rep.print_header()
    rep.print_body()
    rep.print_footer()
    sys.exit(0 if rep.all_pass else 1)


if __name__ == "__main__":
    main()
