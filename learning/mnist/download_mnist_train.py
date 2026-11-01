#!/usr/bin/env python3
"""下载 MNIST 训练集（只下载，不训练、不解压）。

默认下载到仓库内、但已被 .gitignore 忽略的目录：

    learning/mnist/data

只下载训练集的两个文件：

    train-images-idx3-ubyte.gz
    train-labels-idx1-ubyte.gz

带进度条、MD5 校验；已存在且校验通过则跳过。
目标目录如果在仓库内，必须被 .gitignore 忽略，否则脚本拒绝执行，确保下载内容不会被 git 追踪。

用法：

    .venv\\Scripts\\python.exe learning\\mnist\\download_mnist_train.py
    .venv\\Scripts\\python.exe learning\\mnist\\download_mnist_train.py --force
    .venv\\Scripts\\python.exe learning\\mnist\\download_mnist_train.py --dest D:\\data\\mnist
"""
from __future__ import annotations

import argparse
import hashlib
import os
import subprocess
import sys
import time
from pathlib import Path
from urllib.error import URLError
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[2]          # learning/mnist -> repo root
DEFAULT_DEST = Path(__file__).resolve().parent / "data"
BASE_URL = "https://storage.googleapis.com/cvdf-datasets/mnist"

FILES = {
    "train-images-idx3-ubyte.gz": "f68b3c2dcbeaaa9fbdd348bbdeb94873",
    "train-labels-idx1-ubyte.gz": "d53e105ee54ea40749a09fcbcd1e9432",
}

CHUNK = 64 * 1024


def human(n: float) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} GB"


def md5_file(path: Path) -> str:
    h = hashlib.md5()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def is_git_ignored(path: Path) -> bool:
    """用 git check-ignore 判断目标文件是否会被忽略。"""
    try:
        result = subprocess.run(
            ["git", "check-ignore", "-q", str(path)],
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return result.returncode == 0
    except FileNotFoundError:
        return False


def draw_progress(done: int, total: int, started: float, width: int = 30) -> None:
    elapsed = max(time.monotonic() - started, 1e-6)
    speed = done / elapsed
    if total > 0:
        frac = min(done / total, 1.0)
        filled = int(width * frac)
        bar = "#" * filled + "-" * (width - filled)
        pct = f"{frac * 100:5.1f}%"
        total_text = human(total)
    else:
        bar = "?" * width
        pct = "  ?  "
        total_text = "?"
    sys.stdout.write(
        f"\r  [{bar}] {pct}  {human(done)}/{total_text}  {human(speed)}/s"
    )
    sys.stdout.flush()


def download(url: str, dest: Path, expected_md5: str, force: bool) -> None:
    if dest.exists() and not force:
        got = md5_file(dest)
        if got == expected_md5:
            print(f"  skip (MD5 ok): {dest.name} ({human(dest.stat().st_size)})")
            return
        print(f"  MD5 mismatch, redownload: {dest.name}")

    part = dest.with_name(dest.name + ".part")
    req = Request(url, headers={"User-Agent": "tflm-learn-mnist-download/1.0"})
    with urlopen(req, timeout=60) as resp, part.open("wb") as f:
        total = int(resp.headers.get("Content-Length") or 0)
        done = 0
        started = time.monotonic()
        while True:
            chunk = resp.read(CHUNK)
            if not chunk:
                break
            f.write(chunk)
            done += len(chunk)
            draw_progress(done, total, started)
    print()

    got = md5_file(part)
    if got != expected_md5:
        part.unlink(missing_ok=True)
        raise RuntimeError(
            f"MD5 mismatch for {dest.name}: got {got}, expected {expected_md5}"
        )

    os.replace(part, dest)
    print(f"  saved: {dest} ({human(dest.stat().st_size)})")


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
            sys.stderr.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass

    parser = argparse.ArgumentParser(
        description="下载 MNIST 训练集（只下载，不解压）。仓库内目标必须被 .gitignore 忽略。"
    )
    parser.add_argument(
        "--dest",
        type=Path,
        default=DEFAULT_DEST,
        help=f"下载目录（默认：{DEFAULT_DEST}，已被 .gitignore 忽略）",
    )
    parser.add_argument("--force", action="store_true", help="已存在也重新下载")
    args = parser.parse_args()

    dest = args.dest.expanduser().resolve()
    probe = dest / "train-images-idx3-ubyte.gz"
    if (dest == ROOT or ROOT in dest.parents) and not is_git_ignored(probe):
        parser.error(
            "目标目录在仓库内且未被 .gitignore 忽略，会污染 git 状态；"
            f"请换到已忽略的目录，例如 {DEFAULT_DEST}"
        )

    dest.mkdir(parents=True, exist_ok=True)

    print("MNIST training set download")
    print(f"  repo : {ROOT}")
    print(f"  dest : {dest}")
    print("  files: train images + train labels (no test set, no decompress)")

    for name, expected in FILES.items():
        print(f"\n{name}")
        try:
            download(f"{BASE_URL}/{name}", dest / name, expected, args.force)
        except (URLError, TimeoutError, RuntimeError) as e:
            print(f"\n[error] {e}", file=sys.stderr)
            return 1

    print("\nDone. 训练集已下载到被 .gitignore 忽略的目录，git 不会追踪。")
    print("下一步：用 numpy 读取 IDX，或运行训练脚本。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
