@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo
if errorlevel 1 exit /b %errorlevel%
if not exist build\telemetry-tests mkdir build\telemetry-tests
cl /nologo /utf-8 /std:c11 /W4 /Itests/telemetry/stubs /ICore/Inc Core/Src/telemetry.c tests/telemetry/test_telemetry.c /Febuild/telemetry-tests/test_telemetry.exe /Fobuild/telemetry-tests/
if errorlevel 1 exit /b %errorlevel%
build\telemetry-tests\test_telemetry.exe
