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
  -DBT_ENABLE_CUPS=0
  -Iinclude
  -Isrc
)

FILES=(
  src/components/CupsPrinterClient.cpp
  src/components/DatabaseSchema.cpp
  src/components/PrnPatcher.cpp
  src/components/PrnValidator.cpp
  src/components/PrnWriter.cpp
  src/components/RenderedDocumentWriter.cpp
  src/repositories/LptRepository.cpp
  src/repositories/PrintSettingsRepository.cpp
  src/repositories/SettingsRepository.cpp
  src/repositories/VehicleRepository.cpp
  src/stores/CurrentTestAxleDataStore.cpp
  src/stores/LptStore.cpp
  src/stores/PrintStatusStore.cpp
  src/stores/PrnPayloadStore.cpp
  src/stores/SelectedVehicleStore.cpp
  src/stores/SerialDeviceStore.cpp
)

# Excluded from this lightweight check because they require external include paths/libraries
# (e.g., cpp-httplib, libserial, renderer internals, or full app wiring).

echo "[compile_check_core] Using compiler: ${CXX_BIN}"
for file in "${FILES[@]}"; do
  echo "[compile_check_core] Checking ${file}"
  "${CXX_BIN}" "${COMMON_FLAGS[@]}" "${file}"
done

echo "[compile_check_core] All checks passed."
