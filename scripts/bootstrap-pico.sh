#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="$ROOT_DIR/.deps"
PICO_SDK_VERSION="${PICO_SDK_VERSION:-2.3.0}"
mkdir -p "$DEPS_DIR"

if [ ! -d "$DEPS_DIR/pico-sdk/.git" ]; then
  git -C "$DEPS_DIR" clone --depth 1 --branch "$PICO_SDK_VERSION" https://github.com/raspberrypi/pico-sdk.git pico-sdk
else
  git -C "$DEPS_DIR/pico-sdk" fetch --depth 1 origin "$PICO_SDK_VERSION"
  git -C "$DEPS_DIR/pico-sdk" checkout "$PICO_SDK_VERSION"
fi

git -C "$DEPS_DIR/pico-sdk" submodule update --init --recursive
