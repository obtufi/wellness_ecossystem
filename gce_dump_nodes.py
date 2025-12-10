from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path


def dump_nodes(db_path: Path):
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    cur.execute(
        """
        SELECT node_id, first_seen, last_seen, last_rssi, hw_version, fw_version, capabilities
        FROM nodes
        ORDER BY node_id
        """
    )
    rows = cur.fetchall()
    if not rows:
        print("No nodes found.")
        return
    for r in rows:
        node_id, first_seen, last_seen, last_rssi, hw, fw, caps = r
        print(f"node_id={node_id} first_seen={first_seen} last_seen={last_seen} rssi={last_rssi} hw={hw} fw={fw} caps=0x{caps:04X}")


def main():
    ap = argparse.ArgumentParser(description="Dump nodes from GCE SQLite DB")
    ap.add_argument("--db", type=Path, default=Path("gce_data.sqlite3"), help="Path to SQLite DB")
    args = ap.parse_args()
    dump_nodes(args.db)


if __name__ == "__main__":
    main()
