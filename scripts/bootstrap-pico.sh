#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="$ROOT_DIR/.deps"
PICO_SDK_VERSION="${PICO_SDK_VERSION:-2.3.0}"
PICO_SDK_DIR="${PICO_SDK_PATH:-$DEPS_DIR/pico-sdk}"
LITTLEFS_DIR="$DEPS_DIR/littlefs"
mkdir -p "$DEPS_DIR"

mkdir -p "$(dirname "$PICO_SDK_DIR")"
if [ ! -d "$PICO_SDK_DIR/.git" ]; then
  git clone --depth 1 --branch "$PICO_SDK_VERSION" https://github.com/raspberrypi/pico-sdk.git "$PICO_SDK_DIR"
else
  git -C "$PICO_SDK_DIR" fetch --depth 1 origin "$PICO_SDK_VERSION"
  git -C "$PICO_SDK_DIR" checkout "$PICO_SDK_VERSION"
fi

git -C "$PICO_SDK_DIR" submodule update --init --recursive

mkdir -p "$DEPS_DIR"
if [ ! -d "$LITTLEFS_DIR/.git" ]; then
  git clone --depth 1 https://github.com/littlefs-project/littlefs.git "$LITTLEFS_DIR"
else
  git -C "$LITTLEFS_DIR" pull --ff-only
fi
