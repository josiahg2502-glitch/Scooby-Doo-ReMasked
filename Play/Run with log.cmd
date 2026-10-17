@echo off
cd /d "%~dp0"
echo Starting... > run.log
"Scooby-Doo Re-Masked.exe" >> run.log 2>&1
echo exit code %errorlevel% >> run.log
type run.log
pause
