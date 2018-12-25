// Macros ====================================================================

/*
 * Serial Logging Control
 * Should be disabled when this is uploaded for the last time
 * 0: Disable
 * 1: Enable
 */
#define LOG 1

#if LOG == 1
#define LOG_SPEED \
    Serial.print("SPEED: "); Serial.print(current_speed); Serial.println('%');
#else
#define LOG_SPEED // Nothing
#endif

/*
 * Set Motor Driver
 */
#define L298N 0
#define MCP4725 1
#define DRIVER MCP4725
//#define DRIVER L298N

#if DRIVER == MCP4725
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#endif

// Input =====================================================================

#define FORWARD_SWITCH_PRESSED (digitalRead(5) == HIGH)
#define FORWARD_DIR(value) (value)
#define BACKWARD_SWITCH_PRESSED (digitalRead(6) == HIGH)
#define BACKWARD_DIR(value) -(value)

// Output ====================================================================

#define NEGATIVE_DIR LOW
#define POSITIVE_DIR HIGH

// MCP4725 -------------------------------------------------------------------
#if DRIVER == MCP4725

const int DIRECTION_PIN = 7;

// Copied from one of the official examples:
// For Adafruit MCP4725A1 the address is 0x62 (default) or 0x63 (ADDR pin tied to VCC)
// For MCP4725A0 the address is 0x60 or 0x61
// For MCP4725A2 the address is 0x64 or 0x65
const int MCP4725_ADDRESS = 0x62;

// Max Value to Pass to the DAC
const int MCP4725_MAX = 4095;

// L298N ---------------------------------------------------------------------
#else

const int DIR_1_PIN = 13;
const int DIR_2_PIN = 12;

const int SPEED_PIN = 10;
const int L298N_MAX = 255;
#endif

// Output Pins ---------------------------------------------------------------
const int output_pins[] = {
#if DRIVER == MCP4725
    DIRECTION_PIN
#else // L298N
    DIR_1_PIN,
    DIR_2_PIN,
    SPEED_PIN
#endif
};

// Other Constants ===========================================================

// If a button is being pressed, this is how much it will change between loop()
// iterations as a percent
const int ACCELERATE_STEP = 5;

// If no button is pressed, this is how much it will change between loop()
// iterations as a percent decelerate
const int DECELERATE_STEP = 20;

// Number of milliseconds to wait between loop() iterations
const int TICK = 100;

// Global Variables ==========================================================

/*
 * Current Speed of the Motor as a Percent
 * Goes from -100 to 100
 */
int current_speed = 0;

/*
 * Target Speed of the Motor as a Percent
 * Goes from -100 to 100
 */
int target_speed = 0;

#if DRIVER == MCP4725
/*
 * DAC Driver Object
 */
Adafruit_MCP4725 dac1;
#endif

#if LOG == 1
// Switch Update States (For Debug Logging Only)
bool forward_printed = true;
bool backward_printed = true;
#endif

// Functions =================================================================

void setup() {
#if DRIVER == MCP4725
    dac1.begin(MCP4725_ADDRESS);
#endif

    // Set Output Pins
    int output_pin_count = sizeof(output_pins) / sizeof(output_pins[0]);
    for (int i = 0; i < output_pin_count; i++) {
        pinMode(output_pins[i], OUTPUT);
    }

#if LOG == 1
    // Set up Serial Output
    Serial.begin(9600);

    // Print Initial Speed
    LOG_SPEED;
#endif
}

void loop() {

    // Update Target Speed By Checking Switches
    int speed_step;
    bool forward = FORWARD_SWITCH_PRESSED;
    bool backward = BACKWARD_SWITCH_PRESSED;
    if (forward && !backward) {
        target_speed = FORWARD_DIR(100);
        speed_step = ACCELERATE_STEP;
#if LOG == 1
        // Print FORWARD PRESSED if not done already
        if (!forward_printed) {
            Serial.println("FORWARD PRESSED");
            forward_printed = true;
            backward_printed = false;
        }
#endif
    } else if (backward && !forward) {
        target_speed = BACKWARD_DIR(100);
        speed_step = ACCELERATE_STEP;
#if LOG == 1
        // Print BACKWARD PRESSED if not done already
        if (!backward_printed) {
            Serial.println("BACKWARD PRESSED");
            forward_printed = false;
            backward_printed = true;
        }
#endif
    } else {
        // Neither Button is being pressed or both are being pressed, so we shouldn't
        // do anything.
        target_speed = 0;
        speed_step = DECELERATE_STEP;
#if LOG == 1
        // Print OFF if not done already
        if (forward_printed || backward_printed) {
            Serial.println("OFF");
            forward_printed = false;
            backward_printed = false;
        }
#endif
    }

    // Update Current Speed
    // Move it closer to target speed if it's not the same.
    if (target_speed != current_speed) {
        if (target_speed > current_speed) {
            current_speed += speed_step;
            if (current_speed > target_speed)
                current_speed = target_speed;
        } else {
            current_speed -= speed_step;
            if (current_speed < target_speed)
                current_speed = target_speed;
        }
        LOG_SPEED;
    }

    // Update Direction
    int abs_speed = 0;
    if (current_speed > 0) {
        abs_speed = current_speed;
#if DRIVER == MCP4725
        digitalWrite(DIRECTION_PIN, POSITIVE_DIR);
#else // L298N
        digitalWrite(DIR_1_PIN, POSITIVE_DIR);
        digitalWrite(DIR_2_PIN, NEGATIVE_DIR);
#endif
    } else if (current_speed < 0) {
        abs_speed = -current_speed;
#if DRIVER == MCP4725
        digitalWrite(DIRECTION_PIN, NEGATIVE_DIR);
#else // L298N
        digitalWrite(DIR_1_PIN, NEGATIVE_DIR);
        digitalWrite(DIR_2_PIN, POSITIVE_DIR);
#endif
    }

    // Update Speed
#if DRIVER == MCP4725
    dac1.setVoltage(MCP4725_MAX * abs_speed / 100, false);
#else // L298N
    analogWrite(SPEED_PIN, L298N_MAX * abs_speed / 100);
#endif

    // Sleep for a While
    delay(TICK);
}
