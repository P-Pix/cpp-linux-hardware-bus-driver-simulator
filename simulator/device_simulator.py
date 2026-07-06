#!/usr/bin/env python3
"""Simulated Linux hardware peripheral for the LHBD C++ driver.

The simulator exposes a UNIX domain socket and replies to binary command frames.
It intentionally behaves like a small user-space stand-in for an UART/I2C/SPI
peripheral: register map, sensor readings, optional delay, CRC corruption and
packet drop simulation.
"""

from __future__ import annotations

import argparse
import os
import random
import socket
import struct
import time
import zlib
from dataclasses import dataclass, field

MAGIC = 0x4C484244  # LHBD
VERSION = 1
HEADER_SIZE = 16
CRC_SIZE = 4
MAX_PAYLOAD = 4096

CMD_PING = 0x01
CMD_READ_REGISTER = 0x02
CMD_WRITE_REGISTER = 0x03
CMD_READ_SENSOR = 0x04
CMD_GET_STATUS = 0x05
CMD_RESET = 0x06

STATUS_OK = 0x00
STATUS_INVALID_COMMAND = 0x01
STATUS_INVALID_PAYLOAD = 0x02
STATUS_CRC_ERROR = 0x03
STATUS_DEVICE_BUSY = 0x04
STATUS_DEVICE_FAULT = 0x05

SENSOR_TEMPERATURE = 0x01
SENSOR_VOLTAGE = 0x02
SENSOR_CURRENT = 0x03
SENSOR_GYRO_X = 0x04


@dataclass
class Frame:
    command: int
    status: int
    sequence: int
    payload: bytes = b""


def encode(frame: Frame) -> bytes:
    header = struct.pack(
        ">IBBBBHHI",
        MAGIC,
        VERSION,
        frame.command & 0xFF,
        frame.status & 0xFF,
        0,
        frame.sequence & 0xFFFF,
        0,
        len(frame.payload),
    )
    body = header + frame.payload
    crc = zlib.crc32(body) & 0xFFFFFFFF
    return body + struct.pack(">I", crc)


def read_exact(conn: socket.socket, size: int) -> bytes | None:
    chunks: list[bytes] = []
    remaining = size
    while remaining > 0:
        chunk = conn.recv(remaining)
        if not chunk:
            return None
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def read_frame(conn: socket.socket) -> Frame | None:
    header = read_exact(conn, HEADER_SIZE)
    if header is None:
        return None

    magic, version, command, status, _reserved, sequence, _reserved2, payload_len = struct.unpack(
        ">IBBBBHHI", header
    )
    if magic != MAGIC or version != VERSION or payload_len > MAX_PAYLOAD:
        return Frame(command=command, status=STATUS_INVALID_PAYLOAD, sequence=sequence)

    payload_and_crc = read_exact(conn, payload_len + CRC_SIZE)
    if payload_and_crc is None:
        return None

    payload = payload_and_crc[:payload_len]
    expected_crc = struct.unpack(">I", payload_and_crc[payload_len:])[0]
    actual_crc = zlib.crc32(header + payload) & 0xFFFFFFFF
    if actual_crc != expected_crc:
        return Frame(command=command, status=STATUS_CRC_ERROR, sequence=sequence)

    return Frame(command=command, status=status, sequence=sequence, payload=payload)


@dataclass
class DeviceState:
    registers: dict[int, int] = field(default_factory=lambda: {0x0001: 0xCAFECAFE, 0x0002: 0x00000042})
    start_time: float = field(default_factory=time.monotonic)
    mode: int = 1
    error_flags: int = 0

    def reset(self) -> None:
        self.registers = {0x0001: 0xCAFECAFE, 0x0002: 0x00000042}
        self.start_time = time.monotonic()
        self.mode = 1
        self.error_flags = 0

    def sensor_value(self, sensor: int) -> float:
        elapsed = time.monotonic() - self.start_time
        if sensor == SENSOR_TEMPERATURE:
            return 22.0 + random.uniform(-0.25, 0.25)
        if sensor == SENSOR_VOLTAGE:
            return 3.3 + random.uniform(-0.02, 0.02)
        if sensor == SENSOR_CURRENT:
            return 0.120 + random.uniform(-0.005, 0.005)
        if sensor == SENSOR_GYRO_X:
            return 0.01 * elapsed
        return float("nan")


def handle_frame(frame: Frame, state: DeviceState) -> Frame:
    if frame.status == STATUS_CRC_ERROR:
        return Frame(command=frame.command, status=STATUS_CRC_ERROR, sequence=frame.sequence)

    if frame.command == CMD_PING:
        return Frame(command=frame.command, status=STATUS_OK, sequence=frame.sequence, payload=b"PONG")

    if frame.command == CMD_READ_REGISTER:
        if len(frame.payload) != 2:
            return Frame(frame.command, STATUS_INVALID_PAYLOAD, frame.sequence)
        address = struct.unpack(">H", frame.payload)[0]
        value = state.registers.get(address, 0)
        return Frame(frame.command, STATUS_OK, frame.sequence, struct.pack(">I", value))

    if frame.command == CMD_WRITE_REGISTER:
        if len(frame.payload) != 6:
            return Frame(frame.command, STATUS_INVALID_PAYLOAD, frame.sequence)
        address, value = struct.unpack(">HI", frame.payload)
        state.registers[address] = value
        return Frame(frame.command, STATUS_OK, frame.sequence)

    if frame.command == CMD_READ_SENSOR:
        if len(frame.payload) != 1:
            return Frame(frame.command, STATUS_INVALID_PAYLOAD, frame.sequence)
        sensor = frame.payload[0]
        value = state.sensor_value(sensor)
        if value != value:  # NaN check without importing math.
            return Frame(frame.command, STATUS_INVALID_PAYLOAD, frame.sequence)
        return Frame(frame.command, STATUS_OK, frame.sequence, struct.pack(">f", value))

    if frame.command == CMD_GET_STATUS:
        uptime = time.monotonic() - state.start_time
        payload = struct.pack(">BHf", state.mode, state.error_flags, uptime)
        return Frame(frame.command, STATUS_OK, frame.sequence, payload)

    if frame.command == CMD_RESET:
        state.reset()
        return Frame(frame.command, STATUS_OK, frame.sequence)

    return Frame(frame.command, STATUS_INVALID_COMMAND, frame.sequence)


def serve(args: argparse.Namespace) -> None:
    if os.path.exists(args.socket):
        os.unlink(args.socket)

    state = DeviceState()
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(args.socket)
    server.listen(1)
    print(f"[simulator] listening on {args.socket}", flush=True)

    try:
        while True:
            conn, _ = server.accept()
            print("[simulator] client connected", flush=True)
            with conn:
                while True:
                    frame = read_frame(conn)
                    if frame is None:
                        break

                    if args.delay_ms > 0:
                        time.sleep(args.delay_ms / 1000.0)

                    if random.random() < args.drop_rate:
                        print("[simulator] dropped response", flush=True)
                        continue

                    response = handle_frame(frame, state)
                    encoded = bytearray(encode(response))
                    if random.random() < args.corrupt_rate:
                        encoded[-1] ^= 0xFF
                        print("[simulator] corrupted response CRC", flush=True)
                    conn.sendall(encoded)
            print("[simulator] client disconnected", flush=True)
    finally:
        server.close()
        if os.path.exists(args.socket):
            os.unlink(args.socket)


def main() -> int:
    parser = argparse.ArgumentParser(description="LHBD simulated hardware peripheral")
    parser.add_argument("--socket", default="/tmp/lhbd_device.sock")
    parser.add_argument("--delay-ms", type=int, default=0)
    parser.add_argument("--drop-rate", type=float, default=0.0)
    parser.add_argument("--corrupt-rate", type=float, default=0.0)
    args = parser.parse_args()
    serve(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
