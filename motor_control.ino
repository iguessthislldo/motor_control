#include <Wire.h>

/*
 * Serial Logging Control
 * Should be disabled when this is uploaded for the last time
 * 0: Disable
 * 1: Enable
 */
#define LOG 1

#if LOG == 1
#define LOG_SPEED \
    Serial.print("SPEED: "); \
    Serial.print(current_speed); \
    Serial.print('/'); \
    Serial.println(DENOMINATOR);
#else
#define LOG_SPEED // Nothing
#endif

// Input =====================================================================

#define FORWARD_DIR(value) (value)
const int FORWARD_PIN = 4;

#define BACKWARD_DIR(value) -(value)
const int BACKWARD_PIN = 5;

const int POT_MAX = 1023;
#define RAMP_POT analogRead(A0)
#define MAX_SPEED_POT analogRead(A1)

// Output ====================================================================

// Output Pins ---------------------------------------------------------------

const int ENABLE_PIN = A2;

const int DIRECTION_PIN = A3;
#define NEGATIVE_DIR false
#define POSITIVE_DIR true

const int output_pins[] = {
    ENABLE_PIN,
    DIRECTION_PIN
};

// MCP4725 -------------------------------------------------------------------
const int MCP4725_ADDRESS = 0x60;

// Max Value to Pass to the DAC
const int MCP4725_MAX = 4095;

// Relays --------------------------------------------------------------------
// From https://learn.sparkfun.com/tutorials/qwiic-quad-relay-hookup-guide
const int RELAY_ADDRESS = 0x6D;

// I2C Commands
const int TOGGLE_RELAY_ONE = 0x01;
const int TOGGLE_RELAY_TWO = 0x02;
const int TOGGLE_RELAY_THREE = 0x03;
const int TOGGLE_RELAY_FOUR = 0x04;
const int TURN_ALL_ON = 0xB;
const int TURN_ALL_OFF = 0xA;
const int TOGGLE_ALL = 0xC;
const int RELAY_ONE_STATUS = 0x05;
const int RELAY_TWO_STATUS = 0x06;
const int RELAY_THREE_STATUS = 0x07;
const int RELAY_FOUR_STATUS = 0x08;

// Other Constants ===========================================================

/*
 * The common denominator for speed. 100 means the speed is a percentage of the
 * max speed.
 */
const long DENOMINATOR = 100;

// Number of milliseconds to wait between loop() iterations
const int TICK = 100;

// Global Variables ==========================================================

/*
 * Max Speed of the Motor as a Percent
 * Goes from 0 to DENOMINATOR
 */
int max_speed = 0;

/*
 * Current Speed of the Motor as a Percent
 * Goes from -DENOMINATOR to DENOMINATOR
 */
int current_speed = 0;

/*
 * Target Speed of the Motor as a Percent
 * Goes from -DENOMINATOR to DENOMINATOR
 */
int target_speed = 0;

// Variables Used For Debug Logging Only
#if LOG == 1
// Switch Update States
bool forward_printed = true;
bool backward_printed = true;

// Max Speed Update State
int last_max_speed = 0;
#endif

/*
 * Disable opposite direction for 1 second after releasing current direction
 * Motor (BLWS23MDA) is asking for this
 * State Diagram looks like this:
 *
 *         +---------------(TIMEOUT)----------+  +----------------(TIMEOUT)-------+
 *         |                                  V  V                                |
 * FORWARD_DISABLED <-(RELEASED)- BACKWARD <- IDLE -> FORWARD -(RELEASED)> BACKWARD_DISABLED
 *         |                          ^                  ^                        |
 *         +--------------------------+                  +------------------------+
 */
const unsigned long OPPOSITE_DELAY_TICKS = 10;
bool disable_opposite_dir = false;
bool last_dir; // true is forward, false is backward
bool timedout = false;

// Functions =================================================================

void set_speed(int speed) {
    int value = ((long) MCP4725_MAX) * speed / DENOMINATOR;

    // Based on Sparkfun Example
    Wire.beginTransmission(MCP4725_ADDRESS);
    Wire.write(64);
    Wire.write(value >> 4);
    Wire.write((value & 15) << 4);
#if LOG == 1
    int result =
#endif
    Wire.endTransmission();
#if LOG == 1
    switch (result) {
    case 0:
        Serial.println("set_speed(): Successfully wrote speed");
        break;
    case 1:
        Serial.println("set_speed(): ERROR: Can't send more than 32 bytes");
        break;
    case 2:
        Serial.println("set_speed(): ERROR: NACK on address transmit");
        break;
    case 3:
        Serial.println("set_speed(): ERROR: NACK on data transmit");
        break;
    default:
        Serial.print("set_speed(): ERROR: unknown error code: ");
        Serial.println(result);
        break;
    }
#endif
}

void set_direction(bool positive) {
    if (last_dir == positive) {
        return;
    } else {
        last_dir = positive;
    }
    Wire.beginTransmission(RELAY_ADDRESS);
    Wire.write(TURN_ALL_OFF);
    Wire.endTransmission();
    if (positive) {
        Wire.beginTransmission(RELAY_ADDRESS);
        Wire.write(TOGGLE_RELAY_ONE);
        Wire.endTransmission();
    }
}

void setup() {
    Wire.begin();

    // Set Output Pins
    int output_pin_count = sizeof(output_pins) / sizeof(output_pins[0]);
    for (int i = 0; i < output_pin_count; i++) {
        pinMode(output_pins[i], OUTPUT);
    }

    digitalWrite(ENABLE_PIN, LOW);

#if LOG == 1
    // Set up Serial Output
    Serial.begin(9600);

    // Print Initial Speed
    LOG_SPEED;
#endif

}

void loop() {
    // Update Max Speed
    max_speed = ((long) MAX_SPEED_POT) * DENOMINATOR / POT_MAX;
#if LOG == 1
    if (abs(max_speed - last_max_speed)) {
        Serial.print("MAX: ");
        Serial.print(max_speed);
        Serial.print('/');
        Serial.println(DENOMINATOR);
        last_max_speed = max_speed;
    }
#endif

    // Get Ramp
    int ramp = ((long) RAMP_POT) * DENOMINATOR / (POT_MAX * 10);
    //ramp = ((long) ramp) * ((long) max_speed) / DENOMINATOR;
    if (ramp == 0) {
        // Ramp should be at least 1
        ramp = 1;
    }

    // Update Target Speed By Checking Switches
    bool forward = digitalRead(FORWARD_PIN) == HIGH;
    bool backward = digitalRead(BACKWARD_PIN) == HIGH;
    if (forward && !backward) {
        digitalWrite(ENABLE_PIN, HIGH);
        target_speed = FORWARD_DIR(max_speed);

#if LOG == 1
        // Print FORWARD PRESSED if not done already
        if (!forward_printed) {
            Serial.println("FORWARD PRESSED");
            forward_printed = true;
            backward_printed = false;
        }
#endif
    } else if (backward && !forward) {
        digitalWrite(ENABLE_PIN, HIGH);
        target_speed = BACKWARD_DIR(max_speed);

#if LOG == 1
        // Print BACKWARD PRESSED if not done already
        if (!backward_printed) {
            Serial.println("BACKWARD PRESSED");
            forward_printed = false;
            backward_printed = true;
        }
#endif
    } else {
        digitalWrite(ENABLE_PIN, LOW);

        // Neither Button is being pressed or both are being pressed, so we shouldn't
        // do anything.
        target_speed = 0;
        current_speed = 0;
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
            current_speed += ramp;
            if (current_speed > target_speed)
                current_speed = target_speed;
        } else {
            current_speed -= ramp;
            if (current_speed < target_speed)
                current_speed = target_speed;
        }
        LOG_SPEED;
    }

    // Update Direction
    int abs_speed = 0;
    if (current_speed > 0) {
        abs_speed = current_speed;
        set_direction(POSITIVE_DIR);
    } else if (current_speed < 0) {
        abs_speed = -current_speed;
        set_direction(NEGATIVE_DIR);
    }

    // Update Speed
    set_speed(abs_speed);

    // Sleep for a While
    delay(TICK);
}
