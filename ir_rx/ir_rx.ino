// =============================================================
//  IR Protocol v5.0 — RECEIVER
//  Hardware : Arduino Nano + TS1838 IR receiver module
//
//  PIN MAP
//  -------
//  TS1838 OUT  -> D2  (INT0, hardware interrupt)
//  TS1838 VCC  -> 3.3V or 5V (check your module)
//  TS1838 GND  -> GND
//
//  TSOP / TS1838 signal is ACTIVE LOW (inverted):
//    LOW  = IR carrier present  = MARK
//    HIGH = no IR carrier       = SPACE
//    FALLING edge = start of mark  (end of space)
//    RISING  edge = start of space (end of mark)
// =============================================================

// ── Protocol constants ────────────────────────────────────────
#define MY_ADDRESS      0x01    // accept frames addressed to this

// Timing targets (µs)
#define MARK_US         560
#define SPACE_0_US      560
#define SPACE_1_US      1690
#define START_MARK_US   9000
#define START_SPACE_NEW_US    4500
#define START_SPACE_RPT_US    2250
#define MIN_PRE_GAP_US  5000
#define INTERBIT_TIMEOUT_US   3000

// Tolerance ±25%
#define TOLERANCE       0.25f

// Command labels for Serial output
#define CMD_POWER       0x10
#define CMD_INCREASE    0x11
#define CMD_DECREASE    0x12
#define CMD_MODE_A      0x20
#define CMD_MODE_B      0x21
#define CMD_RESET       0x30
#define CMD_USER1       0x40
#define CMD_USER2       0x41

// Repeat / debounce
#define DEBOUNCE_MS     150

// ── State machine ─────────────────────────────────────────────
typedef enum {
    S_IDLE,
    S_START,
    S_RECEIVING,
    S_VALIDATE
} State;

// ── ISR shared variables (volatile) ──────────────────────────
volatile uint32_t isrPulseWidth  = 0;
volatile bool     isrWasRising   = false;
volatile bool     isrNewPulse    = false;
volatile uint32_t isrLastEdge    = 0;

// ── Receiver state ────────────────────────────────────────────
State    rxState        = S_IDLE;
uint32_t rxBits         = 0;      // shift register for incoming bits
uint8_t  rxBitCount     = 0;
uint32_t rxLastEdge     = 0;      // last edge time used for timeout

// Repeat / dispatch
uint8_t  lastCmd        = 0x00;
uint32_t lastCmdTime    = 0;
bool     hasLastCommand = false;

// ── Frame type ────────────────────────────────────────────────
typedef enum { FRAME_NEW, FRAME_REPEAT, FRAME_INVALID } FrameType;

// =============================================================
//  TOLERANCE HELPER
// =============================================================
bool inRange(uint32_t val, uint32_t target) {
    uint32_t lo = (uint32_t)(target * (1.0f - TOLERANCE));
    uint32_t hi = (uint32_t)(target * (1.0f + TOLERANCE));
    return (val >= lo && val <= hi);
}

// =============================================================
//  CRC-8/MAXIM
// =============================================================
uint8_t crc8_maxim(uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ b) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            b >>= 1;
        }
    }
    return crc;
}

// =============================================================
//  COMMAND NAME LOOKUP
// =============================================================
const __FlashStringHelper* cmdName(uint8_t cmd) {
    switch (cmd) {
        case CMD_POWER:    return F("POWER");
        case CMD_INCREASE: return F("INCREASE");
        case CMD_DECREASE: return F("DECREASE");
        case CMD_MODE_A:   return F("MODE_A");
        case CMD_MODE_B:   return F("MODE_B");
        case CMD_RESET:    return F("RESET");
        case CMD_USER1:    return F("USER1");
        case CMD_USER2:    return F("USER2");
        default:           return F("UNKNOWN");
    }
}

// =============================================================
//  DISPATCH — called when a valid frame is ready
// =============================================================
void dispatchFrame(FrameType type, uint8_t addr, uint8_t cmd) {
    // Address filter
    if (addr != MY_ADDRESS && addr != 0xFF) {
        Serial.print(F("RX ADDR MISMATCH  addr=0x"));
        Serial.println(addr, HEX);
        return;
    }

    if (type == FRAME_REPEAT) {
        // Boot guard — never execute repeat before first valid NEW frame
        if (!hasLastCommand) return;

        Serial.print(F("RX RPT  >> "));
        Serial.println(cmdName(lastCmd));
        // Application action here:
        executeCommand(lastCmd);
        return;
    }

    // Debounce NEW frames only
    uint32_t now = millis();
    if (cmd == lastCmd && (now - lastCmdTime) < DEBOUNCE_MS) {
        Serial.println(F("RX DEBOUNCE (ignored)"));
        return;
    }

    lastCmd        = cmd;
    lastCmdTime    = now;
    hasLastCommand = true;

    Serial.print(F("RX NEW  >> "));
    Serial.print(cmdName(cmd));
    Serial.print(F("  addr=0x"));
    Serial.print(addr, HEX);
    Serial.print(F("  cmd=0x"));
    Serial.print(cmd, HEX);
    Serial.print(F("  crc=OK"));
    Serial.println();

    executeCommand(cmd);
}

// =============================================================
//  EXECUTE COMMAND — add your application logic here
// =============================================================
void executeCommand(uint8_t cmd) {
    // Generic test: nothing to drive, Serial already printed above.
    // Replace this switch with real hardware control when ready.
    switch (cmd) {
        case CMD_POWER:
            // e.g. toggle an LED on pin 13
            // digitalWrite(13, !digitalRead(13));
            break;
        case CMD_INCREASE:
            break;
        case CMD_DECREASE:
            break;
        case CMD_MODE_A:
            break;
        case CMD_MODE_B:
            break;
        case CMD_RESET:
            break;
        case CMD_USER1:
            break;
        case CMD_USER2:
            break;
        default:
            break;
    }
}

// =============================================================
//  RECEIVER RESET
// =============================================================
void resetReceiver() {
    rxState    = S_IDLE;
    rxBits     = 0;
    rxBitCount = 0;
}

// =============================================================
//  ISR — fires on every edge of IR receiver output (D2 / INT0)
// =============================================================
void irISR() {
    uint32_t now = micros();

    // Direct register read — PIND2 = Arduino D2
    // TS1838 is active-low:
    //   HIGH after edge = rising  = end of mark,  start of space
    //   LOW  after edge = falling = end of space, start of mark
    bool rising = (PIND & (1 << PIND2));

    // Unsigned subtraction — overflow-safe through micros() 70-min wrap
    isrPulseWidth  = now - isrLastEdge;
    isrLastEdge    = now;
    isrWasRising   = rising;
    isrNewPulse    = true;
}

// =============================================================
//  PROCESS ONE PULSE — called from loop() when ISR fires
// =============================================================
void processPulse(uint32_t width, bool wasRising) {
    // wasRising = true  means the pin just went HIGH
    //           = end of a MARK; width = mark duration
    // wasRising = false means the pin just went LOW
    //           = end of a SPACE; width = space duration

    switch (rxState) {

        // ── IDLE: waiting for a valid start mark ─────────────
        case S_IDLE: {
            if (!wasRising) return;   // ignore spaces in IDLE

            // Rising edge = end of a mark — check mark duration
            if (!inRange(width, START_MARK_US)) return;

            // Pre-gap check: the space BEFORE this mark must be long enough.
            // isrLastEdge was updated before this call; the space before the
            // mark started is already captured as 'width' on the previous
            // falling edge, but here we simply enforce that we haven't seen
            // a recent non-idle edge. We use the mark width as the proxy:
            // If the state machine just reset cleanly, isrLastEdge gap is long.
            // A full pre-gap guard is handled by the IDLE state itself —
            // we only arrive here after silence long enough for the ISR to
            // have seen a large width for the preceding space.
            // The space preceding the start mark was measured on the
            // falling edge; check that it was ≥ MIN_PRE_GAP_US.
            // We store it in rxLastEdge below on falling edge path.
            if (rxLastEdge < MIN_PRE_GAP_US) {
                // Pre-gap too short — reject this start mark
                resetReceiver();
                return;
            }

            rxState = S_START;
            break;
        }

        // ── START: measure the space after start mark ─────────
        case S_START: {
            if (wasRising) return;    // ignore stray marks

            // Falling edge = end of start space; width = space duration
            FrameType ft;
            if      (inRange(width, START_SPACE_NEW_US)) ft = FRAME_NEW;
            else if (inRange(width, START_SPACE_RPT_US)) ft = FRAME_REPEAT;
            else { resetReceiver(); return; }   // invalid -> IDLE

            if (ft == FRAME_REPEAT) {
                // Repeat frame: execute lastCmd, then back to IDLE.
                // The trailing 560µs mark is handled — we just go IDLE
                // and let the next edges be swallowed silently.
                dispatchFrame(FRAME_REPEAT, MY_ADDRESS, 0);
                resetReceiver();
                return;
            }

            // New frame — start receiving 24 bits
            rxBits     = 0;
            rxBitCount = 0;
            rxState    = S_RECEIVING;
            break;
        }

        // ── RECEIVING: decode 24 bits ─────────────────────────
        case S_RECEIVING: {
            // We only care about falling edges here:
            // falling = end of space, width = space duration -> 0 or 1
            if (wasRising) return;   // skip mark-end edges

            // Dead-zone check: 700–1300 µs is outside all valid windows
            if (width > 700 && width < 1300) {
                Serial.println(F("RX ERR dead-zone -> IDLE"));
                resetReceiver();
                return;
            }

            uint8_t bit;
            if      (inRange(width, SPACE_0_US)) bit = 0;
            else if (inRange(width, SPACE_1_US)) bit = 1;
            else {
                Serial.println(F("RX ERR bad space -> IDLE"));
                resetReceiver();
                return;
            }

            // Shift bit in MSB-first
            rxBits = (rxBits << 1) | bit;
            rxBitCount++;

            if (rxBitCount == 24) {
                // All 24 bits received — validate
                rxState = S_VALIDATE;

                uint8_t addr = (rxBits >> 16) & 0xFF;
                uint8_t cmd  = (rxBits >>  8) & 0xFF;
                uint8_t crc  = (rxBits      ) & 0xFF;

                uint8_t packet[3] = { addr, cmd, crc };
                if (crc8_maxim(packet, 3) == 0x00) {
                    dispatchFrame(FRAME_NEW, addr, cmd);
                } else {
                    Serial.println(F("RX ERR CRC fail -> IDLE"));
                }
                resetReceiver();
            }
            break;
        }

        default:
            resetReceiver();
            break;
    }
}

// =============================================================
//  SETUP
// =============================================================
void setup() {
    Serial.begin(115200);

    pinMode(2, INPUT);   // TS1838 output — no pullup (module has its own)
    attachInterrupt(digitalPinToInterrupt(2), irISR, CHANGE);

    Serial.println(F("=== IR Receiver v5.0 ready ==="));
    Serial.println(F("Listening on D2 (TS1838 OUT)"));
    Serial.println(F("MY_ADDRESS = 0x01"));
}

// =============================================================
//  MAIN LOOP
// =============================================================
void loop() {
    // ── Inter-bit timeout: unstick receiver if frame interrupted
    if (rxState == S_RECEIVING) {
        // Unsigned subtraction — overflow-safe
        if ((micros() - isrLastEdge) > INTERBIT_TIMEOUT_US) {
            Serial.println(F("RX ERR timeout -> IDLE"));
            resetReceiver();
        }
    }

    // ── Process ISR pulse if one arrived ─────────────────────
    if (isrNewPulse) {
        // Snapshot ISR data atomically
        noInterrupts();
        uint32_t w = isrPulseWidth;
        bool     r = isrWasRising;
        isrNewPulse = false;
        interrupts();

        // Store space-before-mark for pre-gap check
        if (!r) {
            // falling edge = start of mark, width was the preceding space
            rxLastEdge = w;   // reused as pre-gap store in IDLE
        }

        processPulse(w, r);
    }
}
