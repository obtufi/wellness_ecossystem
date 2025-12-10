"""
Serial uplink helper for TGW <-> GCE.

Framing: [len LSB][len MSB][payload...]
"""

from __future__ import annotations

import threading
from typing import Callable, Optional

import serial
from serial.tools import list_ports


def auto_detect_port() -> Optional[str]:
    """Returns first detected serial port name or None."""
    ports = list(list_ports.comports())
    return ports[0].device if ports else None


class TgwUplinkSerial:
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 1.0, log=None):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self._ser: Optional[serial.Serial] = None
        self._log = log
        self._stop_event = threading.Event()
        self._reader_thread: Optional[threading.Thread] = None
        self._callback: Optional[Callable[[bytes], None]] = None

    def open(self):
        self._ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
        if self._log:
            self._log.info("serial-open", port=self.port, baud=self.baudrate)

    def close(self):
        self._stop_event.set()
        if self._reader_thread and self._reader_thread.is_alive():
            self._reader_thread.join(timeout=2.0)
        if self._ser:
            try:
                self._ser.close()
            finally:
                self._ser = None
        if self._log:
            self._log.info("serial-closed")

    def send_payload(self, payload: bytes):
        if not self._ser:
            raise RuntimeError("serial not open")
        if len(payload) > 0xFFFF:
            raise ValueError("payload too large for framing")
        frame = len(payload).to_bytes(2, "little") + payload
        self._ser.write(frame)
        self._ser.flush()
        if self._log:
            self._log.debug("serial-send", bytes=len(payload))

    def start_reader(self, callback: Callable[[bytes], None]):
        if not self._ser:
            raise RuntimeError("serial not open")
        if self._reader_thread:
            raise RuntimeError("reader already started")
        self._callback = callback
        self._stop_event.clear()
        self._reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader_thread.start()

    def _read_exact(self, n: int) -> bytes:
        buf = b""
        while len(buf) < n and not self._stop_event.is_set():
            chunk = self._ser.read(n - len(buf)) if self._ser else b""
            if not chunk:
                break
            buf += chunk
        return buf

    def _reader_loop(self):
        if not self._ser or not self._callback:
            return
        while not self._stop_event.is_set():
            try:
                header = self._read_exact(2)
                if len(header) < 2:
                    continue
                length = int.from_bytes(header, "little")
                if length == 0 or length > 4096:
                    # discard suspicious length to avoid desync
                    continue
                payload = self._read_exact(length)
                if len(payload) != length:
                    continue
                self._callback(payload)
            except serial.SerialException as exc:
                if self._log:
                    self._log.error("serial-error", err=str(exc))
                break
            except Exception as exc:
                if self._log:
                    self._log.error("serial-loop-exception", err=str(exc))
                continue
        if self._log:
            self._log.info("serial-reader-exit")
