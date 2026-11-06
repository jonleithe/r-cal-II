#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

usage() {
    echo "Usage: $0 [all|debug|release]"
    echo "Defaults to all (builds both debug and release)."
}

build() {
    local mode="$1"
    local config="$2"
    local output_dir="$project_dir/build/$mode"
    cmake -S "$project_dir" -B "$output_dir" -DCMAKE_BUILD_TYPE="$config"
    cmake --build "$output_dir" --parallel
}

if (( $# > 1 )); then
    usage >&2
    exit 2
fi

case "${1:-all}" in
    all)
        build debug Debug
        build release Release
        ;;
    debug) build debug Debug ;;
    release) build release Release ;;
    -h|--help) usage ;;
    *) usage >&2; exit 2 ;;
esac
