// =============================================================
//  IR Protocol v5.0 — TRANSMITTER
//  Hardware : Arduino Nano + IR LED (TSAL6200 or equiv)
//             IR LED + 33ohm resistor on Pin 3 (OC2B)
//  Buttons  : 8x tactile buttons on D4–D11, active LOW
//             (pin -> GND, INPUT_PULLUP)
// =============================================================
//
//  PIN MAP
//  -------
//  D3  -> IR LED anode (via 33Ω resistor) -> LED cathode -> GND
//  D4  -> Button 0 (POWER TOGGLE)   -> GND
//  D5  -> Button 1 (INCREASE)        -> GND
//  D6  -> Button 2 (DECREASE)        -> GND
//  D7  -> Button 3 (MODE A)          -> GND
//  D8  -> Button 4 (MODE B)          -> GND
//  D9  -> Button 5 (RESET)           -> GND
//  D10 -> Button 6 (USER 1)          -> GND
//  D11 -> Button 7 (USER 2)          -> GND
//
// =============================================================

// ── Protocol constants ────────────────────────────────────────
#define MY_ADDRESS      0x01    // this transmitter's address

#define CMD_POWER       0x10
#define CMD_INCREASE    0x11
#define CMD_DECREASE    0x12
#define CMD_MODE_A      0x20
#define CMD_MODE_B      0x21
#define CMD_RESET       0x30
#define CMD_USER1       0x40
#define CMD_USER2       0x41

// Timing (µs)
#define MARK_US         560
#define SPACE_0_US      560
#define SPACE_1_US      1690
#define START_MARK_US   9000
#define START_SPACE_US  4500
#define REPEAT_SPACE_US 2250
#define END_GAP_MS      12       // ms

// Repeat behaviour
#define REPEAT_INTERVAL_MS  108  // ms between repeat frames while held
#define HOLD_DELAY_MS       500  // ms before first repeat fires

// ── Button config ─────────────────────────────────────────────
#define NUM_BUTTONS     8
const uint8_t BTN_PINS[NUM_BUTTONS] = { 4, 5, 6, 7, 8, 9, 10, 11 };
const uint8_t BTN_CMDS[NUM_BUTTONS] = {
    CMD_POWER, CMD_INCREASE, CMD_DECREASE, CMD_MODE_A,
    CMD_MODE_B, CMD_RESET,   CMD_USER1,    CMD_USER2
};
const char* BTN_NAMES[NUM_BUTTONS] = {
    "POWER", "INCREASE", "DECREASE", "MODE_A",
    "MODE_B", "RESET",   "USER1",    "USER2"
};

// ── Button state ──────────────────────────────────────────────
bool     btnPrev[NUM_BUTTONS]      = { false };
uint32_t btnPressTime[NUM_BUTTONS] = { 0 };
uint32_t lastRepeatTime[NUM_BUTTONS] = { 0 };
bool     holdFired[NUM_BUTTONS]    = { false };

// =============================================================
//  CARRIER — Timer2 Fast PWM ~38 kHz, 33% duty on Pin 3
// =============================================================
void setup_38khz() {
    pinMode(3, OUTPUT);
    // Fast PWM, TOP = OCR2A  (WGM2[2:0] = 0b111)
    TCCR2A = (1 << WGM20) | (1 << WGM21) | (1 << COM2B1);
    TCCR2B = (1 << WGM22) | (1 << CS20);   // no prescaler
    OCR2A  = 209;   // ~38 kHz carrier
    OCR2B  = 69;    // ~33% duty cycle
}

inline void carrier_on() {
    TCCR2A |= (1 << COM2B1);
}

inline void carrier_off() {
    TCCR2A &= ~(1 << COM2B1);
    PORTD  &= ~(1 << PORTD3);   // force LOW — prevent DC LED glitch
}

// =============================================================
//  CRC-8/MAXIM  (polynomial 0x31, reflected = 0x8C)
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
//  LOW-LEVEL PULSE PRIMITIVES
// =============================================================
void sendMark(uint16_t us) {
    carrier_on();
    delayMicroseconds(us);
    carrier_off();
}

void sendSpace(uint16_t us) {
    delayMicroseconds(us);
}

void sendBit(uint8_t bit) {
    sendMark(MARK_US);
    sendSpace(bit ? SPACE_1_US : SPACE_0_US);
}

void sendByte(uint8_t b) {
    // MSB first
    for (int8_t i = 7; i >= 0; i--) {
        sendBit((b >> i) & 0x01);
    }
}

// =============================================================
//  FRAME TRANSMISSION
// =============================================================
void sendFrame(uint8_t address, uint8_t command) {
    uint8_t packet[3];
    packet[0] = address;
    packet[1] = command;
    packet[2] = crc8_maxim(packet, 2);

    // Start pulse
    sendMark(START_MARK_US);
    sendSpace(START_SPACE_US);

    // Payload — 24 bits MSB first
    sendByte(packet[0]);
    sendByte(packet[1]);
    sendByte(packet[2]);

    // End gap
    delay(END_GAP_MS);
}

void sendRepeatFrame() {
    sendMark(START_MARK_US);
    sendSpace(REPEAT_SPACE_US);
    sendMark(MARK_US);       // single trailing mark
    delay(END_GAP_MS);
}

// =============================================================
//  SETUP
// =============================================================
void setup() {
    Serial.begin(115200);
    setup_38khz();
    carrier_off();

    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BTN_PINS[i], INPUT_PULLUP);
    }

    Serial.println(F("=== IR Transmitter v5.0 ready ==="));
    Serial.println(F("Buttons D4-D11 -> Commands 0x10..0x41"));
}

// =============================================================
//  MAIN LOOP
// =============================================================
void loop() {
    uint32_t now = millis();

    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        bool pressed = (digitalRead(BTN_PINS[i]) == LOW);

        if (pressed && !btnPrev[i]) {
            // ── Fresh press ──────────────────────────────────
            btnPressTime[i]   = now;
            lastRepeatTime[i] = now;
            holdFired[i]      = false;

            Serial.print(F("TX NEW  >> "));
            Serial.print(BTN_NAMES[i]);
            Serial.print(F("  (0x"));
            Serial.print(BTN_CMDS[i], HEX);
            Serial.println(F(")"));

            sendFrame(MY_ADDRESS, BTN_CMDS[i]);

        } else if (pressed && btnPrev[i]) {
            // ── Held down ────────────────────────────────────
            uint32_t heldMs = now - btnPressTime[i];

            if (heldMs >= HOLD_DELAY_MS) {
                if ((now - lastRepeatTime[i]) >= REPEAT_INTERVAL_MS) {
                    lastRepeatTime[i] = now;
                    holdFired[i]      = true;

                    Serial.print(F("TX RPT  >> "));
                    Serial.println(BTN_NAMES[i]);

                    sendRepeatFrame();
                }
            }
        }

        btnPrev[i] = pressed;
    }
}
