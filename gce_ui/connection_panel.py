"""
Connection panel for selecting and toggling the TGW serial link.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QComboBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSpinBox,
    QWidget,
)
from serial.tools import list_ports

from tgw_uplink_serial import auto_detect_port

from .controllers import GceBackendController


class ConnectionPanel(QWidget):
    """Widget that manages serial connection settings and actions."""

    def __init__(self, controller: GceBackendController, parent: Optional[QWidget] = None):
        super().__init__(parent)
        self._controller = controller

        self._port_combo = QComboBox(self)
        self._baud_spin = QSpinBox(self)
        self._connect_btn = QPushButton("Conectar", self)
        self._disconnect_btn = QPushButton("Desconectar", self)
        self._refresh_btn = QPushButton("Atualizar portas", self)
        self._status_label = QLabel("Desconectado", self)

        self._setup_widgets()
        self._layout_widgets()
        self._wire_signals()
        self.refresh_ports()
        self.update_status(False, "Desconectado")

    def refresh_ports(self):
        """Reload available serial ports plus an 'auto' entry."""
        current = self._port_combo.currentText()
        self._port_combo.blockSignals(True)
        self._port_combo.clear()
        self._port_combo.addItem("auto")
        for port in list_ports.comports():
            self._port_combo.addItem(port.device)
        if current:
            idx = self._port_combo.findText(current)
            if idx >= 0:
                self._port_combo.setCurrentIndex(idx)
        self._port_combo.blockSignals(False)

    def update_status(self, connected: bool, message: str):
        """Set status label and colors based on connection state."""
        color = "#2e7d32" if connected else "#c62828"
        self._status_label.setText(message)
        self._status_label.setStyleSheet(f"color: {color}; font-weight: 600;")
        self._set_buttons_enabled(connected)

    def _setup_widgets(self):
        self._baud_spin.setRange(1200, 2_000_000)
        self._baud_spin.setSingleStep(1200)
        self._baud_spin.setValue(115200)
        self._status_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        self._status_label.setStyleSheet("color: #c62828; font-weight: 600;")

    def _layout_widgets(self):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.addWidget(QLabel("Porta:", self))
        layout.addWidget(self._port_combo, stretch=1)
        layout.addWidget(QLabel("Baud:", self))
        layout.addWidget(self._baud_spin)
        layout.addWidget(self._refresh_btn)
        layout.addWidget(self._connect_btn)
        layout.addWidget(self._disconnect_btn)
        layout.addWidget(self._status_label, stretch=1)
        layout.addStretch()
        self.setLayout(layout)

    def _wire_signals(self):
        self._connect_btn.clicked.connect(self._on_connect_clicked)
        self._disconnect_btn.clicked.connect(self._on_disconnect_clicked)
        self._refresh_btn.clicked.connect(self.refresh_ports)

    def _resolve_port(self, selection: str) -> Optional[str]:
        if selection == "auto" or not selection:
            return auto_detect_port()
        return selection

    def _on_connect_clicked(self):
        selected = self._port_combo.currentText()
        port = self._resolve_port(selected)
        baud = int(self._baud_spin.value())
        if not port:
            self.update_status(False, "Porta não encontrada")
            return
        success = self._controller.connect_to_tgw(port, baud)
        if not success:
            self.update_status(False, "Falha ao conectar")

    def _on_disconnect_clicked(self):
        self._controller.disconnect_from_tgw()
        self.update_status(False, "Desconectado")

    def _set_buttons_enabled(self, connected: bool):
        """Toggle connect/disconnect buttons according to link state."""
        self._connect_btn.setEnabled(not connected)
        self._disconnect_btn.setEnabled(True if connected else False)
