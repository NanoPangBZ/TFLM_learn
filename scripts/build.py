#!/usr/bin/env python3
"""构建脚本：按 target + debug/release 编译框架。

用法:
    build.bat                              # 交互菜单（方向键）选 target 与配置
    build.bat TFLM_LEARN debug             # 直接构建
    build.bat platform_develop release
    build.bat --clean TFLM_LEARN debug     # 先清理再构建
    build.bat --sync-vscode                # 扫描 src/target，同步 .vscode 的 target 列表
    build.bat --list-targets               # 打印可用 target（一行一个）

产物输出到 build/<target>/<debug|release>/，可执行文件名与目标名一致
（如 TFLM_LEARN.exe；SDL 静态链接，无需拷贝 SDL3.dll）。

由根目录 build.bat 调用（build.bat 负责找 Python 并切换控制台为 UTF-8）。
交互菜单全部在本 Python 脚本内实现（非 bat）。
"""
import argparse
import json
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
TARGET_DIR = os.path.join(REPO_ROOT, "src", "target")
BUILD_CONFIGS = ("debug", "release")


def discover_board_targets():
    """扫描 src/target/ 下含 CMakeLists.txt 的子目录，作为编译目标列表。"""
    if not os.path.isdir(TARGET_DIR):
        sys.exit(f"[错误] 未找到 target 目录: {os.path.relpath(TARGET_DIR, REPO_ROOT)}")
    names = []
    for name in sorted(os.listdir(TARGET_DIR)):
        path = os.path.join(TARGET_DIR, name)
        if os.path.isdir(path) and os.path.isfile(os.path.join(path, "CMakeLists.txt")):
            names.append(name)
    if not names:
        sys.exit(f"[错误] {os.path.relpath(TARGET_DIR, REPO_ROOT)} 下没有可用目标"
                 "（需要含 CMakeLists.txt 的子目录）。")
    return names


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


def _enable_windows_ansi():
    """尽量打开 Windows 控制台 VT 处理，使 ANSI 光标移动可用。"""
    if sys.platform != "win32":
        return
    try:
        import ctypes
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
        mode = ctypes.c_uint32()
        if kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
            ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
            kernel32.SetConsoleMode(handle, mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
    except Exception:
        pass


def _read_key():
    """读一个逻辑按键：up/down/enter/esc 或普通字符。"""
    if sys.platform == "win32":
        import msvcrt
        ch = msvcrt.getwch()
        if ch in ("\x00", "\xe0"):
            ch2 = msvcrt.getwch()
            return {"H": "up", "P": "down"}.get(ch2, ch2)
        if ch in ("\r", "\n"):
            return "enter"
        if ch == "\x1b":
            return "esc"
        if ch == "\x03":
            raise KeyboardInterrupt
        return ch

    import termios
    import tty
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        ch = sys.stdin.read(1)
        if ch == "\x1b":
            rest = sys.stdin.read(2)
            if rest == "[A":
                return "up"
            if rest == "[B":
                return "down"
            return "esc"
        if ch in ("\r", "\n"):
            return "enter"
        if ch == "\x03":
            raise KeyboardInterrupt
        return ch
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)


def _prompt_choice(title, options):
    """方向键选择一项（↑↓ + Enter；Esc 取消）。非 TTY 时回退到数字输入。"""
    if not options:
        sys.exit("[错误] 无可选项。")

    if not sys.stdin.isatty() or not sys.stdout.isatty():
        print(f"\n{title}")
        for i, opt in enumerate(options, 1):
            print(f"  {i}. {opt}")
        while True:
            raw = input(f"请选择 [1-{len(options)}]: ").strip()
            if raw.isdigit():
                idx = int(raw)
                if 1 <= idx <= len(options):
                    return options[idx - 1]
            print("无效输入，请重试。")

    _enable_windows_ansi()
    idx = 0
    hint = "↑↓ 选择，Enter 确认，Esc 取消"
    print(f"\n{title}  ({hint})")
    # 先画出完整菜单，之后用 ANSI 回退重绘
    for i, opt in enumerate(options):
        mark = ">" if i == idx else " "
        print(f"  {mark} {opt}")

    try:
        while True:
            key = _read_key()
            if key == "up":
                idx = (idx - 1) % len(options)
            elif key == "down":
                idx = (idx + 1) % len(options)
            elif key == "enter":
                # 光标留在菜单下方，打印确认行
                print(f"\n已选择: {options[idx]}")
                return options[idx]
            elif key == "esc":
                print("\n已取消。")
                sys.exit(1)
            else:
                continue

            # 回到菜单首行重绘
            sys.stdout.write(f"\033[{len(options)}A")
            for i, opt in enumerate(options):
                mark = ">" if i == idx else " "
                line = f"  {mark} {opt}"
                sys.stdout.write("\033[2K" + line + "\n")
            sys.stdout.flush()
    except KeyboardInterrupt:
        print("\n已取消。")
        sys.exit(1)


def _vscode_input_block(targets):
    return {
        "id": "tflmTarget",
        "type": "pickString",
        "description": "选择编译目标 (src/target)",
        "options": list(targets),
        "default": targets[0],
    }


def _vscode_shell_task(label, bat_args, is_default_build=False):
    task = {
        "label": label,
        "type": "shell",
        "command": "cmd",
        "args": ["/c", "build.bat", *bat_args],
        "presentation": {
            "reveal": "always",
            "panel": "shared",
            "focus": False,
        },
        "problemMatcher": [],
    }
    if is_default_build:
        task["group"] = {"kind": "build", "isDefault": True}
    elif label.startswith("build "):
        task["group"] = "build"
    return task


def _write_json(path, data):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=4, ensure_ascii=False)
        f.write("\n")


def _build_tasks_json(targets):
    """生成 tasks.json：通用 pick 任务 + 每 target 的 debug 任务（供 F5 preLaunchTask）。"""
    tasks = [
        _vscode_shell_task(
            "build (debug)", ["${input:tflmTarget}", "debug"], is_default_build=True
        ),
        _vscode_shell_task("build (release)", ["${input:tflmTarget}", "release"]),
        _vscode_shell_task("clean (debug)", ["--clean", "${input:tflmTarget}", "debug"]),
    ]
    for name in targets:
        tasks.append(_vscode_shell_task(f"build {name} (debug)", [name, "debug"]))
    tasks.append(_vscode_shell_task("sync vscode targets", ["--sync-vscode"]))
    return {
        "version": "2.0.0",
        "tasks": tasks,
        "inputs": [_vscode_input_block(targets)],
    }


def _build_launch_json(targets):
    """生成 launch.json：F5 只弹一次 QuickPick，再匹配 build <target> (debug)。"""
    return {
        "version": "0.2.0",
        "configurations": [
            {
                "name": "Debug (gdb)",
                "type": "cppdbg",
                "request": "launch",
                "program": (
                    "${workspaceFolder}/build/${input:tflmTarget}/debug/${input:tflmTarget}.exe"
                ),
                "args": [],
                "stopAtEntry": False,
                "cwd": "${workspaceFolder}",
                "environment": [],
                "externalConsole": False,
                "MIMode": "gdb",
                "miDebuggerPath": "C:/mingw64/bin/gdb.exe",
                "preLaunchTask": "build ${input:tflmTarget} (debug)",
                "setupCommands": [
                    {
                        "description": "Enable pretty-printing for gdb",
                        "text": "-enable-pretty-printing",
                        "ignoreFailures": True,
                    }
                ],
            }
        ],
        "inputs": [_vscode_input_block(targets)],
    }


def sync_vscode_targets(targets, quiet=False):
    """扫描结果写入 .vscode/tasks.json 与 launch.json（options + 每 target 构建任务）。"""
    vscode_dir = os.path.join(REPO_ROOT, ".vscode")
    os.makedirs(vscode_dir, exist_ok=True)
    files = {
        "tasks.json": _build_tasks_json(targets),
        "launch.json": _build_launch_json(targets),
    }
    updated = []
    for name, data in files.items():
        path = os.path.join(vscode_dir, name)
        old = None
        if os.path.isfile(path):
            with open(path, "r", encoding="utf-8") as f:
                old = f.read()
        new = json.dumps(data, indent=4, ensure_ascii=False) + "\n"
        if old != new:
            _write_json(path, data)
            updated.append(os.path.relpath(path, REPO_ROOT))
    if not quiet:
        if updated:
            print("[sync-vscode] 已更新: " + ", ".join(updated))
        else:
            print("[sync-vscode] 已是最新")
        print("[sync-vscode] targets = " + ", ".join(targets))
    return updated


def resolve_args(argv, board_targets):
    """解析 CLI；缺省时进入交互菜单。返回 (board, config, clean) 或特殊动作。"""
    parser = argparse.ArgumentParser(
        description="编译框架：指定 target（src/target 下目录）与 debug|release。"
    )
    parser.add_argument(
        "board",
        nargs="?",
        choices=board_targets,
        help="编译目标（扫描自 src/target/<name>）",
    )
    parser.add_argument(
        "config",
        nargs="?",
        choices=BUILD_CONFIGS,
        help="构建配置：debug 或 release",
    )
    parser.add_argument("--clean", action="store_true", help="先删除对应 build 目录再构建")
    parser.add_argument(
        "--sync-vscode",
        action="store_true",
        help="扫描 src/target，同步 .vscode 里 F5/任务的 target 下拉列表",
    )
    parser.add_argument(
        "--list-targets",
        action="store_true",
        help="打印可用 target（一行一个）后退出",
    )
    args = parser.parse_args(argv)

    if args.list_targets:
        for name in board_targets:
            print(name)
        return None

    if args.sync_vscode:
        sync_vscode_targets(board_targets)
        return None

    board = args.board
    config = args.config
    if board is None or config is None:
        if board is None:
            board = _prompt_choice("选择编译目标 (TFLM_TARGET):", board_targets)
        if config is None:
            config = _prompt_choice("选择构建配置:", list(BUILD_CONFIGS))
    return board, config, args.clean


def build(board, config, clean=False):
    _setup_build_env()
    build_type = "Debug" if config == "debug" else "Release"
    build_dir = os.path.join(REPO_ROOT, "build", board, config)
    gen = pick_generator()

    if clean and os.path.isdir(build_dir):
        print(f"[清理] 删除 {os.path.relpath(build_dir, REPO_ROOT)}")
        shutil.rmtree(build_dir)

    print(f"\n==== 构建: {board} / {config} ({build_type}) ====")
    print(f"[生成器] {gen or '自动选择'}")

    configure = [
        "cmake", "-S", REPO_ROOT, "-B", build_dir,
        "-DCMAKE_BUILD_TYPE=" + build_type,
        "-DTFLM_TARGET=" + board,
    ]
    if gen:
        configure += ["-G", gen]
    if run(configure) != 0:
        sys.exit("[错误] CMake 配置失败。")

    if run(["cmake", "--build", build_dir]) != 0:
        sys.exit("[错误] 构建失败。")

    print(f"\n==== 完成: {os.path.relpath(build_dir, REPO_ROOT)} ====")


def main():
    board_targets = discover_board_targets()
    # 构建前静默同步 F5 下拉，避免新增 target 后 VS Code 列表过期
    sync_vscode_targets(board_targets, quiet=True)

    resolved = resolve_args(sys.argv[1:], board_targets)
    if resolved is None:
        return

    if shutil.which("cmake") is None:
        sys.exit("[错误] 未找到 cmake，请先安装并加入 PATH。")

    board, config, clean = resolved
    build(board, config, clean=clean)


if __name__ == "__main__":
    main()
