#!/usr/bin/env bash
set -euo pipefail
binary="$1"
output=$(printf '%s\n' 'submit alpha 1' 'submit beta 1' 'wait' 'status' 'submit bad -1' 'submit overflow 999999999999999999999' 'status extra' 'nonsense' 'quit' | "$binary" --workers 2 --capacity 8)
printf '%s\n' "$output"
for expected in 'ACCEPTED alpha' 'DONE alpha' 'DONE beta' 'IDLE' 'completed=2' 'ERROR usage:' 'ERROR unexpected arguments' 'ERROR unknown command' 'STOPPED'; do
  grep -Fq "$expected" <<< "$output"
done
if "$binary" --workers 0 >/dev/null 2>&1; then exit 1; fi
if "$binary" --capacity -1 >/dev/null 2>&1; then exit 1; fi
if "$binary" --unknown >/dev/null 2>&1; then exit 1; fi
