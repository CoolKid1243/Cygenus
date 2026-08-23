@echo off
REM build_windows.bat - Build script for Windows

echo 🚀 Building Cygenus Engine...

REM Check if Conan is installed
where conan >nul 2>nul
if %errorlevel% neq 0 (
    echo ❌ Conan is not installed!
    echo    Please install Conan:
    echo    pip install conan
    pause
    exit /b 1
)

REM Create build directory if it doesn't exist
if not exist build mkdir build

REM Install dependencies
echo 📦 Installing dependencies with Conan...
call conan install . --build=missing -s compiler.cppstd=17
if %errorlevel% neq 0 (
    echo ❌ Conan install failed!
    pause
    exit /b 1
)

REM Configure with CMake
REM NOTE: Windows/MSVC is a multi-config generator, so Conan names the preset
REM "conan-default" here (macOS/Linux use single-config generators and get "conan-release").
echo ⚙️  Configuring with CMake...
call cmake --preset conan-default
if %errorlevel% neq 0 (
    echo ❌ CMake configure failed!
    pause
    exit /b 1
)

REM Build the project
echo 🔨 Building project...
call cmake --build build --config Release
if %errorlevel% neq 0 (
    echo ❌ Build failed!
    pause
    exit /b 1
)

REM Find the built exe
if exist build\Release\cygenus.exe (
    echo ✅ Build successful!
    echo 🚀 Running Cygenus...
    build\Release\cygenus.exe
) else (
    echo ⚠️  Build reported success but cygenus.exe was not found at build\Release\cygenus.exe
    echo    Searching for it...
    dir /s /b build\cygenus.exe
    pause
)