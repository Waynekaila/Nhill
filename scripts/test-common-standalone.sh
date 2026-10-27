#!/usr/bin/env bash
# Runs the business-rule tests of the shared library (tests/common/test_common.cpp:
# money, taxes, currency conversion, establishment codes, validation, passwords)
# with only a C++17 compiler — no CMake, Qt, PostgreSQL or GoogleTest needed.
#
#   scripts/test-common-standalone.sh [--gtest-shim|--gtest-system] [OUTPUT_DIR]
#
# Default: use GoogleTest if it is installed (pkg-config gtest_main), otherwise the
# minimal compatible runner in tests/support/minigtest/. Exit code 0 = all tests pass.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODE=auto
OUT="${TMPDIR:-/tmp}/nhill-test-common"
for a in "$@"; do
  case "$a" in
    --gtest-shim) MODE=shim ;;
    --gtest-system) MODE=system ;;
    -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
    *) OUT="$a" ;;
  esac
done
CXX="${CXX:-g++}"
command -v "$CXX" >/dev/null || { echo "Compilateur C++ introuvable ($CXX). Installez build-essential." >&2; exit 2; }
mkdir -p "$OUT"
SRCS=("$ROOT"/common/src/*.cpp "$ROOT/tests/common/test_common.cpp" "$ROOT/tests/common/test_int128.cpp")
read -ra EXTRA_FLAGS <<<"${CXXFLAGS:-}"   # ex. CXXFLAGS=-DNHILL_PORTABLE_INT128 : chemin Windows (sans __int128)
FLAGS=(-std=c++17 -O1 -g -Wall -Wextra -I"$ROOT/common/include" "${EXTRA_FLAGS[@]}")
if [ "$MODE" = auto ]; then
  if pkg-config --exists gtest_main 2>/dev/null; then MODE=system; else MODE=shim; fi
fi
echo "==> Compilation ($MODE, $($CXX --version | head -1))"
if [ "$MODE" = system ]; then
  # shellcheck disable=SC2046
  "$CXX" "${FLAGS[@]}" "${SRCS[@]}" $(pkg-config --cflags --libs gtest_main) -pthread -o "$OUT/test_common"
else
  "$CXX" "${FLAGS[@]}" -I"$ROOT/tests/support/minigtest" "${SRCS[@]}" -o "$OUT/test_common"
fi
echo "==> Exécution"
"$OUT/test_common"
