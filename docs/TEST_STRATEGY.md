# Test Strategy

## Unit tests

The unit tests are implemented in `tests/test_main.cpp` without external dependencies. They validate:

- frame encode/decode round-trip;
- CRC corruption detection;
- driver ping transaction;
- register read/write transaction;
- retry after timeout;
- retry after CRC error;
- sensor and status parsing.

The tests use `MockTransport` to avoid depending on the simulator or real hardware.

## Integration test

The integration test `scripts/run_integration_test.py` launches the Python simulator and runs the C++ CLI in demo mode. This validates:

- UNIX socket transport;
- full binary protocol exchange;
- Python/C++ interoperability;
- real command handling;
- basic end-to-end behavior.

## CI/CD

The GitHub Actions workflow runs:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
python3 scripts/run_integration_test.py --binary build/lhbd_cli --socket /tmp/lhbd_ci.sock
```

This ensures the project remains buildable and testable on a clean Linux runner.
