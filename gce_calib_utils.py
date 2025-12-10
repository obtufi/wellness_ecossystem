"""
Helper functions for basic conversions on RSN raw telemetry.

NOTE: Constants depend on your hardware (divider ratios, ADC reference).
Provide the parameters explicitly to avoid baking wrong numbers.
No automatic/ML logic here; just deterministic helpers.
"""

from __future__ import annotations

def vbat_raw_to_mv(raw: int, *, vref: float, divider_ratio: float, adc_max: int = 4095) -> int:
    """
    Convert a raw ADC reading to millivolts using a provided reference and divider ratio.
    - raw: ADC raw count (0..adc_max)
    - vref: ADC reference voltage in volts (e.g., 3.3)
    - divider_ratio: scale from sensed voltage to battery voltage (e.g., (Rtop+Rbot)/Rbot)
    - adc_max: max ADC count (default 4095 for 12-bit ESP32)

    Returns integer millivolts. Raises ValueError if parameters are missing/invalid.
    """
    if adc_max <= 0:
        raise ValueError("adc_max must be > 0")
    if raw < 0 or raw > adc_max:
        raise ValueError(f"raw out of range: {raw}")
    if divider_ratio <= 0:
        raise ValueError("divider_ratio must be > 0")
    mv = (raw / adc_max) * vref * 1000.0 * divider_ratio
    return int(mv)


def ntc_raw_to_temp_stub(raw: int) -> float:
    """
    Placeholder for NTC conversion. Requires sensor constants (R0, B, divider).
    Keep as stub to avoid hard-coding wrong numbers; fill in when constants are defined.
    """
    raise NotImplementedError("Define NTC constants and implement conversion here.")
