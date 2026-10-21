#!/usr/bin/env python3
"""Download third-party deps into third_party/ (not committed; called by setup).

    SDL3       -> third_party/SDL3/
    SDL3_ttf   -> third_party/SDL3_ttf/   (statically bundles FreeType)
    DejaVuSans -> third_party/fonts/DejaVuSans.ttf

Stdlib only; on Windows uses Python's OpenSSL stack (avoids schannel TLS issues).
"""
import os
import platform
import shutil
import sys
import urllib.request
import zipfile

SDL3_VERSION = "3.4.18"
SDL3_TTF_VERSION = "3.2.2"

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TP = os.path.join(ROOT, "third_party")


def download(url, dest):
    print(f"  downloading {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "tflm-learn-setup/1.0"})
    with urllib.request.urlopen(req, timeout=180) as r, open(dest, "wb") as f:
        shutil.copyfileobj(r, f)


def fetch_mingw(name, version, repo, asset):
    dest = os.path.join(TP, name)
    if os.path.isdir(dest):
        print(f"  {name} present, skip")
        return
    url = f"https://github.com/libsdl-org/{repo}/releases/download/release-{version}/{asset}"
    archive = os.path.join(TP, asset)
    download(url, archive)
    with zipfile.ZipFile(archive) as z:
        z.extractall(TP)
    os.remove(archive)
    src = os.path.join(TP, f"{name}-{version}")
    if os.path.isdir(src):
        os.replace(src, dest)
    print(f"  {name} ready -> {dest}")


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


def main():
    os.makedirs(TP, exist_ok=True)
    if platform.system() != "Windows":
        print("On Linux/macOS, install SDL3 / SDL3_ttf via your package manager (see setup.sh).")
        return 0

    fetch_mingw("SDL3", SDL3_VERSION, "SDL", f"SDL3-devel-{SDL3_VERSION}-mingw.zip")
    fetch_mingw("SDL3_ttf", SDL3_TTF_VERSION, "SDL_ttf", f"SDL3_ttf-devel-{SDL3_TTF_VERSION}-mingw.zip")
    fetch_font()
    print("All dependencies ready.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
