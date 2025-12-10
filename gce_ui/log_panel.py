"""
Simple log console widget.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtWidgets import QTextEdit, QVBoxLayout, QWidget


class LogPanel(QWidget):
    """Append-only text log pane."""

    def __init__(self, parent: Optional[QWidget] = None):
        super().__init__(parent)
        self._text = QTextEdit(self)
        self._text.setReadOnly(True)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(2, 2, 2, 2)
        layout.addWidget(self._text)
        self.setLayout(layout)

    def append_log(self, message: str):
        """Append log line to the console."""
        self._text.append(message)
