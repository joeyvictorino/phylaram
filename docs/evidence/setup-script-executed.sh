#!/bin/bash
set -euo pipefail
export CARGO_HOME=/workspace/.phylaram-tools/cargo
export RUSTUP_HOME=/workspace/.phylaram-tools/rustup
export PATH="$CARGO_HOME/bin:$PATH"
export CXX=g++
export CARGO_BUILD_JOBS=4
command -v python3
command -v g++
if ! command -v rustup >/dev/null 2>&1; then
  installer=$(mktemp /tmp/phylaram-rustup.XXXXXX)
  trap 'rm -f "$installer"' EXIT
  curl --proto '=https' --tlsv1.2 -fsS https://sh.rustup.rs -o "$installer"
  sh "$installer" -y --no-modify-path --profile minimal --default-toolchain 1.99.0 --component rustfmt --component clippy
else
  rustup toolchain install 1.99.0 --profile minimal --component rustfmt --component clippy
fi
export RUSTUP_TOOLCHAIN=1.99.0
cd /workspace/phylaram
cargo fetch --locked --manifest-path tools/phylaram-verify/Cargo.toml
cargo build --locked --manifest-path tools/phylaram-verify/Cargo.toml
cargo build --release --locked --manifest-path tools/phylaram-verify/Cargo.toml
python3 tests/run_all_tests.py
python3 -m compileall -q scripts tests tools
git status --short
