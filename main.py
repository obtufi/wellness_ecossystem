from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import structlog

from gce_store import GceStore
from rsn_proto import RsnConfig
from tgw_proto import (
    build_down_config_payload,
    build_down_handshake_payload,
    parse_up_payload,
    UpHelloFrame,
    UpTelemetryFrame,
    UpConfigAckFrame,
)
from tgw_uplink_serial import TgwUplinkSerial, auto_detect_port


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="GCE uplink listener for TGW/RSN")
    parser.add_argument("--port", default="auto", help="Serial port for TGW (default: auto-detect)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baudrate")
    parser.add_argument("--db", type=Path, default=Path("gce_data.sqlite3"), help="SQLite DB path")
    parser.add_argument("--send-config", type=Path, help="JSON file with config to send")
    parser.add_argument("--node-id", type=int, help="Node id for sending config/handshake")
    parser.add_argument("--send-handshake", action="store_true", help="Send handshake before config")
    return parser.parse_args()


def _load_config(path: Path, node_id: int) -> RsnConfig:
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return RsnConfig.from_dict(node_id=node_id, cfg=data)


def main():
    args = _parse_args()
    structlog.configure(processors=[structlog.processors.KeyValueRenderer(key_order=["event"])])
    log = structlog.get_logger()

    port = auto_detect_port() if args.port == "auto" else args.port
    if not port:
        log.error("no-serial-port-found")
        sys.exit(1)

    if args.send_config and args.node_id is None:
        log.error("missing-node-id-for-config")
        sys.exit(1)
    if args.node_id is not None and not args.send_config and not args.send_handshake:
        log.warning("node-id-provided-without-config", node_id=args.node_id)
    if args.node_id is not None and (args.node_id < 1 or args.node_id > 255):
        log.error("node-id-out-of-range", node_id=args.node_id)
        sys.exit(1)

    store = GceStore(args.db, log=log)
    link = TgwUplinkSerial(port, baudrate=args.baud, log=log)

    def on_payload(payload: bytes):
        try:
            frame = parse_up_payload(payload)
        except Exception as exc:
            log.warning("frame-parse-failed", err=str(exc))
            return
        if isinstance(frame, UpHelloFrame):
            log.info("hello-received", node_id=frame.node_id, rssi=frame.rssi)
            store.upsert_node(frame.node_id, frame.rssi, frame.hello)
        elif isinstance(frame, UpTelemetryFrame):
            log.info("telemetry-received", node_id=frame.node_id, rssi=frame.rssi, tgw_ts_ms=frame.tgw_local_ts_ms)
            store.add_telemetry(frame.node_id, frame.rssi, frame.tgw_local_ts_ms, frame.telemetry)
        elif isinstance(frame, UpConfigAckFrame):
            log.info("config-ack-received", node_id=frame.node_id, rssi=frame.rssi, status=frame.ack.status)
            store.add_config_ack(frame.node_id, frame.rssi, frame.ack)
        else:
            log.warning("unknown-frame", type=type(frame).__name__)

    try:
        link.open()
        link.start_reader(on_payload)

        if args.send_config and args.node_id is not None:
            cfg = _load_config(args.send_config, args.node_id)
            if args.send_handshake:
                payload = build_down_handshake_payload(args.node_id)
                try:
                    link.send_payload(payload)
                except Exception as exc:
                    log.error("handshake-send-failed", err=str(exc))
                else:
                    log.info("handshake-sent", node_id=args.node_id)
                time.sleep(0.05)
            payload = build_down_config_payload(args.node_id, cfg)
            try:
                link.send_payload(payload)
            except Exception as exc:
                log.error("config-send-failed", err=str(exc))
            else:
                log.info(
                    "config-sent",
                    node_id=args.node_id,
                    cfg=str(args.send_config),
                    sleep_s=cfg.sleep_time_s,
                    settle_ms=cfg.settling_time_ms,
                    sample_ms=cfg.sampling_interval_ms,
                    lost_rx_limit=cfg.lost_rx_limit,
                    debug_mode=cfg.debug_mode,
                )

        log.info("listening", port=port, baud=args.baud, db=str(args.db))
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        log.info("shutdown")
    finally:
        link.close()
        store.close()


if __name__ == "__main__":
    main()
