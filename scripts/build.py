#!/usr/bin/env python3
"""交互式构建 helper：构建 demo/ 目录下的一个目标。

用法:
    python scripts/build.py                 # 方向键交互菜单
    python scripts/build.py <目标名>         # 直接构建，如 scripts/build.py 0_sdl_setup
    python scripts/build.py --clean <目标名>  # 先删除 build 目录再构建

每个 demo 可在自身目录放一个「编译前脚本」，cmake 之前自动运行：
    pre_build.py   (跨平台，首选，用当前 Python 运行)
    pre_build.ps1  (仅 Windows)
    pre_build.sh   (仅 Linux/macOS)

由根目录的 build.bat 调用（build.bat 负责找 Python 并切换控制台为 UTF-8）。
"""
import argparse
import os
import shutil
import subprocess
import sys

# Windows 控制台默认可能是 GBK，统一成 UTF-8，配合 build.bat 的 chcp 65001；
# line_buffering=True 保证 print 与 cmake 子进程的输出顺序正确。
if sys.platform == "win32":
    for _s in (sys.stdout, sys.stderr):
        try:
            _s.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
        except (AttributeError, ValueError):
            pass

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEMO_DIR = os.path.join(REPO_ROOT, "demo")


def find_targets():
    """返回 demo/ 下所有含 CMakeLists.txt 的子目录，按名字排序 (name, path)。"""
    targets = []
    if os.path.isdir(DEMO_DIR):
        for name in sorted(os.listdir(DEMO_DIR)):
            d = os.path.join(DEMO_DIR, name)
            if os.path.isdir(d) and os.path.isfile(os.path.join(d, "CMakeLists.txt")):
                targets.append((name, d))
    return targets


def pick_generator():
    """返回 CMake 的 -G 值；找不到 MinGW 时返回 None（让 CMake 自动选 MSVC/Ninja）。"""
    if shutil.which("mingw32-make"):
        return "MinGW Makefiles"
    for p in (r"C:\mingw64\bin", r"C:\msys64\mingw64\bin"):
        if os.path.isfile(os.path.join(p, "mingw32-make.exe")):
            os.environ["PATH"] = p + os.pathsep + os.environ.get("PATH", "")
            return "MinGW Makefiles"
    return None


def run(cmd, cwd=None):
    """运行一条命令，输出直接透传到控制台，返回退出码。"""
    print("  > " + " ".join(cmd))
    return subprocess.call(cmd, cwd=cwd)


def run_pre_build(src, target):
    """运行 demo 的编译前脚本（若存在）。只运行第一个找到的。"""
    if sys.platform == "win32":
        order = ["pre_build.py", "pre_build.ps1"]
    else:
        order = ["pre_build.py", "pre_build.sh"]

    for name in order:
        script = os.path.join(src, name)
        if not os.path.isfile(script):
            continue
        print(f"[预构建] 运行 {target}/{name}")
        if name.endswith(".py"):
            cmd = [sys.executable, script]
        elif name.endswith(".ps1"):
            cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script]
        else:  # .sh
            cmd = ["bash", script]
        if run(cmd, cwd=src) != 0:
            sys.exit(f"[错误] 预构建脚本失败: {name}")
        return


def _setup_build_env():
    """把 TMP/TEMP 指到工作区内的 .tmp，并让子进程 Python 统一用 UTF-8 输出。

    编译器的临时文件因此不会落到系统临时目录，既自包含，
    也规避了某些环境下系统临时目录的 ACL 问题。
    """
    local_tmp = os.path.join(REPO_ROOT, ".tmp")
    os.makedirs(local_tmp, exist_ok=True)
    os.environ["TMP"] = local_tmp
    os.environ["TEMP"] = local_tmp
    os.environ["TMPDIR"] = local_tmp
    os.environ.setdefault("PYTHONIOENCODING", "utf-8")


def build(target, clean=False):
    _setup_build_env()
    src = os.path.join(DEMO_DIR, target)
    build_dir = os.path.join(src, "build")
    gen = pick_generator()

    if clean and os.path.isdir(build_dir):
        print(f"[清理] 删除 {os.path.relpath(build_dir, REPO_ROOT)}")
        shutil.rmtree(build_dir)

    print(f"\n==== 构建目标: {target} ====")
    print(f"[生成器] {gen or '自动选择'}")

    run_pre_build(src, target)

    configure = ["cmake", "-S", src, "-B", build_dir]
    if gen:
        configure += ["-G", gen]
    if run(configure) != 0:
        sys.exit("[错误] CMake 配置失败。")

    if run(["cmake", "--build", build_dir]) != 0:
        sys.exit("[错误] 构建失败。")

    print(f"\n==== 完成: {os.path.relpath(build_dir, REPO_ROOT)} ====")


# ---------------------------------------------------------------------------
# 键盘读取（纯标准库，跨平台）
# ---------------------------------------------------------------------------

def _enable_vt():
    """在 Windows 控制台启用 ANSI/VT 序列（清屏、反显高亮需要）。"""
    if sys.platform == "win32":
        try:
            import ctypes
            k = ctypes.windll.kernel32
            h = k.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
            mode = ctypes.c_uint32()
            if k.GetConsoleMode(h, ctypes.byref(mode)):
                k.SetConsoleMode(h, mode.value | 0x0004)  # ENABLE_VIRTUAL_TERMINAL_PROCESSING
        except Exception:
            pass


def read_key():
    """读取一个按键，返回语义化名称：up/down/enter/esc/q，或普通字符。"""
    if sys.platform == "win32":
        import msvcrt
        ch = msvcrt.getwch()
        if ch in ("\x00", "\xe0"):  # 扩展键（方向键等）的前导字节
            ch2 = msvcrt.getwch()
            return {"H": "up", "P": "down", "K": "left", "M": "right"}.get(ch2, ch2)
        if ch in ("\r", "\n"):
            return "enter"
        if ch == "\x1b":
            return "esc"
        if ch in ("q", "Q"):
            return "q"
        return ch
    else:
        import termios
        import tty
        fd = sys.stdin.fileno()
        old = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            ch = sys.stdin.read(1)
            if ch == "\x1b":
                seq = sys.stdin.read(2)
                if seq == "[A":
                    return "up"
                if seq == "[B":
                    return "down"
                if seq == "[C":
                    return "right"
                if seq == "[D":
                    return "left"
                return "esc"
            if ch in ("\r", "\n"):
                return "enter"
            if ch in ("q", "Q"):
                return "q"
            return ch
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)


def menu_select(names):
    """方向键菜单：返回选中的目标名；Esc/q 返回 None。"""
    _enable_vt()
    selected = 0
    n = len(names)

    while True:
        # 清屏 + 重绘（反显高亮当前项）
        lines = ["\x1b[2J\x1b[H", "可构建的目标:", ""]
        for i, name in enumerate(names):
            if i == selected:
                lines.append(f"  \x1b[7m> {i + 1}. {name}\x1b[0m")
            else:
                lines.append(f"    {i + 1}. {name}")
        lines += ["", "[↑/↓] 选择   [Enter] 构建   [Esc/Q] 退出"]
        print("\n".join(lines), flush=True)

        key = read_key()
        if key == "up":
            selected = (selected - 1) % n
        elif key == "down":
            selected = (selected + 1) % n
        elif key == "enter":
            return names[selected]
        elif key in ("esc", "q"):
            return None
        elif key in "123456789":  # 数字快速跳转
            idx = int(key) - 1
            if idx < n:
                selected = idx


def _number_fallback(names):
    """非交互（stdin 被重定向）时，回退到数字输入。"""
    print("\n可构建的目标:")
    for i, name in enumerate(names, 1):
        print(f"  {i}. {name}")
    print()
    try:
        choice = input(f"请输入编号 [1-{len(names)}]: ").strip()
    except (EOFError, KeyboardInterrupt):
        print()
        sys.exit(0)
    try:
        return names[int(choice) - 1]
    except (ValueError, IndexError):
        sys.exit("[错误] 无效选择。")


def main():
    if shutil.which("cmake") is None:
        sys.exit("[错误] 未找到 cmake，请先安装并加入 PATH。")

    targets = find_targets()
    if not targets:
        sys.exit("[错误] demo/ 下没有可构建的目标（缺少 CMakeLists.txt）。")

    parser = argparse.ArgumentParser(description="构建 demo/ 下的一个目标。")
    parser.add_argument("target", nargs="?", help="目标名；省略则进入交互菜单")
    parser.add_argument("--clean", action="store_true", help="先删除 build 目录再构建")
    args = parser.parse_args()

    names = [n for n, _ in targets]

    # 参数方式：build.py 0_sdl_setup
    if args.target:
        if args.target not in names:
            print(f"[错误] 未知目标: {args.target}")
            print("可用目标:")
            for i, n in enumerate(names, 1):
                print(f"  {i}. {n}")
            sys.exit(1)
        build(args.target, clean=args.clean)
        return

    # 交互菜单：有 tty 用方向键，否则回退到数字输入
    if sys.stdin.isatty():
        name = menu_select(names)
        if name is None:
            print()
            sys.exit(0)
    else:
        name = _number_fallback(names)

    build(name, clean=args.clean)


if __name__ == "__main__":
    main()
