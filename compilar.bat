@echo off
gcc driver.c -O2 -Wall -o FlappyBird.exe
if %errorlevel% equ 0 (
    echo Compilacion exitosa: FlappyBird.exe
) else (
    echo Error en la compilacion
)
pause
