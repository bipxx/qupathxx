@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /O2 /Ob2 /DNDEBUG /MD /EHsc /std:c++17 /fp:strict /I include /c "C:\Users\HEQ\Projects\qupathxx\build\ci\scientific-grid-159\cpp\imagej_gaussian.cpp" /Foimagej.obj
if errorlevel 1 exit /b %errorlevel%
cl /nologo /O2 /Ob2 /DNDEBUG /MD /EHsc /std:c++17 /I include nucleus_probe.cpp imagej.obj /Fenucleus_probe.exe
