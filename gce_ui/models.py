"""
Qt models for nodes and telemetry tables.
"""

from __future__ import annotations

from typing import Any, Dict, List, Optional

from PySide6.QtCore import QAbstractTableModel, QModelIndex, Qt

NodeRow = Dict[str, Any]
TelemetryRow = Dict[str, Any]


class NodesTableModel(QAbstractTableModel):
    """Table model for RSN nodes stored in the database."""

    headers = ["node_id", "last_seen", "last_rssi", "hw_version", "fw_version", "capabilities"]

    def __init__(self, nodes: Optional[List[NodeRow]] = None, parent=None):
        super().__init__(parent)
        self._nodes: List[NodeRow] = nodes or []

    def rowCount(self, parent: QModelIndex = QModelIndex()) -> int:  # type: ignore[override]
        return 0 if parent.isValid() else len(self._nodes)

    def columnCount(self, parent: QModelIndex = QModelIndex()) -> int:  # type: ignore[override]
        return 0 if parent.isValid() else len(self.headers)

    def data(self, index: QModelIndex, role: int = Qt.DisplayRole):  # type: ignore[override]
        if not index.isValid() or role not in (Qt.DisplayRole, Qt.EditRole):
            return None
        key = self.headers[index.column()]
        value = self._nodes[index.row()].get(key, "")
        if key == "capabilities":
            return f"0x{int(value):04X}"
        return str(value)

    def headerData(self, section: int, orientation: Qt.Orientation, role: int = Qt.DisplayRole):  # type: ignore[override]
        if role != Qt.DisplayRole or orientation != Qt.Horizontal:
            return None
        titles = {
            "node_id": "Node",
            "last_seen": "Last Seen (UTC)",
            "last_rssi": "RSSI",
            "hw_version": "HW",
            "fw_version": "FW",
            "capabilities": "Caps",
        }
        key = self.headers[section]
        return titles.get(key, key)

    def update_data(self, nodes: List[NodeRow]):
        """Replace table content."""
        self.beginResetModel()
        self._nodes = nodes
        self.endResetModel()

    def node_id_at(self, row: int) -> Optional[int]:
        if row < 0 or row >= len(self._nodes):
            return None
        try:
            return int(self._nodes[row].get("node_id"))
        except Exception:
            return None


class TelemetryTableModel(QAbstractTableModel):
    """Table model for telemetry of a single node."""

    headers = [
        "cycle",
        "tgw_local_ts_ms",
        "batt_status",
        "flags",
        "soil_mean_raw",
        "vbat_mean_raw",
        "ntc_mean_raw",
        "rssi",
    ]

    def __init__(self, rows: Optional[List[TelemetryRow]] = None, parent=None):
        super().__init__(parent)
        self._rows: List[TelemetryRow] = rows or []

    def rowCount(self, parent: QModelIndex = QModelIndex()) -> int:  # type: ignore[override]
        return 0 if parent.isValid() else len(self._rows)

    def columnCount(self, parent: QModelIndex = QModelIndex()) -> int:  # type: ignore[override]
        return 0 if parent.isValid() else len(self.headers)

    def data(self, index: QModelIndex, role: int = Qt.DisplayRole):  # type: ignore[override]
        if not index.isValid() or role not in (Qt.DisplayRole, Qt.EditRole):
            return None
        key = self.headers[index.column()]
        value = self._rows[index.row()].get(key, "")
        if key == "flags":
            return f"0x{int(value):02X}"
        return str(value)

    def headerData(self, section: int, orientation: Qt.Orientation, role: int = Qt.DisplayRole):  # type: ignore[override]
        if role != Qt.DisplayRole or orientation != Qt.Horizontal:
            return None
        titles = {
            "cycle": "Cycle",
            "tgw_local_ts_ms": "TGW ts (ms)",
            "batt_status": "Batt",
            "flags": "Flags",
            "soil_mean_raw": "Soil mean",
            "vbat_mean_raw": "VBAT mean",
            "ntc_mean_raw": "NTC mean",
            "rssi": "RSSI",
        }
        key = self.headers[section]
        return titles.get(key, key)

    def update_data(self, rows: List[TelemetryRow]):
        """Replace table content."""
        self.beginResetModel()
        self._rows = rows
        self.endResetModel()
