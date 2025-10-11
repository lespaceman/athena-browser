#!/bin/bash
# Format all C++ source files using clang-format
# Run this script after installing clang-format:
#   sudo apt-get install clang-format

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo "Error: clang-format is not installed"
    echo "Install it with: sudo apt-get install clang-format"
    exit 1
fi

echo "Formatting C++ source files..."

# Format source files
find "$PROJECT_DIR/app/src" -type f \( -name "*.cpp" -o -name "*.h" \) \
    ! -path "*/third_party/*" \
    -exec clang-format -i {} +

# Format test files
find "$PROJECT_DIR/app/tests" -type f \( -name "*.cpp" -o -name "*.h" \) \
    ! -path "*/third_party/*" \
    -exec clang-format -i {} +

echo "Formatting complete!"
echo ""
echo "Files formatted:"
echo "  - app/src/**/*.{cpp,h}"
echo "  - app/tests/**/*.{cpp,h}"
