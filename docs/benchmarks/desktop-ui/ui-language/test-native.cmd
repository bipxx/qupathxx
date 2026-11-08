@echo off
call "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul
cl /nologo /std:c++17 /utf-8 /EHsc /MD /O2 cpp/tests/gui_i18n_tests.cpp /Fo"build\ui-language\i18n-tests.obj" /Fe"build\ui-language\i18n-tests.exe"
if errorlevel 1 exit /b %errorlevel%
"build\ui-language\i18n-tests.exe"
