#!/usr/bin/env python3
"""构建脚本：编译整个 src/ 框架，只有 debug / release 两个目标。

用法:
    build.bat                 # 默认构建 debug
    build.bat debug           # Debug（-g -O0，带调试符号）
    build.bat release         # Release（-O2 -DNDEBUG）
    build.bat --clean debug   # 先清理对应 build 目录再构建

构建产物分别输出到 build/debug/ 与 build/release/，可执行文件均为
tflm_framework.exe（SDL3.dll 由 CMake 自动拷贝到产物目录）。

由根目录 build.bat 调用（build.bat 负责找 Python 并切换控制台为 UTF-8）。
"""
import argparse
import os
import shutil
import subprocess
import sys

# Windows 控制台默认可能是 GBK，统一成 UTF-8；line_buffering 保证 print 与
# cmake 子进程输出顺序正确。
if sys.platform == "win32":
    for _s in (sys.stdout, sys.stderr):
        try:
            _s.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
        except (AttributeError, ValueError):
            pass

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGETS = ("debug", "release")


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


def _setup_build_env():
    """把 TMP/TEMP 指到工作区内的 .tmp，让子进程 Python 统一用 UTF-8 输出。"""
    local_tmp = os.path.join(REPO_ROOT, ".tmp")
    os.makedirs(local_tmp, exist_ok=True)
    os.environ["TMP"] = local_tmp
    os.environ["TEMP"] = local_tmp
    os.environ["TMPDIR"] = local_tmp
    os.environ.setdefault("PYTHONIOENCODING", "utf-8")


def build(target, clean=False):
    _setup_build_env()
    build_type = "Debug" if target == "debug" else "Release"
    build_dir = os.path.join(REPO_ROOT, "build", target)
    gen = pick_generator()

    if clean and os.path.isdir(build_dir):
        print(f"[清理] 删除 {os.path.relpath(build_dir, REPO_ROOT)}")
        shutil.rmtree(build_dir)

    print(f"\n==== 构建: {target} ({build_type}) ====")
    print(f"[生成器] {gen or '自动选择'}")

    configure = ["cmake", "-S", REPO_ROOT, "-B", build_dir, "-DCMAKE_BUILD_TYPE=" + build_type]
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

    parser = argparse.ArgumentParser(description="编译 src/ 框架（debug / release）。")
    parser.add_argument("target", nargs="?", default="debug", choices=TARGETS,
                        help="构建目标：debug（默认）或 release")
    parser.add_argument("--clean", action="store_true", help="先删除对应 build 目录再构建")
    args = parser.parse_args()

    build(args.target, clean=args.clean)


if __name__ == "__main__":
    main()
