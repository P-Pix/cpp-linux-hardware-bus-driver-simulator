#!/usr/bin/env python3
"""Integration smoke test for the C++ driver and Python simulator."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path


def wait_for_socket(path: str, timeout_s: float = 5.0) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if os.path.exists(path):
            return True
        time.sleep(0.05)
    return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--socket", default="/tmp/lhbd_integration.sock")
    args = parser.parse_args()

    socket_path = args.socket
    if os.path.exists(socket_path):
        os.unlink(socket_path)

    root = Path(__file__).resolve().parents[1]
    simulator = root / "simulator" / "device_simulator.py"

    proc = subprocess.Popen(
        [sys.executable, str(simulator), "--socket", socket_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    try:
        if not wait_for_socket(socket_path):
            output = proc.stdout.read() if proc.stdout else ""
            print(output)
            raise RuntimeError("simulator socket did not appear")

        result = subprocess.run(
            [args.binary, "--socket", socket_path, "--demo"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        print(result.stdout)
        print(result.stderr, file=sys.stderr)
        if result.returncode != 0:
            return result.returncode
        if "PING: OK" not in result.stdout:
            return 10
        if "WRITE REG 0x0010: OK" not in result.stdout:
            return 11
        if "TEMPERATURE:" not in result.stdout:
            return 12
        return 0
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
        if os.path.exists(socket_path):
            os.unlink(socket_path)


if __name__ == "__main__":
    raise SystemExit(main())
