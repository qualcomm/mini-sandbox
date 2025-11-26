#!/bin/bash
set -e

GO_VERSION="1.25.0"
ARCH="amd64"
OS="linux"
INSTALL_DIR="/usr/local"
PROFILE_FILE="$HOME/.bash_profile"

# Download Go
echo "Downloading Go ${GO_VERSION}..."
curl -LO "https://go.dev/dl/go${GO_VERSION}.${OS}-${ARCH}.tar.gz"

# Remove any previous Go installation
echo "Removing previous Go installation from ${INSTALL_DIR}/go..."
sudo rm -rf "${INSTALL_DIR}/go"

# Extract and install
echo "Installing Go ${GO_VERSION} to ${INSTALL_DIR}/go..."
sudo tar -C "${INSTALL_DIR}" -xzf "go${GO_VERSION}.${OS}-${ARCH}.tar.gz"

# Set up environment variables
echo "Setting up Go environment..."
{
  echo "export PATH=\$PATH:${INSTALL_DIR}/go/bin"
  echo "export GOPATH=\$HOME/go"
  echo "export PATH=\$PATH:\$GOPATH/bin"
} >> "$PROFILE_FILE"

# Apply changes
source "$PROFILE_FILE"

# Verify installation
echo "Go version installed:"
go version

echo "run `source $PROFILE_FILE` or log in another time to add go into the PATH env"
