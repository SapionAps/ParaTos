#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PARATOS_BIN="${ROOT_DIR}/build/paratos"
TEST_GLOB="${SCRIPT_DIR}/syscalls/*.tos"

if [[ ! -x "${PARATOS_BIN}" ]]; then
	echo "error: ${PARATOS_BIN} not found or not executable"
	echo "hint: build ParaTos first: cmake -S . -B build && cmake --build build -j"
	exit 1
fi

if [[ "${1:-}" != "--no-build" ]]; then
	make -C "${SCRIPT_DIR}" >/dev/null
fi

shopt -s nullglob
tests=( ${TEST_GLOB} )
shopt -u nullglob

if (( ${#tests[@]} == 0 )); then
	echo "error: no syscall test binaries found"
	echo "hint: run make -C test_programs"
	exit 1
fi

workdir="$(mktemp -d "${SCRIPT_DIR}/.syscall-run.XXXXXX")"
cleanup() {
	rm -rf "${workdir}"
}
trap cleanup EXIT

echo "Running ${#tests[@]} syscall tests in ${workdir}"

pass_count=0
fail_count=0

for test_bin in "${tests[@]}"; do
	test_name="$(basename "${test_bin}")"
	if (cd "${workdir}" && "${PARATOS_BIN}" "${test_bin}" >/dev/null 2>&1); then
		echo "PASS ${test_name}"
		pass_count=$((pass_count + 1))
	else
		echo "FAIL ${test_name}"
		fail_count=$((fail_count + 1))
	fi
done

echo "Result: ${pass_count} passed, ${fail_count} failed"

if (( fail_count > 0 )); then
	exit 1
fi
