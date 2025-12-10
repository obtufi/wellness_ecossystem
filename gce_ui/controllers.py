"""
Backend controller that bridges TGW serial uplink, database, and Qt widgets.
"""

from __future__ import annotations

import threading
from pathlib import Path
from typing import List, Optional

import structlog
from PySide6.QtCore import QObject, Signal

from gce_store import GceStore
from rsn_proto import RsnConfig
from tgw_proto import (
    UpConfigAckFrame,
    UpHelloFrame,
    UpTelemetryFrame,
    build_down_config_payload,
    build_down_handshake_payload,
    parse_up_payload,
)
from tgw_uplink_serial import TgwUplinkSerial

from .models import NodeRow, TelemetryRow


class GceBackendController(QObject):
    """
    Coordinates serial uplink callbacks, database writes, and UI updates via Qt signals.
    """

    node_updated = Signal(int)
    telemetry_updated = Signal(int)
    log_message = Signal(str)
    connection_state_changed = Signal(bool, str)

    def __init__(self, db_path: Path | str = Path("gce_data.sqlite3"), parent: Optional[QObject] = None):
        super().__init__(parent)
        self._logger = structlog.get_logger("gce_ui")
        self._store = GceStore(Path(db_path), log=self._logger)
        self._store_lock = threading.Lock()
        self._link: Optional[TgwUplinkSerial] = None
        self._connected_port: Optional[str] = None
        self._baud: Optional[int] = None

    @property
    def is_connected(self) -> bool:
        """Return True when a TGW serial link is open."""
        return self._link is not None

    def shutdown(self):
        """Close serial link and DB."""
        self.disconnect_from_tgw()
        self._store.close()

    def connect_to_tgw(self, port: str, baud: int) -> bool:
        """Open serial link and start the reader thread."""
        self.disconnect_from_tgw()
        try:
            self._link = TgwUplinkSerial(port, baudrate=baud, log=self._logger)
            self._link.open()
            self._link.start_reader(self._on_payload)
        except Exception as exc:
            self._emit_log("connection-error", level="error", err=str(exc))
            self.connection_state_changed.emit(False, f"Erro: {exc}")
            self._link = None
            return False

        self._connected_port = port
        self._baud = baud
        self.connection_state_changed.emit(True, f"Conectado a {port} @ {baud}")
        self._emit_log("connected", port=port, baud=baud)
        return True

    def disconnect_from_tgw(self):
        """Stop reader and close the serial port."""
        if self._link:
            try:
                self._link.close()
            except Exception as exc:
                self._emit_log("disconnect-error", level="warning", err=str(exc))
            self._link = None
        if self._connected_port:
            self._emit_log("disconnected", port=self._connected_port)
        self._connected_port = None
        self.connection_state_changed.emit(False, "Desconectado")

    def list_nodes(self) -> List[NodeRow]:
        """Return all known nodes."""
        with self._store_lock:
            return self._store.list_nodes()

    def list_recent_telemetry(self, node_id: int, limit: int = 100) -> List[TelemetryRow]:
        """Return last telemetry rows for a node."""
        with self._store_lock:
            return self._store.list_recent_telemetry(node_id, limit=limit)

    def send_config(self, node_id: int, cfg: RsnConfig) -> bool:
        """Send configuration frame to a node."""
        if not self._link:
            self._emit_log("send-config-failed", level="warning", reason="not-connected")
            return False
        payload = build_down_config_payload(node_id, cfg)
        try:
            self._link.send_payload(payload)
        except Exception as exc:
            self._emit_log("send-config-error", level="error", err=str(exc))
            return False
        self._emit_log("config-sent", node_id=node_id)
        return True

    def send_handshake(self, node_id: int) -> bool:
        """Send optional handshake frame."""
        if not self._link:
            self._emit_log("send-handshake-failed", level="warning", reason="not-connected")
            return False
        payload = build_down_handshake_payload(node_id)
        try:
            self._link.send_payload(payload)
        except Exception as exc:
            self._emit_log("send-handshake-error", level="error", err=str(exc))
            return False
        self._emit_log("handshake-sent", node_id=node_id)
        return True

    def _on_payload(self, payload: bytes):
        """Handle raw payload from serial reader thread."""
        try:
            frame = parse_up_payload(payload)
        except Exception as exc:
            self._emit_log("frame-parse-failed", level="warning", err=str(exc))
            return

        try:
            if isinstance(frame, UpHelloFrame):
                with self._store_lock:
                    self._store.upsert_node(frame.node_id, frame.rssi, frame.hello)
                self.node_updated.emit(frame.node_id)
                self._emit_log("hello-received", node_id=frame.node_id, rssi=frame.rssi)
            elif isinstance(frame, UpTelemetryFrame):
                with self._store_lock:
                    self._store.add_telemetry(frame.node_id, frame.rssi, frame.tgw_local_ts_ms, frame.telemetry)
                    self._store.touch_node(frame.node_id, frame.rssi, frame.telemetry.header.hw_version, frame.telemetry.header.fw_version)
                self.telemetry_updated.emit(frame.node_id)
                self._emit_log(
                    "telemetry-received",
                    node_id=frame.node_id,
                    rssi=frame.rssi,
                    tgw_ts_ms=frame.tgw_local_ts_ms,
                )
            elif isinstance(frame, UpConfigAckFrame):
                with self._store_lock:
                    self._store.add_config_ack(frame.node_id, frame.rssi, frame.ack)
                    self._store.touch_node(frame.node_id, frame.rssi, frame.ack.header.hw_version, frame.ack.header.fw_version)
                self.node_updated.emit(frame.node_id)
                self._emit_log("config-ack-received", node_id=frame.node_id, status=frame.ack.status)
            else:
                self._emit_log("unknown-frame", level="warning", frame_type=type(frame).__name__)
        except Exception as exc:
            self._emit_log("store-error", level="error", err=str(exc))

    def _emit_log(self, event: str, level: str = "info", **fields):
        """Send log both to structlog and to the UI console."""
        text = event
        if fields:
            kv = " ".join(f"{k}={v}" for k, v in fields.items())
            text = f"{event} {kv}"
        self.log_message.emit(text)
        log_fn = getattr(self._logger, level, self._logger.info)
        log_fn(event, **fields)
