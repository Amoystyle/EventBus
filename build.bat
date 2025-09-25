@echo off
echo ========================================
echo    EventBus Enterprise Build Script
echo ========================================
echo.

REM Initialize Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1

echo Building EventBus components...
echo.

REM Build simple test
echo [1/3] Building simple test...
cl /EHsc /std:c++17 /O2 simple_test.cpp /Fe:simple_test.exe > nul 2>&1
if %errorlevel% neq 0 (
    echo Simple test build failed!
    exit /b 1
) else (
    echo simple_test.exe
)

REM Build complete test
echo [2/3] Building complete test...
cl /EHsc /std:c++17 /O2 test_full.cpp /Fe:complete_test.exe > nul 2>&1
if %errorlevel% neq 0 (
    echo Complete test build failed!
    exit /b 1
) else (
    echo complete_test.exe
)

REM Build usage example
echo [3/3] Building usage example...
cl /EHsc /std:c++17 /O2 example_simple.cpp /Fe:usage_example.exe > nul 2>&1
if %errorlevel% neq 0 (
    echo Usage example build failed!
    exit /b 1
) else (
    echo usage_example.exe
)

echo.
echo ========================================
echo    All builds successful!
echo ========================================
echo.
echo Available executables:
echo   simple_test.exe      - Basic functionality test
echo   complete_test.exe    - Full feature test with performance
echo   usage_example.exe    - Real-world usage example
echo.
echo To run a quick test:
echo   simple_test.exe
echo.
