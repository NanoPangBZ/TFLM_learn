#!/usr/bin/env python3
"""Download third-party deps into third_party/ (not committed; called by setup).

    SDL3 / SDL3_ttf 源码 -> 编成静态库 third_party/sdl_static/（exe 无第三方 DLL）
    DejaVuSans           -> third_party/fonts/DejaVuSans.ttf

Stdlib only; on Windows uses Python's OpenSSL stack (avoids schannel TLS issues).
"""
import os
import platform
import shutil
import subprocess
import sys
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TP = os.path.join(ROOT, "third_party")


def download(url, dest):
    print(f"  downloading {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "tflm-learn-setup/1.0"})
    with urllib.request.urlopen(req, timeout=180) as r, open(dest, "wb") as f:
        shutil.copyfileobj(r, f)


def fetch_font():
    font_dir = os.path.join(TP, "fonts")
    os.makedirs(font_dir, exist_ok=True)
    ttf = os.path.join(font_dir, "DejaVuSans.ttf")
    if os.path.isfile(ttf):
        print("  DejaVuSans.ttf present, skip")
        return
    url = "https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37/dejavu-sans-ttf-2.37.zip"
    archive = os.path.join(TP, "dejavu-sans-ttf-2.37.zip")
    download(url, archive)
    with zipfile.ZipFile(archive) as z:
        for n in z.namelist():
            if n.endswith("ttf/DejaVuSans.ttf"):
                with z.open(n) as src, open(ttf, "wb") as dst:
                    shutil.copyfileobj(src, dst)
                break
    os.remove(archive)
    print(f"  DejaVuSans.ttf ready -> {ttf}")


def build_static_sdl():
    script = os.path.join(ROOT, "scripts", "build_sdl_static.py")
    rc = subprocess.call([sys.executable, script])
    if rc != 0:
        raise SystemExit(rc)


def main():
    os.makedirs(TP, exist_ok=True)
    if platform.system() != "Windows":
        print("On Linux/macOS, install SDL3 / SDL3_ttf via your package manager (see setup.sh).")
        return 0

    fetch_font()
    build_static_sdl()
    print("All dependencies ready.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
