# PID numerical tests

Run in a Visual Studio x64 Native Tools command prompt from the project root
(use a mapped drive or `pushd` for UNC workspaces):

```bat
if not exist build\pid-tests mkdir build\pid-tests
cl /nologo /utf-8 /std:c11 /W4 /ICore/Inc Core/Src/com_pid.c tests/pid/test_pid.c /Febuild/pid-tests/test_pid.exe /Fobuild/pid-tests/
build\pid-tests\test_pid.exe
```

Tests use the real PID implementation. Keep assertions enabled.
These validate controller arithmetic, not aircraft stability or actual task timing.
