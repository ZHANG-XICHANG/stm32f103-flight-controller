@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo
if errorlevel 1 exit /b %errorlevel%
if not exist build\pid-tests mkdir build\pid-tests
cl /nologo /utf-8 /std:c11 /W4 /Itests/telemetry/stubs /ICore/Inc Core/Src/pid_command.c tests/pid_command/flight_test.c /Febuild/pid-tests/flight.exe /Fobuild/pid-tests/
if errorlevel 1 exit /b %errorlevel%
build\pid-tests\flight.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /utf-8 /std:c11 /W4 /Itests/pid_command/stubs /IZ:/Darcy/STM32/F411_remote_hal/Core/Inc Z:/Darcy/STM32/F411_remote_hal/Core/Src/pid_command.c tests/pid_command/remote_test.c /Febuild/pid-tests/remote.exe /Fobuild/pid-tests/
if errorlevel 1 exit /b %errorlevel%
build\pid-tests\remote.exe

