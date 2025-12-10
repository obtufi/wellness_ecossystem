"""
Dialog for sending HANDSHAKE/CONFIG to a selected RSN node.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtWidgets import (
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
)

from rsn_proto import RsnConfig

from .controllers import GceBackendController


class ConfigDialog(QDialog):
    """Modal dialog to edit and send configuration for a node."""

    def __init__(self, controller: GceBackendController, node_id: int, parent=None):
        super().__init__(parent)
        self._controller = controller
        self._node_id = node_id
        self.setWindowTitle(f"Configurar nó {node_id}")

        self._sleep = self._make_spin(1, 36000, 300)
        self._pwr_up = self._make_spin(1, 60000, 100)
        self._settle = self._make_spin(1, 60000, 150)
        self._sample = self._make_spin(1, 10000, 50)
        self._led = self._make_spin(0, 255, 0)
        self._batt_bucket = self._make_spin(0, 2, 1)
        self._lost_rx = self._make_spin(0, 20, 3)
        self._debug_mode = self._make_spin(0, 1, 0)
        self._reset_flags = self._make_spin(0, 255, 0)

        self._handshake_btn = QPushButton("Enviar HANDSHAKE", self)
        self._send_btn = QPushButton("Enviar CONFIG", self)
        self._defaults_btn = QPushButton("Restaurar defaults", self)

        self._build_layout()
        self._wire_signals()
        self._load_defaults()

    def _make_spin(self, minimum: int, maximum: int, value: int) -> QSpinBox:
        spin = QSpinBox(self)
        spin.setRange(minimum, maximum)
        spin.setValue(value)
        return spin

    def _build_layout(self):
        form = QFormLayout()
        form.addRow("sleep_time_s", self._sleep)
        form.addRow("pwr_up_time_ms", self._pwr_up)
        form.addRow("settling_time_ms", self._settle)
        form.addRow("sampling_interval_ms", self._sample)
        form.addRow("led_mode_default", self._led)
        form.addRow("batt_bucket", self._batt_bucket)
        form.addRow("lostRx_limit", self._lost_rx)
        form.addRow("debug_mode", self._debug_mode)
        form.addRow("reset_flags", self._reset_flags)

        buttons = QDialogButtonBox(QDialogButtonBox.Cancel, self)
        buttons.rejected.connect(self.reject)

        bottom = QHBoxLayout()
        bottom.addWidget(self._handshake_btn)
        bottom.addWidget(self._send_btn)
        bottom.addWidget(self._defaults_btn)
        bottom.addStretch()
        bottom.addWidget(buttons)

        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addLayout(bottom)
        self.setLayout(layout)

    def _wire_signals(self):
        self._handshake_btn.clicked.connect(self._on_handshake)
        self._send_btn.clicked.connect(self._on_send_config)
        self._defaults_btn.clicked.connect(self._load_defaults)

    def _load_defaults(self):
        cfg = RsnConfig.from_dict(node_id=self._node_id, cfg={})
        self._sleep.setValue(cfg.sleep_time_s)
        self._pwr_up.setValue(cfg.pwr_up_time_ms)
        self._settle.setValue(cfg.settling_time_ms)
        self._sample.setValue(cfg.sampling_interval_ms)
        self._led.setValue(cfg.led_mode_default)
        self._batt_bucket.setValue(cfg.batt_bucket)
        self._lost_rx.setValue(cfg.lost_rx_limit)
        self._debug_mode.setValue(cfg.debug_mode)
        self._reset_flags.setValue(cfg.reset_flags)

    def _current_cfg(self) -> RsnConfig:
        data = {
            "sleep_time_s": self._sleep.value(),
            "pwr_up_time_ms": self._pwr_up.value(),
            "settling_time_ms": self._settle.value(),
            "sampling_interval_ms": self._sample.value(),
            "led_mode_default": self._led.value(),
            "batt_bucket": self._batt_bucket.value(),
            "lost_rx_limit": self._lost_rx.value(),
            "debug_mode": self._debug_mode.value(),
            "reset_flags": self._reset_flags.value(),
        }
        return RsnConfig.from_dict(node_id=self._node_id, cfg=data)

    def _on_handshake(self):
        ok = self._controller.send_handshake(self._node_id)
        if ok:
            QMessageBox.information(self, "Handshake", f"Handshake enviado para nó {self._node_id}")
        else:
            QMessageBox.warning(self, "Handshake", "Falha ao enviar handshake (verifique conexão)")

    def _on_send_config(self):
        try:
            cfg = self._current_cfg()
        except Exception as exc:
            QMessageBox.warning(self, "Config", f"Config inválida: {exc}")
            return
        ok = self._controller.send_config(self._node_id, cfg)
        if ok:
            QMessageBox.information(self, "Config", f"CONFIG enviado para nó {self._node_id}")
        else:
            QMessageBox.warning(self, "Config", "Falha ao enviar CONFIG (verifique conexão)")
