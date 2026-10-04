# Driver screen and PCM logging update

Built on working revision 9c60597. `python prepare_source.py` generates the complete `firmware` directory for ESP-IDF 5.5.5.

## Driver and startup

Gear moves to the top; the lower middle oil-pressure pod matches the temperature/voltage reading size. Only the present forward gear is shown. No selector strip or assumed P/R/N is shown. The startup artwork remains for at least four seconds while UI pages build asynchronously. The four-second LVGL animation sweeps out for two seconds and back for two seconds, even without a vehicle. The splash bar is fully filled. The speed blue fill follows the full arc; RPM blue stops at the reference redline boundary.

## Vehicle detection and limits

Adapter/protocol/support discovery remains automatic. Cached protocol is only a connection hint; the current VIN is read again before choosing an OEM gear decoder. No cached VIN chooses another car's OEM query.

| Brand | Gear request | Scope |
| --- | --- | --- |
| Chevy / GM | 22199A01 on VPW with header 6C10F1; 22199A otherwise | Uses the C10's vehicle-tested VPW request and restores 686AF1 afterward; accepts forward gears 1–10 when returned. |
| Ford | 221E12, one-byte gear | Supported Ford PCM definitions; accepts forward gears 1–10. Needs validation on the target vehicle. |
| BMW | 22D031, 6F1 → 663, extended address 63 | Supported BMW CAN modules. Not a universal E-series/K-line definition. Needs target-vehicle validation. |

Manufacturer detection does not guarantee a particular model implements the selected PID. Unsupported, malformed or failed replies show `-`. Requests back off after three failures. No RPM/speed ratio guess, recommended-gear substitution, or arbitrary cross-manufacturer PID scan is used.

BMW oil pressure uses 22586F at 6F1 → 612 / extended address 12, converting raw/1000 bar to PSI where supported. Chevy and Ford oil pressure have **no validated query in this source** and show `---` unless the existing `obd_auto_set_oil_pressure()` integration supplies valid pressure. Oil temperature is never presented as pressure. Newer UDS-only vehicles retain the existing limited UDS discovery path.

BMW addressing is checked for OK acknowledgements and always restored to standard 7DF addressing. If restoration fails, polling leaves the live loop and reinitializes the adapter.

## Logging

Page 3 → PCM LOGGING opens the separate selector. All 58 decoded channels can be selected across five banks, including RPM, speed, gear and oil pressure. Adapter voltage is identified separately from PCM module voltage. This is every channel decoded by this firmware, not every proprietary variable inside every PCM.

START creates `/sdcard/pcm_000001.csv` (then the next unused number). No previous CSV is overwritten. Selection is locked during a session and remembered at the next START. STOP or SAVE LOG flushes, synchronizes and closes the CSV, preserving it. CLOSE returns to Page 3 without stopping an active session. The original `/sdcard/obd2.log` event/snapshot logger remains in place.

CSV records are coherent snapshots of the latest decoded values every one second; individual OBD PIDs are polled at different rates and are not simultaneous fresh measurements. Unavailable/offline values are blank. This is not a high-rate tuning logger. SD failures stop recording and appear on screen. Power off before removing the card.

## Definition provenance

- GM query: user's serial test: 22199A rejected; 22199A01 returns 62 19 9A 01/02/03.
- Ford one-byte current/commanded gear: https://www.mustang6g.com/forums/threads/self-made-digital-cluster-need-help-to-find-obd2-pids.154387/
- BMW gear/pressure: https://github.com/OBDb/BMW-3-Series/blob/main/signalsets/v3/default.json
- CAN extended addressing: ELM Electronics ELM327 datasheet, CEA/CRA and extended-address sections: https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf

Screenshots in `previews/` are rendered from the actual generated C using LVGL 9.5 with hardware-only stubs. Their values are simulated; they do not prove vehicle PID support.

