#!/usr/bin/env bash
set -euo pipefail

CXX_BIN="${CXX:-g++}"

if ! command -v "${CXX_BIN}" >/dev/null 2>&1; then
  echo "[compile_check_core] Compiler not found: ${CXX_BIN}" >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

COMMON_FLAGS=(
  -std=c++20
  -fsyntax-only
  -Iinclude
  -Isrc
)

FILES=(
  src/stores/LptRepository.cpp
)

echo "[compile_check_core] Using compiler: ${CXX_BIN}"
for file in "${FILES[@]}"; do
  echo "[compile_check_core] Checking ${file}"
  "${CXX_BIN}" "${COMMON_FLAGS[@]}" "${file}"
done

echo "[compile_check_core] All checks passed."
