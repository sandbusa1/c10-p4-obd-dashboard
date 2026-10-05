# Driver screen and PCM logging update

Built on working revision 9c60597. `python prepare_source.py` generates the complete `firmware` directory for ESP-IDF 5.5.5.

## Driver and startup

Gear moves to the top; the lower middle oil-pressure pod matches the temperature/voltage reading size. The single gear tile shows confirmed P/R/N, a reported forward gear, or D when selector Drive is known but a numbered gear is not available. No selector strip or assumed first gear is shown. The startup artwork remains for at least four seconds while UI pages build asynchronously. The four-second LVGL animation sweeps out for two seconds and back for two seconds, even without a vehicle. The splash bar is fully filled. The speed blue fill follows the full arc; RPM blue stops at the reference redline boundary.

## Vehicle detection and limits

Adapter/protocol/support discovery remains automatic. Cached protocol is only a connection hint; the current VIN is read again before choosing an OEM gear decoder. No cached VIN chooses another car's OEM query.

| Brand | Gear request | Scope |
| --- | --- | --- |
| Chevy / GM | VPW: 22199A01 at 6C10F1. CAN protocol 6: broadcast 1F5. | User-tested 0411 VPW request; user-captured E38 P/R/N/D. CAN byte 0 low nibble is treated as commanded gear, separately from byte 1 estimated gear; forward-gear decoding still needs a running-vehicle check. |
| Ford | 221E12 at 7E0/7E8, then 7E1/7E9 | Protocol 6 only; cache a successful controller. Forward gears 1–10. Vehicle validation required. |
| BMW | 22D031, 6F1 → 663, extended address 63 | Supported BMW CAN modules. Not a universal E-series/K-line definition. Needs target-vehicle validation. |

Manufacturer detection does not guarantee a particular model implements the selected PID. Unsupported, malformed or failed replies show `-`. Requests back off after three failures. No RPM/speed ratio guess, recommended-gear substitution, or arbitrary cross-manufacturer PID scan is used.

BMW oil pressure uses 22586F at 6F1 → 612 / extended address 12, converting raw/1000 bar to PSI where supported. GM CAN oil pressure uses 221470 at 7E0/7E8, raw byte × 0.578 PSI. The user confirmed 62147002 with the engine off; running-engine validation remains. Ford has no validated pressure definition here and displays `---`; no pressure is inferred from an oil switch or temperature. Oil temperature is never presented as pressure. Newer UDS-only vehicles retain the existing limited UDS discovery path.

BMW explicit flow control uses 6F1 and the module extended-address prefix, then restores automatic flow control. BMW addressing is checked for OK acknowledgements and always restored to standard 7DF addressing. If restoration fails, polling leaves the live loop and reinitializes the adapter.

## Logging

Page 3 → PCM LOGGING opens the separate selector. All 59 decoded channels can be selected across five banks, including RPM, speed, commanded gear, selector position and oil pressure. Adapter voltage is identified separately from PCM module voltage. This is every channel decoded by this firmware, not every proprietary variable inside every PCM.

START creates `/sdcard/pcm_000001.csv` (then the next unused number). No previous CSV is overwritten. Selection is locked during a session and remembered at the next START. STOP or SAVE LOG flushes, synchronizes and closes the CSV, preserving it. CLOSE returns to Page 3 without stopping an active session. The original `/sdcard/obd2.log` event/snapshot logger remains in place.

CSV records are coherent snapshots of the latest decoded values every one second; individual OBD PIDs are polled at different rates and are not simultaneous fresh measurements. Unavailable/offline values are blank. This is not a high-rate tuning logger. SD failures stop recording and appear on screen. Power off before removing the card.

## Definition provenance

- GM query: user's serial test: 22199A rejected; 22199A01 returns 62 19 9A 01/02/03.
- Ford one-byte current/commanded gear: https://www.mustang6g.com/forums/threads/self-made-digital-cluster-need-help-to-find-obd2-pids.154387/
- BMW gear/pressure: https://github.com/OBDb/BMW-3-Series/blob/main/signalsets/v3/default.json
- CAN extended addressing: ELM Electronics ELM327 datasheet, CEA/CRA and extended-address sections: https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf

Screenshots in `previews/` are rendered from the actual generated C using LVGL 9.5 with hardware-only stubs. Their values are simulated; they do not prove vehicle PID support.


## V5 transport details and evidence

GM protocol-6 polls take a bounded 120 ms filtered ATMA listening window for 1F5. They stop the stream and wait for the prompt before restoring CRA/CAF1/H0/S0/7DF and resuming ordinary OBD queries. A stop/restore failure exits live polling and reconnects. All I/O stays in obd_task. Missing/invalid frames clear selector and gear validity; offline states clear pressure too. Unsupported reads back off. No vehicle actuation or coding requests are sent.

- E38/T43 message names and user's actual frame patterns: https://github.com/l77rodeo/gmlan
- E38 LS3/L99 selector/estimated gear and oil definitions: https://github.com/janimm/RealDash-extras/blob/master/RealDash-CAN/XML-files/GM/gm_ls_can.xml
- Ford PCM addressing: https://github.com/meatpiHQ/wican-fw/blob/main/vehicle_profiles/ford/transit.json
- Ford TCM addressing: https://torque-bhp.com/community/main-forum/ford-6-7-diesel-pids/paged/17/

Brand detection selects candidate definitions; it does not mean all models or years support those definitions. Existing automatic protocol discovery and current-VIN selection remain enabled. BMW/Ford support requires target-car testing, and this build does not claim universal oil-pressure coverage.
