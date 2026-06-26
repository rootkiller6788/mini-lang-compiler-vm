#!/bin/bash
set -e
cd "$(dirname "$0")/.."
echo "=== Running Tests ==="
for t in bin/test_*; do
  if [[ "$t" != *.c ]] && [[ "$t" != *.o ]]; then
    echo ""; "$t"
  fi
done
echo ""
echo "=== All Tests Passed ==="
