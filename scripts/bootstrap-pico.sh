#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="$ROOT_DIR/.deps"
mkdir -p "$DEPS_DIR"

if [ ! -d "$DEPS_DIR/pico-sdk/.git" ]; then
  git -C "$DEPS_DIR" clone --depth 1 https://github.com/raspberrypi/pico-sdk.git pico-sdk
fi

if [ ! -d "$DEPS_DIR/pico-extras/.git" ]; then
  git -C "$DEPS_DIR" clone --depth 1 https://github.com/raspberrypi/pico-extras.git pico-extras
fi

git -C "$DEPS_DIR/pico-sdk" submodule update --init --recursive

git -C "$DEPS_DIR/pico-extras" submodule update --init --recursive
