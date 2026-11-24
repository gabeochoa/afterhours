#!/bin/bash
# Delegate to the one example test runner (examples/run_all_as_tests.sh).
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../run_all_as_tests.sh" "$@"
