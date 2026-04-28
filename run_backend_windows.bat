@echo off
cd backend
gcc scheduler.c -o scheduler.exe
cd ../server
pip install -r requirements.txt
python app.py
pause
