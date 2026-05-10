#!/usr/bin/env bash
#
# scripts/run-vm.sh — spin a microVM with chubby-cat.
#
# Usage:
#   scripts/run-vm.sh                          # boot the default examples/hello.bin guest
#   scripts/run-vm.sh path/to/payload.bin      # boot a custom flat-binary aarch64 guest
#   scripts/run-vm.sh -- --mem 128 -v          # pass options through to chubby-cat
#   scripts/run-vm.sh payload.bin -- --mem 128 # both: a payload and chubby-cat options
#
# The script:
#   1. Checks you're on macOS / Apple Silicon.
#   2. Builds build/chubby-cat if it's missing.
#   3. Builds examples/hello.bin if the default guest is requested and missing.
#   4. Execs chubby-cat with the resolved payload and any passthrough args.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

BIN="build/chubby-cat"
DEFAULT_PAYLOAD="examples/hello.bin"

PAYLOAD="${DEFAULT_PAYLOAD}"
PASSTHROUGH=()

# Parse: optional first positional payload, then "--", then passthrough args.
if [[ $# -gt 0 && "$1" != "--" && "$1" != -* ]]; then
    PAYLOAD="$1"
    shift
fi
if [[ $# -gt 0 && "$1" == "--" ]]; then
    shift
fi
if [[ $# -gt 0 ]]; then
    PASSTHROUGH=("$@")
fi

# Preflight.
if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: chubby-cat only runs on macOS." >&2
    exit 1
fi
if [[ "$(uname -m)" != "arm64" ]]; then
    echo "error: chubby-cat requires Apple Silicon (arm64). Detected: $(uname -m)" >&2
    exit 1
fi
if ! command -v clang++ >/dev/null 2>&1; then
    echo "error: clang++ not found. Install Xcode command-line tools: xcode-select --install" >&2
    exit 1
fi

# Build hypervisor if missing or stale relative to sources.
build_needed=0
if [[ ! -x "${BIN}" ]]; then
    build_needed=1
else
    for src in src/*.cpp src/*.hpp; do
        if [[ "${src}" -nt "${BIN}" ]]; then
            build_needed=1
            break
        fi
    done
fi
if (( build_needed )); then
    echo "==> building chubby-cat"
    make --no-print-directory
fi

# Build the bundled example guest if the user didn't override and it's missing.
if [[ "${PAYLOAD}" == "${DEFAULT_PAYLOAD}" && ! -f "${PAYLOAD}" ]]; then
    echo "==> building example guest payload"
    make --no-print-directory -C examples
fi

if [[ ! -f "${PAYLOAD}" ]]; then
    echo "error: payload not found: ${PAYLOAD}" >&2
    exit 1
fi

echo "==> spinning microVM  (payload: ${PAYLOAD})"
exec "${BIN}" "${PAYLOAD}" ${PASSTHROUGH[@]+"${PASSTHROUGH[@]}"}
