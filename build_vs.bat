@echo off
REM 临时编译脚本：用 VS2022 开发者环境变量，再调用 build.bat
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cd /d d:\Cpoject\c_web
call build.bat
exit /b %errorlevel%
