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
conan install . --build=missing

REM Configure with CMake
echo ⚙️  Configuring with CMake...
cmake --preset conan-release

REM Build the project
echo 🔨 Building project...
cmake --build build/Release --config Release

REM Check if build succeeded
if %errorlevel% equ 0 (
    echo ✅ Build successful!
    echo 🚀 Running Cygenus...
    build\Release\Release\cygenus.exe
) else (
    echo ❌ Build failed!
    pause
    exit /b 1
)