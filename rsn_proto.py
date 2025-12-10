"""
RSN/TGW shared packet definitions for Python.

This mirrors the packed C structs used on RSN/TGW (little-endian).
Keep fields ordered exactly as in tgw_constants.h/constants.h.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from enum import IntEnum
from typing import Any, Dict

RSN_MAX_PACKET_SIZE = 128


class RsnPacketType(IntEnum):
    HELLO = 0x01
    HANDSHAKE = 0x02
    TELEMETRY = 0x03
    CONFIG = 0x04
    CONFIG_ACK = 0x05
    DEBUG = 0x06


class RsnMode(IntEnum):
    RUNNING = 0
    PAIRING = 1
    DEBUG = 2


# Struct layouts (little-endian, packed)
HEADER_STRUCT = struct.Struct("<BBBBB")  # type, node_id, mode, hw, fw
HELLO_STRUCT = struct.Struct("<BBBBBH")  # header + capabilities
CONFIG_STRUCT = struct.Struct("<BBBBBHHHHBBBBB")

_telemetry_format = "<BBBBBII" + "BB" + ("H" * 15) + "b"
TELEMETRY_STRUCT = struct.Struct(_telemetry_format)

CONFIG_ACK_STRUCT = struct.Struct("<BBBBBB")


@dataclass
class RsnHeader:
    pkt_type: int
    node_id: int
    mode: int
    hw_version: int
    fw_version: int

    @classmethod
    def from_bytes(cls, data: bytes) -> "RsnHeader":
        if len(data) < HEADER_STRUCT.size:
            raise ValueError("header too short")
        return cls(*HEADER_STRUCT.unpack_from(data))


@dataclass
class RsnHello:
    header: RsnHeader
    capabilities: int

    @classmethod
    def from_bytes(cls, data: bytes) -> "RsnHello":
        if len(data) < HELLO_STRUCT.size:
            raise ValueError("hello payload too short")
        raw = HELLO_STRUCT.unpack_from(data)
        header = RsnHeader(*raw[:5])
        capabilities = raw[5]
        return cls(header=header, capabilities=capabilities)


@dataclass
class RsnConfig:
    header: RsnHeader
    sleep_time_s: int
    pwr_up_time_ms: int
    settling_time_ms: int
    sampling_interval_ms: int
    led_mode_default: int
    batt_bucket: int
    lost_rx_limit: int
    debug_mode: int
    reset_flags: int

    @classmethod
    def from_bytes(cls, data: bytes) -> "RsnConfig":
        if len(data) < CONFIG_STRUCT.size:
            raise ValueError("config payload too short")
        raw = CONFIG_STRUCT.unpack_from(data)
        header = RsnHeader(*raw[:5])
        sleep_time_s, pwr_up_time_ms, settling_time_ms, sampling_interval_ms = raw[5:9]
        led_mode_default, batt_bucket, lost_rx_limit, debug_mode, reset_flags = raw[9:14]
        return cls(
            header=header,
            sleep_time_s=sleep_time_s,
            pwr_up_time_ms=pwr_up_time_ms,
            settling_time_ms=settling_time_ms,
            sampling_interval_ms=sampling_interval_ms,
            led_mode_default=led_mode_default,
            batt_bucket=batt_bucket,
            lost_rx_limit=lost_rx_limit,
            debug_mode=debug_mode,
            reset_flags=reset_flags,
        )

    def to_bytes(self) -> bytes:
        return CONFIG_STRUCT.pack(
            self.header.pkt_type,
            self.header.node_id,
            self.header.mode,
            self.header.hw_version,
            self.header.fw_version,
            self.sleep_time_s,
            self.pwr_up_time_ms,
            self.settling_time_ms,
            self.sampling_interval_ms,
            self.led_mode_default,
            self.batt_bucket,
            self.lost_rx_limit,
            self.debug_mode,
            self.reset_flags,
        )

    @classmethod
    def from_dict(cls, node_id: int, cfg: Dict[str, Any], *, hw_version: int = 1, fw_version: int = 1) -> "RsnConfig":
        header = RsnHeader(RsnPacketType.CONFIG, node_id, RsnMode.RUNNING, hw_version, fw_version)
        return cls(
            header=header,
            sleep_time_s=int(cfg.get("sleep_time_s", 300)),
            pwr_up_time_ms=int(cfg.get("pwr_up_time_ms", 100)),
            settling_time_ms=int(cfg.get("settling_time_ms", 150)),
            sampling_interval_ms=int(cfg.get("sampling_interval_ms", 50)),
            led_mode_default=int(cfg.get("led_mode_default", 0)),
            batt_bucket=int(cfg.get("batt_bucket", 1)),
            lost_rx_limit=int(cfg.get("lost_rx_limit", 3)),
            debug_mode=int(cfg.get("debug_mode", 0)),
            reset_flags=int(cfg.get("reset_flags", 0)),
        )


@dataclass
class RsnTelemetry:
    header: RsnHeader
    cycle: int
    ts_ms: int
    batt_status: int
    flags: int
    soil_mean_raw: int
    soil_median_raw: int
    soil_min_raw: int
    soil_max_raw: int
    soil_std_raw: int
    vbat_mean_raw: int
    vbat_median_raw: int
    vbat_min_raw: int
    vbat_max_raw: int
    vbat_std_raw: int
    ntc_mean_raw: int
    ntc_median_raw: int
    ntc_min_raw: int
    ntc_max_raw: int
    ntc_std_raw: int
    last_rssi: int

    @classmethod
    def from_bytes(cls, data: bytes) -> "RsnTelemetry":
        if len(data) < TELEMETRY_STRUCT.size:
            raise ValueError("telemetry payload too short")
        raw = TELEMETRY_STRUCT.unpack_from(data)
        header = RsnHeader(*raw[:5])
        cycle = raw[5]
        ts_ms = raw[6]
        batt_status = raw[7]
        flags = raw[8]
        soil_mean_raw, soil_median_raw, soil_min_raw, soil_max_raw, soil_std_raw = raw[9:14]
        vbat_mean_raw, vbat_median_raw, vbat_min_raw, vbat_max_raw, vbat_std_raw = raw[14:19]
        ntc_mean_raw, ntc_median_raw, ntc_min_raw, ntc_max_raw, ntc_std_raw = raw[19:24]
        last_rssi = raw[24]
        return cls(
            header=header,
            cycle=cycle,
            ts_ms=ts_ms,
            batt_status=batt_status,
            flags=flags,
            soil_mean_raw=soil_mean_raw,
            soil_median_raw=soil_median_raw,
            soil_min_raw=soil_min_raw,
            soil_max_raw=soil_max_raw,
            soil_std_raw=soil_std_raw,
            vbat_mean_raw=vbat_mean_raw,
            vbat_median_raw=vbat_median_raw,
            vbat_min_raw=vbat_min_raw,
            vbat_max_raw=vbat_max_raw,
            vbat_std_raw=vbat_std_raw,
            ntc_mean_raw=ntc_mean_raw,
            ntc_median_raw=ntc_median_raw,
            ntc_min_raw=ntc_min_raw,
            ntc_max_raw=ntc_max_raw,
            ntc_std_raw=ntc_std_raw,
            last_rssi=last_rssi,
        )


@dataclass
class RsnConfigAck:
    header: RsnHeader
    status: int

    @classmethod
    def from_bytes(cls, data: bytes) -> "RsnConfigAck":
        if len(data) < CONFIG_ACK_STRUCT.size:
            raise ValueError("config ack payload too short")
        raw = CONFIG_ACK_STRUCT.unpack_from(data)
        header = RsnHeader(*raw[:5])
        status = raw[5]
        return cls(header=header, status=status)
