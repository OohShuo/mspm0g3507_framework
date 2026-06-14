@echo off
REM Windows wrapper for cm.py — uses `python` since `python3` is a Store stub.
python "%~dp0cm.py" %*
