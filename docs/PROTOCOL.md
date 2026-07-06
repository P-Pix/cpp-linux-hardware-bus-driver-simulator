# LHBD Binary Protocol

## Overview

LHBD uses a simple command/response binary protocol intended to mimic a UART/I2C/SPI-like peripheral link.

All multi-byte integers are encoded in big-endian order.

## Frame layout

```text
0               4               8               12              16
+---------------+-------+-------+-------+-------+---------------+
| MAGIC LHBD    | VER   | CMD   | STAT  | RSV   | SEQ | RSV2    |
+---------------+-------+-------+-------+-------+---------------+
| PAYLOAD_LENGTH                                                |
+---------------------------------------------------------------+
| PAYLOAD ...                                                   |
+---------------------------------------------------------------+
| CRC32                                                         |
+---------------------------------------------------------------+
```

Header size: 16 bytes.
CRC size: 4 bytes.
Maximum payload size: 4096 bytes.

## Header fields

| Field | Size | Description |
|---|---:|---|
| Magic | 4 bytes | `0x4C484244`, ASCII `LHBD` |
| Version | 1 byte | Protocol version, currently `1` |
| Command | 1 byte | Command identifier |
| Status | 1 byte | Response status code |
| Reserved | 1 byte | Future bus flags |
| Sequence | 2 bytes | Request/response correlation |
| Reserved2 | 2 bytes | Reserved |
| Payload length | 4 bytes | Number of payload bytes |
| Payload | N bytes | Command-specific data |
| CRC32 | 4 bytes | CRC computed over header + payload |

## Commands

| Command | Hex | Payload | Response |
|---|---:|---|---|
| Ping | `0x01` | empty | optional `PONG` payload |
| Read register | `0x02` | `u16 address` | `u32 value` |
| Write register | `0x03` | `u16 address + u32 value` | empty |
| Read sensor | `0x04` | `u8 sensor_id` | `f32 value` |
| Get status | `0x05` | empty | `u8 mode + u16 error_flags + f32 uptime` |
| Reset | `0x06` | empty | empty |

## Status codes

| Status | Hex | Meaning |
|---|---:|---|
| OK | `0x00` | Command accepted |
| Invalid command | `0x01` | Unknown command |
| Invalid payload | `0x02` | Payload format is invalid |
| CRC error | `0x03` | Frame CRC is invalid |
| Device busy | `0x04` | Device temporarily unavailable |
| Device fault | `0x05` | Device reports an internal fault |
| Timeout | `0x06` | Timeout state, usually handled by driver |

## Robustness rules

The driver rejects a response when:

- magic is invalid;
- protocol version is unsupported;
- payload is too large;
- frame size does not match payload length;
- CRC32 does not match;
- sequence number does not match the request.

Malformed frames are not partially trusted.
