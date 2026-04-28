@echo off
cd backend
gcc scheduler.c -o scheduler.exe
scheduler.exe input.json output.json
type output.json
pause
