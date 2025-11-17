#!/bin/bash
set -e

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
BUILD_DIR="$SCRIPT_DIR/mini_sandbox/src/main/tools/out"
HEADER_DIR="$SCRIPT_DIR/mini_sandbox/src/main/tools"
RELEASE_DIR="$SCRIPT_DIR/release"


build() {
    MINITAP="$SCRIPT_DIR/minitap" make -j -C "$SCRIPT_DIR/mini_sandbox/src/main/tools" all
}


release() {
    build
    echo "Creating release directory structure..."
    rm -rf $RELEASE_DIR
    mkdir -p "$RELEASE_DIR/bin" "$RELEASE_DIR/lib" "$RELEASE_DIR/python" "$RELEASE_DIR/include"

    echo "Copying selected binaries..."
    cp "$BUILD_DIR/mini-sandbox" "$RELEASE_DIR/bin/" || true
    cp "$BUILD_DIR/minitap" "$RELEASE_DIR/bin/" || true
    cp "$BUILD_DIR/mini-tapbox" "$RELEASE_DIR/bin/" || true

    echo "Copying libraries (.so and .a)..."
    find "$BUILD_DIR" -maxdepth 1 -type f \( -name "*.so" -o -name "*.a" \) -exec cp {} "$RELEASE_DIR/lib/" \;
    cp "$BUILD_DIR/minitap" "$RELEASE_DIR/lib/" || true

    echo "Copying headers (.h)..."
    cp "$HEADER_DIR/linux-sandbox-api-c.h" "$RELEASE_DIR/include" || true
    cp "$HEADER_DIR/linux-sandbox-api.h" "$RELEASE_DIR/include" || true
     

    echo "Copying Python bindings..."
    PY_SRC="$SCRIPT_DIR/mini_sandbox/src/py/ctype_bindings"
    cp "$PY_SRC/pyminisandbox.py" "$RELEASE_DIR/python/" || true
    cp "$PY_SRC/pyminitapbox.py" "$RELEASE_DIR/python/" || true
    cp "$BUILD_DIR/minitap" "$RELEASE_DIR/python/" || true
    find "$BUILD_DIR" -maxdepth 1 -type f -name "*.so" -exec cp {} "$RELEASE_DIR/python/" \;

}


install() {
    release

    echo "Installing binaries to /usr/local/bin..."
    if [ -z "${DOCKER_BUILD}" ]; then
    	sudo cp "$RELEASE_DIR/bin/"* /usr/local/bin/
    else
    	cp "$RELEASE_DIR/bin/"* /usr/local/bin/
    fi;

    echo "Installing libraries to /usr/local/lib..."
    if [ -z "${DOCKER_BUILD}" ]; then
        sudo cp "$RELEASE_DIR/lib/"* /usr/local/lib/
    else
        cp "$RELEASE_DIR/lib/"* /usr/local/lib/
    fi;

    #echo "Installing Python packages to user site-packages..."
    #PYTHON_SITE=$(python3 -m site --user-site)
    #mkdir -p "$PYTHON_SITE"
    #cp "$RELEASE_DIR/python/"*.py "$PYTHON_SITE/"
}

# Main logic
case "$1" in
    --release)
        release
        ;;
    --install)
        install
        ;;
    *)
        build
        ;;
esac
