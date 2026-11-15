#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${REPO_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
BUILD_DIR="${SCRIPT_DIR}/build"

mkdir -p "${BUILD_DIR}"

CXX_BIN="${CXX:-g++}"

"${CXX_BIN}" \
  -std=c++20 \
  -O1 \
  -g \
  -Wall \
  -Wextra \
  -Wno-sign-compare \
  -Werror \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  -I"${SCRIPT_DIR}/stubs" \
  -I"${REPO_ROOT}/components/econet" \
  "${REPO_ROOT}/components/econet/econet.cpp" \
  "${REPO_ROOT}/components/econet/automation.cpp" \
  "${REPO_ROOT}/components/econet/binary_sensor/econet_binary_sensor.cpp" \
  "${REPO_ROOT}/components/econet/climate/econet_climate.cpp" \
  "${REPO_ROOT}/components/econet/number/econet_number.cpp" \
  "${REPO_ROOT}/components/econet/select/econet_select.cpp" \
  "${REPO_ROOT}/components/econet/sensor/econet_sensor.cpp" \
  "${REPO_ROOT}/components/econet/switch/econet_switch.cpp" \
  "${REPO_ROOT}/components/econet/text_sensor/econet_text_sensor.cpp" \
  "${SCRIPT_DIR}/test_econet.cpp" \
  -o "${BUILD_DIR}/test_econet"

ASAN_OPTIONS="detect_leaks=1:halt_on_error=1" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1" \
  "${BUILD_DIR}/test_econet"
