"""
TGW <-> GCE framing helpers.

Framing: [len LSB][len MSB][payload...]
Payload types:
  0xA1 UP_RSN_HELLO:   [type][node_id][rssi][rsn_hello_packet_t]
  0xA2 UP_RSN_TELEM:   [type][node_id][rssi][local_ts_ms(4 LE)][rsn_telemetry_packet_t]
  0xA3 UP_RSN_CONFIG_ACK: [type][node_id][rssi][rsn_config_ack_packet_t]
  0xB1 DOWN_RSN_CONFIG: [type][node_id][rsn_config_packet_t]
  0xB2 DOWN_RSN_HANDSHAKE: [type][node_id][rsn_handshake_packet_t(optional)]
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from enum import IntEnum
from typing import Union

from rsn_proto import (
    RsnConfig,
    RsnConfigAck,
    RsnHello,
    RsnPacketType,
    RsnTelemetry,
    RsnMode,
    HELLO_STRUCT,
    TELEMETRY_STRUCT,
    CONFIG_ACK_STRUCT,
)


class TgwFrameType(IntEnum):
    UP_RSN_HELLO = 0xA1
    UP_RSN_TELEMETRY = 0xA2
    UP_RSN_CONFIG_ACK = 0xA3
    DOWN_RSN_CONFIG = 0xB1
    DOWN_RSN_HANDSHAKE = 0xB2


@dataclass
class UpHelloFrame:
    node_id: int
    rssi: int
    hello: RsnHello


@dataclass
class UpTelemetryFrame:
    node_id: int
    rssi: int
    tgw_local_ts_ms: int
    telemetry: RsnTelemetry


@dataclass
class UpConfigAckFrame:
    node_id: int
    rssi: int
    ack: RsnConfigAck


UpFrame = Union[UpHelloFrame, UpTelemetryFrame, UpConfigAckFrame]


def parse_up_payload(payload: bytes) -> UpFrame:
    if not payload:
        raise ValueError("empty payload")
    msg_type = payload[0]
    if msg_type == TgwFrameType.UP_RSN_HELLO:
        expected = 3 + HELLO_STRUCT.size
        if len(payload) < expected:
            raise ValueError(f"hello frame too short: {len(payload)} < {expected}")
        node_id = payload[1]
        rssi = struct.unpack_from("<b", payload, 2)[0]
        hello = RsnHello.from_bytes(payload[3:3 + HELLO_STRUCT.size])
        return UpHelloFrame(node_id=node_id, rssi=rssi, hello=hello)

    if msg_type == TgwFrameType.UP_RSN_TELEMETRY:
        expected = 3 + 4 + TELEMETRY_STRUCT.size
        if len(payload) < expected:
            raise ValueError(f"telemetry frame too short: {len(payload)} < {expected}")
        node_id = payload[1]
        rssi = struct.unpack_from("<b", payload, 2)[0]
        tgw_ts_ms = struct.unpack_from("<I", payload, 3)[0]
        telemetry = RsnTelemetry.from_bytes(payload[7:7 + TELEMETRY_STRUCT.size])
        return UpTelemetryFrame(node_id=node_id, rssi=rssi, tgw_local_ts_ms=tgw_ts_ms, telemetry=telemetry)

    if msg_type == TgwFrameType.UP_RSN_CONFIG_ACK:
        expected = 3 + CONFIG_ACK_STRUCT.size
        if len(payload) < expected:
            raise ValueError(f"config ack frame too short: {len(payload)} < {expected}")
        node_id = payload[1]
        rssi = struct.unpack_from("<b", payload, 2)[0]
        ack = RsnConfigAck.from_bytes(payload[3:3 + CONFIG_ACK_STRUCT.size])
        return UpConfigAckFrame(node_id=node_id, rssi=rssi, ack=ack)

    raise ValueError(f"unknown frame type 0x{msg_type:02X}")


def build_down_config_payload(node_id: int, cfg: RsnConfig) -> bytes:
    """Builds payload (without length prefix) for DOWN_RSN_CONFIG."""
    return bytes([TgwFrameType.DOWN_RSN_CONFIG, node_id]) + cfg.to_bytes()


def build_down_handshake_payload(node_id: int, hw_version: int = 1, fw_version: int = 1) -> bytes:
    """
    Minimal handshake payload to poke RSN out of pairing.
    This is optional; tgW already sends one before CONFIG.
    """
    header = struct.pack("<BBBBB", RsnPacketType.HANDSHAKE, node_id, RsnMode.RUNNING, hw_version, fw_version)
    return bytes([TgwFrameType.DOWN_RSN_HANDSHAKE, node_id]) + header
