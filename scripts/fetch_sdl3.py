#!/usr/bin/env python3
"""Download and extract the SDL3 development package into third_party/.

Self-contained (Python stdlib only) so it runs on Windows, Linux and macOS
without touching the system: everything lands under third_party/SDL3.

Why a Python downloader instead of curl/Invoke-WebRequest? Python ships its
own OpenSSL stack, so this also works inside restricted shells where the
Windows schannel TLS store is unavailable.
"""
import os
import platform
import shutil
import sys
import tarfile
import urllib.request
import zipfile

SDL3_VERSION = "3.4.18"


def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def download(url, dest):
    print(f"  downloading {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "tflm-learn-setup/1.0"})
    with urllib.request.urlopen(req, timeout=180) as resp, open(dest, "wb") as f:
        shutil.copyfileobj(resp, f)
    return dest


def extract(archive, dest):
    print(f"  extracting -> {dest}")
    if archive.endswith(".zip"):
        with zipfile.ZipFile(archive) as z:
            z.extractall(dest)
    elif archive.endswith((".tar.gz", ".tgz")):
        with tarfile.open(archive) as t:
            t.extractall(dest, filter="data")
    else:
        raise SystemExit(f"unsupported archive format: {archive}")


def main():
    system = platform.system()
    third_party = os.path.join(repo_root(), "third_party")
    os.makedirs(third_party, exist_ok=True)
    base = (
        f"https://github.com/libsdl-org/SDL/releases/download/"
        f"release-{SDL3_VERSION}"
    )

    if system == "Windows":
        asset = f"SDL3-devel-{SDL3_VERSION}-mingw.zip"
    else:
        print("On Linux/macOS, install SDL3 with your package manager:")
        print("  sudo apt install libsdl3-dev    # Debian/Ubuntu")
        print("  brew install sdl3               # macOS")
        print("(this script only auto-vendors SDL3 on Windows)")
        return 0

    archive = os.path.join(third_party, asset)
    download(base + "/" + asset, archive)
    extract(archive, third_party)
    os.remove(archive)

    # The archive unpacks to SDL3-<version>/; normalize to SDL3/ for stable paths.
    src = os.path.join(third_party, f"SDL3-{SDL3_VERSION}")
    dst = os.path.join(third_party, "SDL3")
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    os.replace(src, dst)
    print(f"Done. SDL3 {SDL3_VERSION} is now under third_party/SDL3/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
