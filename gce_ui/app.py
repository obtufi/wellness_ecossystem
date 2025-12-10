"""
Application bootstrap for the GCE dashboard UI.
"""

from __future__ import annotations

import sys

from PySide6.QtWidgets import QApplication

from .main_window import MainWindow


def run() -> int:
    """Create and run the QApplication."""
    app = QApplication(sys.argv)
    app.setApplicationName("GCE Dashboard")
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(run())
