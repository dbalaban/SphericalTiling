#!/bin/bash
# Setup script for third-party dependencies

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
THIRD_PARTY_DIR="$SCRIPT_DIR/third_party"

echo "Setting up third-party dependencies..."

# Create third_party directory if it doesn't exist
mkdir -p "$THIRD_PARTY_DIR"

# Clone ImGui if not present
if [ ! -d "$THIRD_PARTY_DIR/imgui" ]; then
    echo "Cloning ImGui..."
    git clone https://github.com/ocornut/imgui.git "$THIRD_PARTY_DIR/imgui"
    cd "$THIRD_PARTY_DIR/imgui"
    git checkout v1.90.1
    cd "$SCRIPT_DIR"
else
    echo "ImGui already exists, skipping..."
fi

# Generate GLAD if not present
if [ ! -d "$THIRD_PARTY_DIR/glad" ]; then
    echo "Generating GLAD loader..."
    
    # Check if python3 is available
    if ! command -v python3 &> /dev/null; then
        echo "Error: python3 is required to generate GLAD"
        exit 1
    fi
    
    # Install glad2 if not already installed
    pip3 install --user glad2 2>/dev/null || true
    
    # Generate GLAD files
    cd "$THIRD_PARTY_DIR"
    python3 -m glad --api="gl:core=3.3" --out-path=glad c
    cd "$SCRIPT_DIR"
else
    echo "GLAD already exists, skipping..."
fi

echo "Third-party dependencies setup complete!"
echo ""
echo "You can now build the project:"
echo "  mkdir build && cd build"
echo "  cmake .."
echo "  cmake --build ."
