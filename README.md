# arduino-ir-protocol

A custom infrared communication protocol designed for Arduino Nano, built from the physical signal layer up. Includes device addressing, CRC-8/MAXIM error detection, dedicated repeat frame handling, and robust noise rejection. Comes with a complete protocol specification, full tutorials in English and Sinhala, and ready-to-upload transmitter and receiver sketches.

---

## What This Is

Most Arduino IR projects rely on decoding consumer remote protocols (NEC, Sony, RC-5). This project takes a different approach: a fully custom protocol designed specifically for embedded control systems, where both the transmitter and receiver are under your control.

The protocol provides:

- **Device addressing** — up to 254 individually addressable devices on a single IR channel, plus broadcast
- **CRC-8/MAXIM error detection** — corrupted frames are rejected before reaching your command handler
- **Dedicated repeat frames** — button hold events are communicated explicitly, giving your application full control over ramp behaviour
- **Pre-gap noise rejection** — filters false triggers from sunlight, CFL lamps, and motor interference
- **Deterministic failure behaviour** — every error condition has a defined path back to IDLE; no undefined states
- **No libraries required** — pure AVR register-level implementation

---

## Hardware

| Component | Role |
|---|---|
| Arduino Nano (ATmega328P) | Transmitter and receiver MCU |
| TS1838 IR receiver module | Receives and demodulates 38 kHz IR |
| IR LED (TSAL6200 or equivalent) | Transmits the IR signal |
| 8x tactile push buttons | User input on transmitter (D4–D11) |
| 33Ω resistor | IR LED current limiting on D3 |

---

## Wiring

**Transmitter Nano**
```
D3  ---[33Ω]---[IR LED anode | cathode]--- GND
D4  ---[BTN 0]--- GND    (POWER)
D5  ---[BTN 1]--- GND    (INCREASE)
D6  ---[BTN 2]--- GND    (DECREASE)
D7  ---[BTN 3]--- GND    (MODE_A)
D8  ---[BTN 4]--- GND    (MODE_B)
D9  ---[BTN 5]--- GND    (RESET)
D10 ---[BTN 6]--- GND    (USER1)
D11 ---[BTN 7]--- GND    (USER2)
```

**Receiver Nano**
```
TS1838 OUT --- D2
TS1838 VCC --- 5V
TS1838 GND --- GND
```

---

## Quick Start

1. Upload `ir_tx.ino` to the transmitter Nano
2. Upload `ir_rx.ino` to the receiver Nano
3. Open Serial Monitor at **115200 baud** on both
4. Press a button — you should see:

```
TX NEW  >> POWER  (0x10)          ← transmitter
RX NEW  >> POWER  addr=0x01  cmd=0x10  crc=OK   ← receiver
```

To add your own hardware control, edit `executeCommand()` in `ir_rx.ino`. That is the only function you need to modify.

---

## Protocol Summary

```
Frame structure:
  [PRE-GAP  >5 ms silence  ]
  [START MARK   9000 µs    ]
  [START SPACE  4500 µs    ]  ← new command
                OR 2250 µs ]  ← repeat (button held)
  [ADDRESS  8 bits MSB     ]  ← new frames only
  [COMMAND  8 bits MSB     ]  ← new frames only
  [CRC-8    8 bits MSB     ]  ← new frames only
  [END GAP  >10 ms silence ]
```

| Parameter | Value |
|---|---|
| Carrier frequency | ~38 kHz (Timer2 Fast PWM) |
| Carrier duty cycle | 33% |
| Bit encoding | Pulse-Distance Modulation (PDM) |
| Bit 0 space | 560 µs |
| Bit 1 space | 1690 µs |
| Payload | 24 bits (address + command + CRC) |
| Addressing | 8-bit (254 devices + broadcast 0xFF) |
| Error detection | CRC-8/MAXIM (polynomial 0x31 reflected) |

---

## Files

| File | Description |
|---|---|
| `ir_tx.ino` | Transmitter sketch — 8 buttons, D4–D11 |
| `ir_rx.ino` | Receiver sketch — TS1838 on D2 |
| `IR_Protocol_v5.docx` | Full protocol specification (English) |
| `IR_Tutorial.docx` | Complete tutorial — English |
| `IR_Tutorial_Sinhala.docx` | Complete tutorial — සිංහල |

---

## Documents

### Protocol Specification (`IR_Protocol_v5.docx`)
The formal reference document covering all timing values, register configurations, state machine rules, CRC implementation, and design decisions. Written for implementers who need to port the protocol to other platforms or extend it.

### English Tutorial (`IR_Tutorial.docx`)
A complete learning-oriented guide covering everything from IR physics and carrier generation to the receiver state machine and error handling. Written for Arduino-familiar readers who have not worked with custom protocols before. Includes timing diagrams, annotated code snippets, wiring diagrams, and a troubleshooting guide.

### Sinhala Tutorial (`IR_Tutorial_Sinhala.docx`)
සිංහල භාෂාවෙන් ලියූ සම්පූර්ණ නිබන්ධය. ඉංග්‍රීසි tutorial (නිබන්ධය) ලෙසම sections (කොටස්), diagrams (රූප සටහන්), සහ code examples (කේත නිදර්ශන) ඇතුළත් ය. Technical terms (තාක්ෂණික පද) ඉංග්‍රීසි ලෙස රඳවා, Sinhala explanation (සිංහල පැහැදිලි කිරීම) සහිතව ය.

---

## Protocol Version History

| Version | Key Changes |
|---|---|
| v1.0 | Initial design — address, command, CRC-8 |
| v2.0 | PDM encoding, Timer2 PWM, repeat frames, tolerance windows |
| v3.0 | Pre-gap validation, inter-bit timeout, debounce fix, ISR edge capture |
| v4.0 | Boot guard, error sink rule, dead zone, MSB/LSB clarification |
| v5.0 | Timer2 Fast PWM fix, carrier_off() glitch fix, direct port manipulation, active-low documentation, state table clarification |

---

## License

MIT License — free to use, modify, and distribute with attribution.

---

## Author's Note

This protocol was developed iteratively through five revision cycles, incorporating feedback on hardware bugs, timing correctness, noise immunity, and embedded coding practice. The specification and tutorials document not just the implementation but the reasoning behind every design decision, making it suitable as a learning reference as well as a practical starting point.
