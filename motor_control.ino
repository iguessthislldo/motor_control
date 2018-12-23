/*
 * Serial Logging Control
 * Should be disabled when this is uploaded for the last time
 * 0: Disable
 * 1: Enable
 */
#define LOG 1

#if LOG == 1
#define LOG_SPEED Serial.print("SPEED: "); Serial.print(speed); Serial.println('%');
#else
#define LOG_SPEED // Nothing
#endif

#include <Wire.h>
#include <Adafruit_MCP4725.h>

// Input =====================================================================

#define FORWARD_PRESSED (digitalRead(5) == LOW)
#define FORWARD_DIR(value) (value)
#define BACKWARD_PRESSED (digitalRead(6) == LOW)
#define BACKWARD_DIR(value) -(value)

// Output ====================================================================

const int DIRECTION_PIN = 7;
#define NEGATIVE_DIR LOW
#define POSITIVE_DIR HIGH

// Copied from one of the official examples:
// For Adafruit MCP4725A1 the address is 0x62 (default) or 0x63 (ADDR pin tied to VCC)
// For MCP4725A0 the address is 0x60 or 0x61
// For MCP4725A2 the address is 0x64 or 0x65
const int MCP4725_ADDRESS = 0x62;

// Max Value to Pass to the DAC
const int MCP4725_MAX = 4095;

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

/*
 * DAC Driver Object
 */
Adafruit_MCP4725 dac1;

#if LOG == 1
// Switch Update States (For Debug Logging Only)
bool forward_printed = false;
bool backward_printed = false;
#endif

// Functions =================================================================

void setup() {
    dac1.begin(MCP4725_ADDRESS);

    pinMode(DIRECTION_PIN, OUTPUT);

#if LOG == 1
    // Set up Serial Output
    Serial.begin(9600);

    // Print Initial Speed
    print_speed();
#endif
}

void loop() {

    // Update Target Speed By Checking Switches
    int speed_step;
    bool foward = FOWARD_SWITCH_PRESSED;
    bool backward = BACKWARD_SWITCH_PRESSED;
    if (foward && !backward) {
        target_speed = FORWARD_DIR(100);
        speed_step = ACCELERATE_STEP;
#if LOG == 1
        // Print FORWARD if not done already
        if (!foward_printed) {
            Serial.println("FORWARD PRESSED");
            forward_printed = true;
            backward_printed = false;
        }
#endif
    } else if (backward && !forward) {
        target_speed = BACKWARD_DIR(100);
        speed_step = ACCELERATE_STEP;
#if LOG == 1
        // Print FORWARD if not done already
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
        // Print FORWARD if not done already
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

    // Update Output
    int abs_speed = 0;
    if (current_speed > 0) {
        abs_speed = current_speed;
        digitalWrite(DIRECTION_PIN, POSITIVE_DIR);
    } else if (current_speed < 0) {
        abs_speed = -current_speed;
        digitalWrite(DIRECTION_PIN, NEGATIVE_DIR);
    }
    dac1.setVoltage(MPC4725_MAX * abs_speed / 100, false);

    // Sleep for a While
    delay(TICK);
}
