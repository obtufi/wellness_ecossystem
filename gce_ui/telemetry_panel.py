"""
Panel that shows telemetry entries for the selected node.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtWidgets import QAbstractItemView, QLabel, QTableView, QVBoxLayout, QWidget, QHeaderView

from .controllers import GceBackendController
from .models import TelemetryTableModel


class TelemetryPanel(QWidget):
    """Table view for telemetry entries of a single node."""

    def __init__(self, controller: GceBackendController, parent: Optional[QWidget] = None):
        super().__init__(parent)
        self._controller = controller
        self._model = TelemetryTableModel()
        self._table = QTableView(self)
        self._no_selection_label = QLabel("Selecione um nó para ver telemetria", self)
        self._current_node: Optional[int] = None

        self._setup_table()
        self._layout_widgets()

    @property
    def current_node_id(self) -> Optional[int]:
        """Currently selected node id."""
        return self._current_node

    def set_node(self, node_id: int):
        """Set active node and refresh telemetry list."""
        self._current_node = node_id
        self.refresh()

    def refresh(self):
        """Reload telemetry for the selected node."""
        if self._current_node is None:
            self._model.update_data([])
            self._no_selection_label.show()
            self._table.hide()
            return
        rows = self._controller.list_recent_telemetry(self._current_node)
        self._model.update_data(rows)
        self._no_selection_label.hide()
        self._table.show()

    def _setup_table(self):
        self._table.setModel(self._model)
        self._table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self._table.setSelectionMode(QAbstractItemView.NoSelection)
        self._table.setAlternatingRowColors(True)
        header = self._table.horizontalHeader()
        header.setSectionResizeMode(QHeaderView.Stretch)
        self._table.verticalHeader().setVisible(False)
        self._table.hide()

    def _layout_widgets(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(2, 2, 2, 2)
        layout.addWidget(self._no_selection_label)
        layout.addWidget(self._table)
        self.setLayout(layout)
