@echo off
chcp 65001 >nul
cd /d "%~dp0"

rem ============================================================
rem  build.bat - thin wrapper. Delegates to scripts/build.py
rem  (pure-ASCII so the batch parser never chokes on encoding).
rem
rem  Usage:
rem    build.bat            interactive menu to pick a target
rem    build.bat <target>   build directly, e.g. build.bat 0_sdl_setup
rem ============================================================

rem --- locate a Python: prefer the repo's .venv, else any python on PATH ---
set "PY="
if exist ".venv\Scripts\python.exe" set "PY=.venv\Scripts\python.exe"
if not defined PY (
    where python >nul 2>nul && set "PY=python"
)
if not defined PY (
    echo [ERROR] Python not found. Run setup.ps1 first, or install Python 3.10-3.13.
    exit /b 1
)

"%PY%" "scripts\build.py" %*
exit /b %errorlevel%
