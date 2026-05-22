@echo off
REM Self-running probe of every reachable MycoBrain. Double-click to launch.
REM Writes probe_com4.txt, probe_jet228.txt, probe_jet123.txt, probe_done.flag
REM into the repo root. After completion the .txt files are what Claude reads.
REM
REM The Jetson password is set inline below (rotation-pending per Morgan).
REM Delete this file after the probe if you want to clear it from disk.

setlocal
cd /d "%~dp0.."
set "MYCOBRAIN_COM_PORT=COM4"
set "MYCOBRAIN_JETSON_USER=jetson"
set "MYCOBRAIN_JETSON_PASSWORD=Loserology1!"

echo === MycoBrain auto-probe starting ===
echo Repo: %CD%
echo.

echo [1/3] Installing Python deps (pyserial, paramiko)...
python -m pip install --quiet pyserial paramiko
if errorlevel 1 (
  echo pip install failed. Make sure Python 3 + pip are on PATH.
  pause
  exit /b 1
)

echo [2/3] Running auto_probe_all.py...
python tools\python\auto_probe_all.py
if errorlevel 1 (
  echo auto_probe_all.py exited non-zero. Check probe_run.log
  pause
  exit /b 1
)

echo.
echo [3/3] Done. Output files in this folder:
dir /b probe_*.txt probe_done.flag
echo.
echo This window will close in 10s. (Or close it now — outputs are saved.)
timeout /t 10 >nul
endlocal
