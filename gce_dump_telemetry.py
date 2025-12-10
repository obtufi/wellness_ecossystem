from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path
from typing import Optional


def dump_telemetry(db_path: Path, node_id: Optional[int], limit: int):
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    base_query = """
        SELECT node_id, ts_host, tgw_ts_ms, rssi,
               soil_mean, soil_median, soil_min, soil_max, soil_std,
               vbat_mean, vbat_median, vbat_min, vbat_max, vbat_std,
               ntc_mean, ntc_median, ntc_min, ntc_max, ntc_std,
               batt_status, flags, last_rssi
        FROM telemetry
    """
    params = []
    if node_id is not None:
        base_query += " WHERE node_id = ?"
        params.append(node_id)
    base_query += " ORDER BY id DESC LIMIT ?"
    params.append(limit)

    cur.execute(base_query, params)
    rows = cur.fetchall()
    if not rows:
        print("No telemetry found.")
        return
    for r in rows:
        (
            nid, ts_host, tgw_ts_ms, rssi,
            soil_mean, soil_median, soil_min, soil_max, soil_std,
            vbat_mean, vbat_median, vbat_min, vbat_max, vbat_std,
            ntc_mean, ntc_median, ntc_min, ntc_max, ntc_std,
            batt_status, flags, last_rssi
        ) = r
        print(
            f"node={nid} ts={ts_host} tgw_ts_ms={tgw_ts_ms} rssi={rssi} "
            f"soil(mean/med/min/max/std)={soil_mean}/{soil_median}/{soil_min}/{soil_max}/{soil_std} "
            f"vbat(mean/med/min/max/std)={vbat_mean}/{vbat_median}/{vbat_min}/{vbat_max}/{vbat_std} "
            f"ntc(mean/med/min/max/std)={ntc_mean}/{ntc_median}/{ntc_min}/{ntc_max}/{ntc_std} "
            f"batt_status={batt_status} flags=0x{flags:02X} last_rssi={last_rssi}"
        )


def main():
    ap = argparse.ArgumentParser(description="Dump telemetry from GCE SQLite DB")
    ap.add_argument("--db", type=Path, default=Path("gce_data.sqlite3"), help="Path to SQLite DB")
    ap.add_argument("--node-id", type=int, help="Filter by node_id")
    ap.add_argument("--limit", type=int, default=20, help="Limit number of rows")
    args = ap.parse_args()
    dump_telemetry(args.db, args.node_id, args.limit)


if __name__ == "__main__":
    main()
