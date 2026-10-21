#!/usr/bin/env python3
"""交互式构建 helper：构建 demo/ 目录下的一个目标。

用法:
    python scripts/build.py            # 交互式菜单
    python scripts/build.py <目标名>    # 直接构建，如 scripts/build.py 0_sdl_setup

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


def run(cmd):
    """运行一条命令，输出直接透传到控制台，返回退出码。"""
    print("  > " + " ".join(cmd))
    return subprocess.call(cmd)


def build(target):
    src = os.path.join(DEMO_DIR, target)
    build_dir = os.path.join(src, "build")
    gen = pick_generator()

    print(f"\n==== 构建目标: {target} ====")
    print(f"[生成器] {gen or '自动选择'}")

    configure = ["cmake", "-S", src, "-B", build_dir]
    if gen:
        configure += ["-G", gen]
    if run(configure) != 0:
        sys.exit("[错误] CMake 配置失败。")

    if run(["cmake", "--build", build_dir]) != 0:
        sys.exit("[错误] 构建失败。")

    print(f"\n==== 完成: {os.path.relpath(build_dir, REPO_ROOT)} ====")


def main():
    if shutil.which("cmake") is None:
        sys.exit("[错误] 未找到 cmake，请先安装并加入 PATH。")

    targets = find_targets()
    if not targets:
        sys.exit("[错误] demo/ 下没有可构建的目标（缺少 CMakeLists.txt）。")

    parser = argparse.ArgumentParser(description="构建 demo/ 下的一个目标。")
    parser.add_argument("target", nargs="?", help="目标名；省略则进入交互菜单")
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
        build(args.target)
        return

    # 交互式菜单
    print("\n可构建的目标:")
    for i, n in enumerate(names, 1):
        print(f"  {i}. {n}")
    print()
    try:
        choice = input(f"请输入编号 [1-{len(names)}]: ").strip()
    except (EOFError, KeyboardInterrupt):
        print()
        sys.exit(0)

    try:
        idx = int(choice)
        name = names[idx - 1]
    except (ValueError, IndexError):
        sys.exit("[错误] 无效选择。")

    build(name)


if __name__ == "__main__":
    main()
