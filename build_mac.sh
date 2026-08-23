#!/bin/bash
# build_mac.sh - Build script for macOS and Linux

echo "🚀 Building Cygenus Engine..."

# Check if Conan is installed
if ! command -v conan &> /dev/null; then
    echo "❌ Conan is not installed!"
    echo "   Please install Conan:"
    echo "   pip install conan"
    exit 1
fi

# Create build directory if it doesn't exist
mkdir -p build

# Install dependencies
echo "📦 Installing dependencies with Conan..."
conan install . --build=missing -s compiler.cppstd=17

# Configure with CMake
echo "⚙️  Configuring with CMake..."
cmake --preset conan-release

# Build the project
echo "🔨 Building project..."
cmake --build build/Release

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
    echo "🚀 Running Cygenus..."
    ./build/Release/cygenus
else
    echo "❌ Build failed!"
    exit 1
fi