from __future__ import annotations

from datetime import datetime, timedelta
from typing import List, Dict

import numpy as np
from PySide6 import QtWidgets
import pyqtgraph as pg

import gce_calib
from gce_model import TelemetryRecord

CALIB_LIMITS = {
    "soil": (0.0, 100.0),
    "vbat": (3.0, 4.3),
    "ntc": (0.0, 50.0),
}


class SensorDetailChart(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.plot = pg.PlotWidget(title="Detalhe do sensor")
        self.plot.showGrid(x=True, y=True)
        self.plot.addLegend()
        self.curve_mean = self.plot.plot([], [], pen=pg.mkPen(color="c", width=2), name="mean")
        self.curve_med = self.plot.plot([], [], pen=pg.mkPen(color="m", width=1, style=pg.QtCore.Qt.DashLine), name="median")
        self.curve_std_top = self.plot.plot([], [], pen=pg.mkPen(color="y", width=1, style=pg.QtCore.Qt.DotLine), name="+std")
        self.curve_std_bot = self.plot.plot([], [], pen=pg.mkPen(color="y", width=1, style=pg.QtCore.Qt.DotLine), name="-std")
        self.fill_minmax = pg.FillBetweenItem(self.curve_std_bot, self.curve_std_top, brush=(255, 255, 0, 50))
        self.plot.addItem(self.fill_minmax)
        self.info_text = pg.TextItem("", anchor=(0, 1))
        self.plot.addItem(self.info_text)
        self.plot.scene().sigMouseMoved.connect(self._on_mouse_move)

        # buffers para tooltip
        self._x_rel = np.array([], dtype=float)
        self._mean_arr = np.array([], dtype=float)
        self._med_arr = np.array([], dtype=float)
        self._std_arr = np.array([], dtype=float)
        self._min_arr = np.array([], dtype=float)
        self._max_arr = np.array([], dtype=float)

        self.node_combo = QtWidgets.QComboBox()
        self.sensor_combo = QtWidgets.QComboBox()
        self.sensor_combo.addItems(["soil", "vbat", "ntc"])
        self.window_combo = QtWidgets.QComboBox()
        self.window_combo.addItems(["10m", "1h", "6h", "24h", "7d"])
        self.window_combo.setCurrentIndex(3)
        self.calib_checkbox = QtWidgets.QCheckBox("Aplicar calibração")
        self.calib_checkbox.setChecked(True)
        self.y_scale_spin = QtWidgets.QDoubleSpinBox()
        self.y_scale_spin.setRange(0.1, 10.0)
        self.y_scale_spin.setSingleStep(0.1)
        self.y_scale_spin.setValue(1.0)

        form = QtWidgets.QFormLayout()
        form.addRow("Node:", self.node_combo)
        form.addRow("Sensor:", self.sensor_combo)
        form.addRow("Janela:", self.window_combo)
        form.addRow("", self.calib_checkbox)
        form.addRow("Escala Y:", self.y_scale_spin)

        layout = QtWidgets.QVBoxLayout(self)
        layout.addLayout(form)
        layout.addWidget(self.plot, stretch=1)

    def set_nodes(self, nodes: List[int]):
        current = self.node_combo.currentText()
        try:
            self.node_combo.blockSignals(True)
            self.node_combo.clear()
            for n in sorted(nodes):
                self.node_combo.addItem(str(n))
            if current:
                idx = self.node_combo.findText(current)
                if idx >= 0:
                    self.node_combo.setCurrentIndex(idx)
        finally:
            self.node_combo.blockSignals(False)

    def refresh(self, history: Dict[int, List[TelemetryRecord]], calib_data, default_node: int | None = None, calib_limits=None, raw_limits=(0, 4095)):
        if self.node_combo.count() == 0 and default_node is not None:
            self.node_combo.addItem(str(default_node))
        if not self.node_combo.currentText():
            self.curve_mean.setData([], [])
            self.curve_med.setData([], [])
            self.curve_std_top.setData([], [])
            self.curve_std_bot.setData([], [])
            return

        try:
            node_id = int(self.node_combo.currentText())
        except ValueError:
            return
        sensor = self.sensor_combo.currentText()
        metric = f"{sensor}_mean"
        metric_min = f"{sensor}_min"
        metric_max = f"{sensor}_max"
        metric_std = f"{sensor}_std"
        metric_med = f"{sensor}_median"

        window = self.window_combo.currentText()
        delta = {
            "10m": timedelta(minutes=10),
            "1h": timedelta(hours=1),
            "6h": timedelta(hours=6),
            "24h": timedelta(hours=24),
            "7d": timedelta(days=7),
        }.get(window, timedelta(hours=24))
        cutoff = datetime.now() - delta
        apply_calib = self.calib_checkbox.isChecked()
        calib_metric = metric in {"soil_mean", "vbat_mean", "ntc_mean"}
        calib_slope = None
        if apply_calib and calib_metric:
            try:
                calib_slope, _ = gce_calib.get_coeff(calib_data, node_id, metric)
                calib_slope = abs(float(calib_slope))
            except Exception:
                calib_slope = None

        xs = []
        mean_vals = []
        med_vals = []
        std_top = []
        std_bot = []
        min_list = []
        max_list = []
        std_vals = []
        for rec in history.get(node_id, []):
            if rec.ts < cutoff:
                continue
            m = getattr(rec, metric, None)
            if m is None:
                continue
            try:
                val_mean = float(m)
            except Exception:
                continue
            if apply_calib and calib_metric:
                val_mean = gce_calib.apply_calibration(calib_data, node_id, metric, val_mean)
            xs.append(rec.ts)
            mean_vals.append(val_mean)

            med = getattr(rec, metric_med, None)
            if med is not None:
                try:
                    val_med = float(med)
                    if apply_calib and calib_metric:
                        val_med = gce_calib.apply_calibration(calib_data, node_id, metric, val_med)
                    med_vals.append(val_med)
                except Exception:
                    med_vals.append(np.nan)
            else:
                med_vals.append(np.nan)

            stdv = getattr(rec, metric_std, None)
            if stdv is not None:
                try:
                    val_std = float(stdv)
                    if apply_calib and calib_metric and calib_slope is not None:
                        val_std = calib_slope * val_std
                    top = val_mean + val_std
                    bot = val_mean - val_std
                    std_top.append(top)
                    std_bot.append(bot)
                    std_vals.append(val_std)
                except Exception:
                    std_top.append(np.nan)
                    std_bot.append(np.nan)
                    std_vals.append(np.nan)
            else:
                std_top.append(np.nan)
                std_bot.append(np.nan)
                std_vals.append(np.nan)

            # min/max
            mn = getattr(rec, metric_min, None)
            mx = getattr(rec, metric_max, None)
            try:
                v_min = float(mn) if mn is not None else np.nan
                if apply_calib and calib_metric and v_min == v_min:
                    v_min = gce_calib.apply_calibration(calib_data, node_id, metric, v_min)
                min_list.append(v_min)
            except Exception:
                min_list.append(np.nan)
            try:
                v_max = float(mx) if mx is not None else np.nan
                if apply_calib and calib_metric and v_max == v_max:
                    v_max = gce_calib.apply_calibration(calib_data, node_id, metric, v_max)
                max_list.append(v_max)
            except Exception:
                max_list.append(np.nan)

        if not xs:
            self.curve_mean.setData([], [])
            self.curve_med.setData([], [])
            self.curve_std_top.setData([], [])
            self.curve_std_bot.setData([], [])
            return

        t0 = xs[0].timestamp()
        x_rel = np.asarray([(x.timestamp() - t0) / 60.0 for x in xs], dtype=float)
        mean_arr = np.asarray(mean_vals, dtype=float)
        med_arr = np.asarray(med_vals, dtype=float)
        top_arr = np.asarray(std_top, dtype=float)
        bot_arr = np.asarray(std_bot, dtype=float)
        min_arr = np.asarray(min_list, dtype=float)
        max_arr = np.asarray(max_list, dtype=float)
        std_arr = np.asarray(std_vals, dtype=float)

        self.curve_mean.setData(x_rel, mean_arr)
        self.curve_med.setData(x_rel, med_arr)
        self.curve_std_top.setData(x_rel, top_arr)
        self.curve_std_bot.setData(x_rel, bot_arr)
        self.plot.setLabel("bottom", "tempo", "min")

        self._x_rel = x_rel
        self._mean_arr = mean_arr
        self._med_arr = med_arr
        self._std_arr = std_arr
        self._min_arr = min_arr
        self._max_arr = max_arr

        # autoajuste de escala
        y_candidates = np.concatenate(
            [arr for arr in (mean_arr, med_arr, top_arr, bot_arr, min_arr, max_arr) if arr.size > 0]
        )
        y_finite = y_candidates[np.isfinite(y_candidates)]
        y_range = None
        selected_limits = None
        if apply_calib:
            if calib_limits and len(calib_limits) == 2:
                try:
                    selected_limits = (float(calib_limits[0]), float(calib_limits[1]))
                except Exception:
                    selected_limits = None
            if selected_limits is None:
                selected_limits = CALIB_LIMITS.get(sensor)
        if not apply_calib:
            y_range = raw_limits
        elif selected_limits:
            y_min_lim, y_max_lim = selected_limits
            if y_finite.size > 0:
                data_min = float(y_finite.min())
                data_max = float(y_finite.max())
                pad = (data_max - data_min) * 0.1 if data_max != data_min else 0.5
                pad *= self.y_scale_spin.value()
                data_low = data_min - pad
                data_high = data_max + pad
                y_low = max(y_min_lim, data_low)
                y_high = min(y_max_lim, data_high)
                y_range = (y_min_lim, y_max_lim) if y_low >= y_high else (y_low, y_high)
            else:
                y_range = (y_min_lim, y_max_lim)
        elif y_finite.size > 0:
            y_min = float(y_finite.min())
            y_max = float(y_finite.max())
            pad = (y_max - y_min) * 0.1 if y_max != y_min else 1.0
            pad *= self.y_scale_spin.value()
            y_min -= pad
            y_max += pad
            y_range = (y_min, y_max)
        else:
            y_range = raw_limits
        self.plot.setRange(
            xRange=(x_rel.min(), x_rel.max()),
            yRange=y_range,
            padding=0,
        )

    def _on_mouse_move(self, evt):
        if not hasattr(self, "_x_rel"):
            return
        pos = evt
        if self.plot.sceneBoundingRect().contains(pos):
            mouse_point = self.plot.plotItem.vb.mapSceneToView(pos)
            x = mouse_point.x()
            if self._x_rel.size == 0:
                return
            idx = int(np.abs(self._x_rel - x).argmin())
            try:
                mean_val = self._mean_arr[idx]
            except Exception:
                return
            med_val = self._med_arr[idx] if idx < self._med_arr.size else np.nan
            std_val = self._std_arr[idx] if idx < self._std_arr.size else np.nan
            min_val = self._min_arr[idx] if idx < self._min_arr.size else np.nan
            max_val = self._max_arr[idx] if idx < self._max_arr.size else np.nan

            def fmt(v):
                return f"{v:.2f}" if np.isfinite(v) else "-"

            self.info_text.setText(
                f"x={x:.2f} min\nmean={fmt(mean_val)}\nmedian={fmt(med_val)}\nstd={fmt(std_val)}\nmin={fmt(min_val)} max={fmt(max_val)}"
            )
            self.info_text.setPos(self._x_rel[idx], mean_val)
