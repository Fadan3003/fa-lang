#!/bin/sh
# One command to build and install Fa:   sh install.sh
# With windows on screen (needs raylib, see install-raylib.sh):   sh install.sh gui
# Afterwards you can type  fa  from anywhere.
set -e
cd "$(dirname "$0")"

if ! command -v g++ >/dev/null 2>&1; then
  echo "g++ (the C++ compiler) is missing - installing it, this needs your password..."
  if command -v apt >/dev/null 2>&1; then
    sudo apt install -y g++
  else
    echo "Please install a C++ compiler (g++ or clang++), then run this again."
    exit 1
  fi
fi

echo "Building Fa..."
sh build.sh "$1"

mkdir -p "$HOME/.local/bin"
cp fa "$HOME/.local/bin/fa"
chmod +x "$HOME/.local/bin/fa"

case ":$PATH:" in
  *":$HOME/.local/bin:"*) ;;
  *)
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$HOME/.bashrc"
    echo "Added ~/.local/bin to PATH in ~/.bashrc - open a NEW terminal before using fa."
    ;;
esac

echo ""
echo "Fa is installed. Try:"
echo "  fa new mysite"
echo "  fa mysite"
