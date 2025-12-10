"""
Lightweight SQLite store for GCE.
Keeps nodes, telemetry, and config acks.
"""

from __future__ import annotations

import sqlite3
from datetime import datetime
from pathlib import Path
from typing import Optional

from rsn_proto import RsnHello, RsnTelemetry, RsnConfigAck


class GceStore:
    def __init__(self, db_path: Path, log=None):
        self.db_path = Path(db_path)
        self._log = log
        self._conn = sqlite3.connect(self.db_path, check_same_thread=False)
        self._conn.execute("PRAGMA journal_mode=WAL;")
        self._create_schema()

    def close(self):
        try:
            self._conn.close()
        except Exception:
            pass

    def _create_schema(self):
        cur = self._conn.cursor()
        cur.executescript(
            """
            CREATE TABLE IF NOT EXISTS nodes (
                node_id INTEGER PRIMARY KEY,
                first_seen TEXT,
                last_seen TEXT,
                last_rssi INTEGER,
                hw_version INTEGER,
                fw_version INTEGER,
                capabilities INTEGER
            );
            CREATE TABLE IF NOT EXISTS telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id INTEGER,
                ts_host TEXT,
                tgw_ts_ms INTEGER,
                rssi INTEGER,
                batt_status INTEGER,
                flags INTEGER,
                soil_mean INTEGER,
                soil_median INTEGER,
                soil_min INTEGER,
                soil_max INTEGER,
                soil_std INTEGER,
                vbat_mean INTEGER,
                vbat_median INTEGER,
                vbat_min INTEGER,
                vbat_max INTEGER,
                vbat_std INTEGER,
                ntc_mean INTEGER,
                ntc_median INTEGER,
                ntc_min INTEGER,
                ntc_max INTEGER,
                ntc_std INTEGER,
                last_rssi INTEGER
            );
            CREATE TABLE IF NOT EXISTS config_acks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id INTEGER,
                ts_host TEXT,
                rssi INTEGER,
                status INTEGER,
                hw_version INTEGER,
                fw_version INTEGER
            );
            """
        )
        self._conn.commit()

    def upsert_node(self, node_id: int, rssi: int, hello: RsnHello):
        now = datetime.utcnow().isoformat()
        cur = self._conn.cursor()
        cur.execute(
            """
            INSERT INTO nodes(node_id, first_seen, last_seen, last_rssi, hw_version, fw_version, capabilities)
            VALUES(?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(node_id) DO UPDATE SET
                last_seen=excluded.last_seen,
                last_rssi=excluded.last_rssi,
                hw_version=excluded.hw_version,
                fw_version=excluded.fw_version,
                capabilities=excluded.capabilities
            """,
            (node_id, now, now, rssi, hello.header.hw_version, hello.header.fw_version, hello.capabilities),
        )
        self._conn.commit()
        if self._log:
            self._log.info("node-upsert", node_id=node_id, rssi=rssi)

    def add_telemetry(self, node_id: int, rssi: int, tgw_ts_ms: int, telemetry: RsnTelemetry):
        now = datetime.utcnow().isoformat()
        cur = self._conn.cursor()
        cur.execute(
            """
            INSERT INTO telemetry (
                node_id, ts_host, tgw_ts_ms, rssi, batt_status, flags,
                soil_mean, soil_median, soil_min, soil_max, soil_std,
                vbat_mean, vbat_median, vbat_min, vbat_max, vbat_std,
                ntc_mean, ntc_median, ntc_min, ntc_max, ntc_std, last_rssi
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                node_id,
                now,
                int(tgw_ts_ms),
                int(rssi),
                int(telemetry.batt_status),
                int(telemetry.flags),
                int(telemetry.soil_mean_raw),
                int(telemetry.soil_median_raw),
                int(telemetry.soil_min_raw),
                int(telemetry.soil_max_raw),
                int(telemetry.soil_std_raw),
                int(telemetry.vbat_mean_raw),
                int(telemetry.vbat_median_raw),
                int(telemetry.vbat_min_raw),
                int(telemetry.vbat_max_raw),
                int(telemetry.vbat_std_raw),
                int(telemetry.ntc_mean_raw),
                int(telemetry.ntc_median_raw),
                int(telemetry.ntc_min_raw),
                int(telemetry.ntc_max_raw),
                int(telemetry.ntc_std_raw),
                int(telemetry.last_rssi),
            ),
        )
        self._conn.commit()
        if self._log:
            self._log.info("telem-insert", node_id=node_id, rssi=rssi)

    def add_config_ack(self, node_id: int, rssi: int, ack: RsnConfigAck):
        now = datetime.utcnow().isoformat()
        cur = self._conn.cursor()
        cur.execute(
            """
            INSERT INTO config_acks(node_id, ts_host, rssi, status, hw_version, fw_version)
            VALUES(?, ?, ?, ?, ?, ?)
            """,
            (node_id, now, int(rssi), int(ack.status), int(ack.header.hw_version), int(ack.header.fw_version)),
        )
        self._conn.commit()
        if self._log:
            self._log.info("config-ack", node_id=node_id, status=ack.status)
