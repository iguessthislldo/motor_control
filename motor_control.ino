#define LOG

// Input =====================================================================

// Switch 1
#define SWITCH_1_PRESSED (digitalRead(5) == LOW)

// Rotary Encoder
const int RE_DT = 2;
const int RE_CLK = 3;
const int RE_INTERRUPT = 1; // Interrupt for Pin 3

// Output ====================================================================

// Motor A
const int MOTOR_A_DIR_1 = 13;
const int MOTOR_A_DIR_2 = 12;
const int MOTOR_A_SPEED = 10;

// Motor B
const int MOTOR_B_DIR_1 = 11;
const int MOTOR_B_DIR_2 = 8;
const int MOTOR_B_SPEED = 9;

const int output_pins[] = {
    MOTOR_A_DIR_1,
    MOTOR_A_DIR_2,
    MOTOR_A_SPEED,
    MOTOR_B_DIR_1,
    MOTOR_B_DIR_2,
    MOTOR_B_SPEED
};

// Other Constants ===========================================================

const int RE_RANGE = 10;
const int SPEED_MAX = 255;
const int TICK = 100;

// Global Variables ==========================================================

// Speed and direction of the motor
int speed = 0;

// Rotary Encoder State
int re_count = 0;
int re_last_clk = 0;

#ifdef LOG
// Switch Update States (For Debug Logging Only)
bool on_printed = false
bool off_printed = false;
#endif

// Functions =================================================================

// Called when Rotary Encoder Changes
void re_interrupt() {
    int re_clk = digitalRead(RE_CLK);
    int re_dt = digitalRead(RE_DT);
    if (re_last_clk != re_clk) {
        re_last_clk = re_clk;
        int pos = re_count < RE_RANGE ? 1 : 0;
        int neg = re_count > -RE_RANGE ? -1 : 0;
        re_count += ((re_clk != re_dt) ? pos : neg);
    }
#ifdef LOG
    // Print New Speed
    Serial.print("SPEED: ");
    if (re_count) {
        Serial.print(re_count);
    }
    Serial.println("0%");
#endif
}

void setup() {
#ifdef LOG
    // Set up Serial Output
    Serial.begin(9600);

    // Print Initial Speed
    Serial.print("SPEED: ");
    if (re_count) {
        Serial.print(re_count);
    }
    Serial.println("0%");
#endif

    // Set Output Pins
    int output_pin_count = sizeof(output_pins) / sizeof(output_pins[0]);
    for (int i = 0; i < output_pin_count; i++) {
        pinMode(output_pins[i], OUTPUT);
    }

    // Set up RE Interrupt
    attachInterrupt(RE_INTERRUPT, re_interrupt, CHANGE);
}

void loop() {
    // Update Speed
    if (SWITCH_1_PRESSED) {
        speed = SPEED_MAX * (((float) re_count) / RE_RANGE);

        // Limit Speed Just In Case
        if (speed > SPEED_MAX)
            speed = SPEED_MAX;
        else if (speed < -SPEED_MAX)
            speed = -SPEED_MAX;

#ifdef LOG
        // Print ON if not done already
        if (!on_printed) {
            Serial.println("ON");
            on_printed = true;
            off_printed = false;
        }
#endif
    } else {
        speed = 0;
#ifdef LOG
        // Print OFF if not done already
        if (!off_printed) {
            Serial.println("OFF");
            off_printed = true;
            on_printed = false;
        }
#endif
    }

    // Update Motor
    if (speed >= 0) {
        digitalWrite(MOTOR_A_DIR_1, LOW);
        digitalWrite(MOTOR_A_DIR_2, HIGH);
        analogWrite(MOTOR_A_SPEED, speed);
    } else {
        digitalWrite(MOTOR_A_DIR_1, HIGH);
        digitalWrite(MOTOR_A_DIR_2, LOW);
        analogWrite(MOTOR_A_SPEED, -speed);
    }

    // Sleep for a While
    delay(TICK);
}
