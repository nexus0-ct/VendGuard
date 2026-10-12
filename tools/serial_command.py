#!/usr/bin/env python3
"""Send a command to the live Wokwi RFC2217 UART and save the response."""
import argparse
import time
from pathlib import Path

import serial

parser = argparse.ArgumentParser()
parser.add_argument("command")
parser.add_argument("--output", required=True)
parser.add_argument("--port", type=int, default=4000)
parser.add_argument("--seconds", type=float, default=2)
args = parser.parse_args()
if args.seconds <= 0:
    parser.error("--seconds must be positive")

connection = serial.serial_for_url(
    f"rfc2217://localhost:{args.port}", baudrate=115200, timeout=0.1
)
try:
    connection.write(args.command.encode("ascii") + b"\r\n")
    response = bytearray()
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        response.extend(connection.read(4096))
finally:
    connection.close()

Path(args.output).write_bytes(response)
print(response.decode("ascii", errors="replace"))
