@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /O2 /Ob2 /DNDEBUG /MD /EHsc /std:c++17 /I "C:\Users\HEQ\Projects\qupathxx\build\ci\scientific-grid-159\cpp" hash_probe.cpp "C:\Users\HEQ\Projects\qupathxx\build\ci\scientific-grid-159\cpp\sha256.cpp" bcrypt.lib /Fehash_probe.exe
