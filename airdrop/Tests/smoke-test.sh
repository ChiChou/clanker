#!/bin/sh
set -eu

binary=${1:?binary path required}
output=$($binary --help)

case "$output" in
  *"USAGE: airdrop <file1>"*"--help – print help info"*) ;;
  *)
    echo "Unexpected help output" >&2
    exit 1
    ;;
esac

echo "Smoke test passed"
