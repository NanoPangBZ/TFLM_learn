#!/usr/bin/env python3
"""把二进制文件生成为可链接的 C 数组源文件。

用法:
    python embed_bin.py <input.bin> <output.c> <symbol_prefix>

生成:
    const unsigned char <symbol_prefix>[] = { ... };
    const unsigned int  <symbol_prefix>_size = N;
"""
from __future__ import annotations

import os
import sys


def main() -> int:
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2

    src, dst, prefix = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(src, "rb") as f:
        data = f.read()

    os.makedirs(os.path.dirname(os.path.abspath(dst)) or ".", exist_ok=True)
    with open(dst, "w", encoding="utf-8", newline="\n") as out:
        out.write(f"/* Auto-generated from {os.path.basename(src)} — do not edit. */\n")
        out.write(f"const unsigned char {prefix}[{len(data)}] = {{\n")
        for i in range(0, len(data), 16):
            chunk = data[i : i + 16]
            hexes = ", ".join(f"0x{b:02x}" for b in chunk)
            comma = "," if i + 16 < len(data) else ""
            out.write(f"    {hexes}{comma}\n")
        out.write("};\n")
        out.write(f"const unsigned int {prefix}_size = {len(data)}u;\n")

    print(f"  embedded {len(data)} bytes -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
