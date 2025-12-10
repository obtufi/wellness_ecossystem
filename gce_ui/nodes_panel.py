"""
Panel showing the list of nodes discovered by the TGW/RSN system.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtCore import Signal, QItemSelectionModel
from PySide6.QtWidgets import QAbstractItemView, QHeaderView, QTableView, QVBoxLayout, QWidget

from .controllers import GceBackendController
from .models import NodesTableModel


class NodesPanel(QWidget):
    """Table view for nodes with selection signal."""

    node_selected = Signal(int)

    def __init__(self, controller: GceBackendController, parent: Optional[QWidget] = None):
        super().__init__(parent)
        self._controller = controller
        self._model = NodesTableModel()
        self._table = QTableView(self)
        self._setup_table()
        self._layout_widgets()

    def refresh(self):
        """Fetch latest nodes from store."""
        nodes = self._controller.list_nodes()
        self._model.update_data(nodes)
        self._auto_select_first()

    def _setup_table(self):
        self._table.setModel(self._model)
        self._table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self._table.setSelectionMode(QAbstractItemView.SingleSelection)
        self._table.setAlternatingRowColors(True)
        header = self._table.horizontalHeader()
        header.setSectionResizeMode(QHeaderView.Stretch)
        self._table.verticalHeader().setVisible(False)
        self._table.selectionModel().selectionChanged.connect(self._on_selection_changed)

    def _layout_widgets(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(2, 2, 2, 2)
        layout.addWidget(self._table)
        self.setLayout(layout)

    def _on_selection_changed(self, selected, _deselected):
        if not selected.indexes():
            return
        row = selected.indexes()[0].row()
        node_id = self._model.node_id_at(row)
        if node_id is not None:
            self.node_selected.emit(node_id)

    def _auto_select_first(self):
        """Select first row when nothing is selected to ease navigation."""
        if self._model.rowCount() == 0:
            return
        sel_model = self._table.selectionModel()
        if not sel_model or sel_model.hasSelection():
            return
        first = self._model.index(0, 0)
        sel_model.select(first, QItemSelectionModel.Select | QItemSelectionModel.Rows)
