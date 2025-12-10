"""
Main window composition for the GCE Dashboard.
"""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtGui import QAction
from PySide6.QtWidgets import QHBoxLayout, QPushButton, QMainWindow, QMessageBox, QSplitter, QVBoxLayout, QWidget

from .connection_panel import ConnectionPanel
from .controllers import GceBackendController
from .config_panel import ConfigDialog
from .log_panel import LogPanel
from .nodes_panel import NodesPanel
from .telemetry_panel import TelemetryPanel


class MainWindow(QMainWindow):
    """Top-level window with connection, nodes, telemetry, and log panels."""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("GCE Dashboard – RSN/TGW Monitor")

        self._controller = GceBackendController()
        self._connection_panel = ConnectionPanel(self._controller)
        self._nodes_panel = NodesPanel(self._controller)
        self._telemetry_panel = TelemetryPanel(self._controller)
        self._log_panel = LogPanel()
        self._config_btn = QPushButton("Configurar nó...", self)
        self._config_btn.setEnabled(False)
        self._current_node_id = None

        self._build_menu()
        self._wire_signals()
        self._build_layout()

        self._nodes_panel.refresh()

    def closeEvent(self, event):  # type: ignore[override]
        self._controller.shutdown()
        super().closeEvent(event)

    def _build_menu(self):
        menu_bar = self.menuBar()

        file_menu = menu_bar.addMenu("Arquivo")
        exit_action = QAction("Sair", self)
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)

        help_menu = menu_bar.addMenu("Ajuda")
        about_action = QAction("Sobre", self)
        about_action.triggered.connect(self._show_about)
        help_menu.addAction(about_action)

    def _build_layout(self):
        central = QWidget(self)
        layout = QVBoxLayout(central)
        top = QHBoxLayout()
        top.addWidget(self._connection_panel, stretch=1)
        top.addStretch()
        top.addWidget(self._config_btn)
        layout.addLayout(top)

        vertical_split = QSplitter(Qt.Vertical, central)
        horizontal_split = QSplitter(Qt.Horizontal, vertical_split)
        horizontal_split.addWidget(self._nodes_panel)
        horizontal_split.addWidget(self._telemetry_panel)
        horizontal_split.setStretchFactor(0, 1)
        horizontal_split.setStretchFactor(1, 1)

        vertical_split.addWidget(horizontal_split)
        vertical_split.addWidget(self._log_panel)
        vertical_split.setStretchFactor(0, 3)
        vertical_split.setStretchFactor(1, 1)

        layout.addWidget(vertical_split)
        self.setCentralWidget(central)

    def _wire_signals(self):
        self._nodes_panel.node_selected.connect(self._telemetry_panel.set_node)
        self._controller.node_updated.connect(self._handle_node_updated)
        self._controller.telemetry_updated.connect(self._handle_telemetry_updated)
        self._controller.log_message.connect(self._log_panel.append_log)
        self._controller.connection_state_changed.connect(self._connection_panel.update_status)
        self._config_btn.clicked.connect(self._open_config_dialog)

    def _handle_node_updated(self, node_id: int):
        self._nodes_panel.refresh()
        if self._telemetry_panel.current_node_id == node_id:
            self._telemetry_panel.refresh()

    def _handle_telemetry_updated(self, node_id: int):
        if self._telemetry_panel.current_node_id == node_id:
            self._telemetry_panel.refresh()
        self._nodes_panel.refresh()
        self._current_node_id = node_id
        self._update_config_btn_state()

    def _show_about(self):
        QMessageBox.information(
            self,
            "Sobre",
            "GCE Dashboard\nMonitoramento RSN/TGW em PySide6.",
        )

    def _update_config_btn_state(self):
        self._config_btn.setEnabled(self._nodes_panel.current_node_id is not None)

    def _open_config_dialog(self):
        node_id = self._nodes_panel.current_node_id
        if node_id is None:
            QMessageBox.information(self, "Config", "Selecione um nó primeiro.")
            return
        dlg = ConfigDialog(self._controller, node_id, self)
        dlg.exec()
