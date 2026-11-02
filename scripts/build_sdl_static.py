#!/usr/bin/env python3
"""从源码编译 SDL3 / SDL3_ttf 静态库，安装到 third_party/sdl_static/。

官方 mingw devel 包只有 DLL；要得到可无第三方 DLL 运行的 exe，需要静态库。
本脚本由 fetch_deps.py 调用；已安装则跳过。

产物：
    third_party/sdl_static/lib/libSDL3.a
    third_party/sdl_static/lib/libSDL3_ttf.a
    third_party/sdl_static/lib/cmake/SDL3/
    third_party/sdl_static/lib/cmake/SDL3_ttf/
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

SDL3_VERSION = "3.4.18"
SDL3_TTF_VERSION = "3.2.2"

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TP = os.path.join(ROOT, "third_party")
SRC = os.path.join(TP, "_src")
PREFIX = os.path.join(TP, "sdl_static")
BUILD = os.path.join(TP, "_build_static")


def download(url: str, dest: str) -> None:
    print(f"  downloading {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "tflm-learn-setup/1.0"})
    with urllib.request.urlopen(req, timeout=300) as r, open(dest, "wb") as f:
        shutil.copyfileobj(r, f)


def extract_archive(archive: str, dest_dir: str) -> str:
    """解压到 dest_dir，返回解压出的顶层目录路径。"""
    os.makedirs(dest_dir, exist_ok=True)
    before = set(os.listdir(dest_dir))
    if archive.endswith(".zip"):
        with zipfile.ZipFile(archive) as z:
            z.extractall(dest_dir)
    else:
        with tarfile.open(archive, "r:*") as t:
            t.extractall(dest_dir)
    after = set(os.listdir(dest_dir)) - before
    if len(after) == 1:
        return os.path.join(dest_dir, after.pop())
    # 已有同名目录的情况：按版本名猜测
    for name in os.listdir(dest_dir):
        path = os.path.join(dest_dir, name)
        if os.path.isdir(path) and ("SDL3" in name or "SDL_ttf" in name or "SDL3_ttf" in name):
            return path
    raise RuntimeError(f"cannot locate extracted root in {dest_dir}")


def ensure_source(name: str, version: str, repo: str, asset: str) -> str:
    """下载并解压源码，返回源码目录。"""
    expected = os.path.join(SRC, f"{name}-{version}")
    if os.path.isdir(expected):
        print(f"  {name} source present, skip download")
        return expected

    # GitHub release 源码包命名：SDL-3.x.y / SDL3-3.x.y / SDL_ttf-...
    url = f"https://github.com/libsdl-org/{repo}/releases/download/release-{version}/{asset}"
    archive = os.path.join(TP, asset)
    if not os.path.isfile(archive):
        download(url, archive)
    os.makedirs(SRC, exist_ok=True)
    root = extract_archive(archive, SRC)
    # 统一成 name-version
    if os.path.abspath(root) != os.path.abspath(expected):
        if os.path.isdir(expected):
            shutil.rmtree(expected)
        os.replace(root, expected)
    try:
        os.remove(archive)
    except OSError:
        pass
    print(f"  {name} source -> {expected}")
    return expected


def run(cmd: list[str], cwd: str | None = None) -> None:
    print("  > " + " ".join(cmd))
    rc = subprocess.call(cmd, cwd=cwd)
    if rc != 0:
        raise RuntimeError(f"command failed ({rc}): {' '.join(cmd)}")


def find_cmake() -> str:
    cmake = shutil.which("cmake")
    if cmake:
        return cmake
    raise RuntimeError("cmake not found in PATH")


def find_generator() -> str:
    if shutil.which("mingw32-make"):
        return "MinGW Makefiles"
    for p in (r"C:\mingw64\bin", r"C:\msys64\mingw64\bin"):
        if os.path.isfile(os.path.join(p, "mingw32-make.exe")):
            os.environ["PATH"] = p + os.pathsep + os.environ.get("PATH", "")
            return "MinGW Makefiles"
    raise RuntimeError("mingw32-make not found; need MinGW-w64 to build static SDL")


def has_sdl3() -> bool:
    return os.path.isfile(os.path.join(PREFIX, "lib", "libSDL3.a")) and os.path.isfile(
        os.path.join(PREFIX, "lib", "cmake", "SDL3", "SDL3Config.cmake")
    )


def has_sdl3_ttf() -> bool:
    return os.path.isfile(os.path.join(PREFIX, "lib", "libSDL3_ttf.a")) and os.path.isfile(
        os.path.join(PREFIX, "lib", "cmake", "SDL3_ttf", "SDL3_ttfConfig.cmake")
    )


def static_ready() -> bool:
    return has_sdl3() and has_sdl3_ttf()


def ensure_freetype(ttf_src: str) -> None:
    """SDL3_ttf 官方源码包不含 submodule；需要拉取 vendored freetype。"""
    ft = os.path.join(ttf_src, "external", "freetype")
    if os.path.isdir(os.path.join(ft, "include")):
        print("  freetype vendored present, skip")
        return
    if not shutil.which("git"):
        raise RuntimeError("git not found; needed to clone SDL3_ttf/external/freetype")
    os.makedirs(os.path.join(ttf_src, "external"), exist_ok=True)
    if os.path.isdir(ft):
        shutil.rmtree(ft)
    run([
        "git", "clone", "--filter=blob:none", "--depth", "1",
        "-b", "VER-2-13-2-SDL",
        "https://github.com/libsdl-org/freetype.git",
        ft,
    ])


def build_sdl3(src: str, cmake: str, gen: str) -> None:
    bdir = os.path.join(BUILD, "SDL3")
    if os.path.isdir(bdir):
        shutil.rmtree(bdir)
    os.makedirs(bdir, exist_ok=True)
    run([
        cmake, "-S", src, "-B", bdir,
        "-G", gen,
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={PREFIX}",
        "-DSDL_SHARED=OFF",
        "-DSDL_STATIC=ON",
        "-DSDL_TEST=OFF",
        "-DSDL_TESTS=OFF",
        "-DSDL_EXAMPLES=OFF",
        "-DSDL_INSTALL_CMAKEDIR=lib/cmake/SDL3",
    ])
    run([cmake, "--build", bdir, "--parallel"])
    run([cmake, "--install", bdir])


def build_sdl3_ttf(src: str, cmake: str, gen: str) -> None:
    ensure_freetype(src)
    bdir = os.path.join(BUILD, "SDL3_ttf")
    if os.path.isdir(bdir):
        shutil.rmtree(bdir)
    os.makedirs(bdir, exist_ok=True)
    run([
        cmake, "-S", src, "-B", bdir,
        "-G", gen,
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={PREFIX}",
        f"-DCMAKE_PREFIX_PATH={PREFIX}",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DSDLTTF_VENDORED=ON",
        "-DSDLTTF_HARFBUZZ=OFF",
        "-DSDLTTF_PLUTOSVG=OFF",
        "-DSDLTTF_SAMPLES=OFF",
        "-DSDLTTF_INSTALL_CMAKEDIR=lib/cmake/SDL3_ttf",
    ])
    run([cmake, "--build", bdir, "--parallel"])
    run([cmake, "--install", bdir])


def main() -> int:
    if sys.platform != "win32":
        print("Static SDL build helper is Windows/MinGW oriented; skip on this OS.")
        return 0

    if static_ready():
        print("  sdl_static present, skip build")
        return 0

    print("==> Building static SDL3 / SDL3_ttf (one-time, may take a few minutes)")
    cmake = find_cmake()
    gen = find_generator()
    os.makedirs(TP, exist_ok=True)
    os.makedirs(PREFIX, exist_ok=True)

    if not has_sdl3():
        sdl_src = ensure_source(
            "SDL3", SDL3_VERSION, "SDL",
            f"SDL3-{SDL3_VERSION}.tar.gz",
        )
        build_sdl3(sdl_src, cmake, gen)

    if not has_sdl3_ttf():
        ttf_asset = f"SDL3_ttf-{SDL3_TTF_VERSION}.tar.gz"
        try:
            ttf_src = ensure_source("SDL3_ttf", SDL3_TTF_VERSION, "SDL_ttf", ttf_asset)
        except Exception:
            ttf_src = ensure_source(
                "SDL_ttf", SDL3_TTF_VERSION, "SDL_ttf",
                f"SDL_ttf-{SDL3_TTF_VERSION}.tar.gz",
            )
        build_sdl3_ttf(ttf_src, cmake, gen)

    if not static_ready():
        print("ERROR: static libraries missing after install", file=sys.stderr)
        return 1

    if os.path.isdir(BUILD):
        shutil.rmtree(BUILD, ignore_errors=True)

    print(f"  static SDL ready -> {PREFIX}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)
