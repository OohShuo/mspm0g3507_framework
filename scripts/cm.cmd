@echo off
REM Windows wrapper for cc.py — use `python` because `python3` may be a Store stub.
python "%~dp0cc.py" %*
