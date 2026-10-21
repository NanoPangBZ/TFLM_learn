#!/usr/bin/env python3
"""示例「编译前脚本」：build.py 在 cmake 之前自动运行本文件（若存在）。

真实项目里，这一步通常做：
    训练模型 -> 导出 .tflite -> 转成 C 头文件数组（.h），供 C++ demo 编译进去。

本 demo（0_sdl_setup）没有模型，所以这里只演示钩子被调用。
去掉本文件即可禁用预构建步骤。
"""
import datetime
import os

print(f"[pre_build] {os.path.basename(os.getcwd())} 无需预处理（示例钩子），"
      f"时间 {datetime.datetime.now().isoformat(timespec='seconds')}")
