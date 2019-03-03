/*
 * From a Arduino-based device, control a BLWS23MDA motor(s) via a Sparkfun
 * MCP4725 DAC and a Sparkfun "Quiic" Quad Relay.
 * The inputs are a forward and backward digital signals, acceleration and max
 * speed analog signals (potentiometers).
 * Pages for Devices:
 *   https://learn.sparkfun.com/tutorials/qwiic-quad-relay-hookup-guide
 *   https://www.sparkfun.com/products/12918
 *   https://www.anaheimautomation.com/products/brushless/brushless-integrated-item.php?sID=147&pt=i&tID=97&cID=48
 */

#include <Wire.h> // For I2C

// Macros ====================================================================

#define ARRAY_COUNT(ARRAY) (sizeof((ARRAY)) / sizeof((ARRAY)[0]))

/*
 * Serial Logging Control
 * Should be disabled when this is uploaded for the last time
 * 0: Disable
 * 1: Enable
 */
#define LOG 1

#define FORWARD_DIR true
#define BACKWARD_DIR !FORWARD_DIR

#if LOG == 1
#define I2C_CHECK(desc) i2c_check((desc), Wire.endTransmission())
#else
#define I2C_CHECK(desc) Wire.endTransmission()
#endif

// Input =====================================================================

const int FORWARD_PIN = 4;
const int BACKWARD_PIN = 5;

const int POT_MAX = 1023;
#define RAMP_POT analogRead(A0)
#define MAX_SPEED_POT analogRead(A1)

// Output ====================================================================

// Output Pins ---------------------------------------------------------------

const int ENABLE_PIN = A2;

const int output_pins[] = {
    ENABLE_PIN,
};

// MCP4725 -------------------------------------------------------------------
const short MCP4725_ADDRESS = 0x60;

// Max Value to Pass to the DAC
const int MCP4725_MAX = 4095;

// I2C Command
#define MCP4725_SET_VOLTAGE 64

// Relays --------------------------------------------------------------------
const short RELAY_ADDRESS = 0x6D;

// I2C Commands
const short TOGGLE_RELAY_ONE = 0x01;
/* Unused
const int TOGGLE_RELAY_TWO = 0x02;
const int TOGGLE_RELAY_THREE = 0x03;
const int TOGGLE_RELAY_FOUR = 0x04;
const int TURN_ALL_ON = 0xB;
const short TURN_ALL_OFF = 0xA;
const int TOGGLE_ALL = 0xC;
*/
const short RELAY_ONE_STATUS = 0x05;
/* Unused
const int RELAY_TWO_STATUS = 0x06;
const int RELAY_THREE_STATUS = 0x07;
const int RELAY_FOUR_STATUS = 0x08;
*/

// Other Constants ===========================================================

// Number of milliseconds to wait between loop() iterations
const int TICK = 100;

// State Attributes
const short forward_attr = 1;
const short backward_attr = 2;
const short accelerate_attr = 4;
const short timeout_attr = 8;

// Global Program States
const short idle_state = 0;
const short forward_state = forward_attr | accelerate_attr;
const short backward_state = backward_attr | accelerate_attr;
const short forward_timeout_state = forward_attr | timeout_attr;
const short backward_timeout_state = backward_attr | timeout_attr;

// Global Variables ==========================================================

short state = idle_state;
float max_speed = 0.0;
float current_speed = 0.0;

// Variables Used For Debug Logging Only
#if LOG == 1
// Switch Update States
bool forward_printed = true;
bool backward_printed = true;

bool topped_out_printed;

// Max Speed Update State
float last_max_speed = 0.0;

// Ramp Update State
float last_ramp = 0.0;

// Speed Update State
float last_speed = 0.0;

bool state_printed = false;
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
const unsigned short OPPOSITE_DIR_TOTAL_TICKS = 10;
unsigned short opposite_dir_tick_count = 0;

// Functions =================================================================

#if LOG == 1
void i2c_check(const char * desc, int result) {
    if (result) {
        Serial.print("I2C ERROR on ");
        Serial.print(desc);
        Serial.print(": ");
        switch (result) {
        case 1:
            Serial.println("Can't send more than 32 bytes");
            break;
        case 2:
            Serial.println("NACK on address transmit");
            break;
        case 3:
            Serial.println("NACK on data transmit");
            break;
        default:
            Serial.print("unknown error code: ");
            Serial.println(result);
            break;
        }
    }
}
#endif

void set_speed(float speed) {
    if (speed == 0.0) {
        digitalWrite(ENABLE_PIN, HIGH);
    } else {
        digitalWrite(ENABLE_PIN, LOW);
    }

    int value = (MCP4725_MAX * speed);

    Wire.beginTransmission(MCP4725_ADDRESS);
    Wire.write(MCP4725_SET_VOLTAGE);
    Wire.write(value >> 4);
    Wire.write((value & 15) << 4);

    I2C_CHECK("Setting Speed");
}

void set_direction(bool positive) {
    Wire.beginTransmission(RELAY_ADDRESS);
    Wire.write(RELAY_ONE_STATUS);
    I2C_CHECK("Read Relay One Status");

    Wire.requestFrom(RELAY_ADDRESS, 1);
    bool on;
    int status;
    while (Wire.available()) {
        status = Wire.read();
        Serial.println(status);
        on = (status ? true : false);
    }

    if (positive == on) {
        Wire.beginTransmission(RELAY_ADDRESS);
        Wire.write(TOGGLE_RELAY_ONE);
        I2C_CHECK("Toggle Relay One");
    }
}

void setup() {
    Wire.begin();

    // Set Output Pins
    for (unsigned int i = 0; i < ARRAY_COUNT(output_pins); i++) {
        pinMode(output_pins[i], OUTPUT);
    }

    digitalWrite(ENABLE_PIN, LOW);

#if LOG == 1
    // Set up Serial Output
    Serial.begin(9600);
#endif

}

void loop() {

    // Update Max Speed
    max_speed = ((float) MAX_SPEED_POT) / POT_MAX;
#if LOG == 1
    if (abs(max_speed - last_max_speed) > 0.01) {
        Serial.print("MAX: ");
        Serial.println(max_speed);
        last_max_speed = max_speed;
    }
#endif

    // Get Ramp
    float ramp = ((float) RAMP_POT) / (POT_MAX * 10);
    if (ramp < 0.01) {
        // Ramp should be at least 0.01
        ramp = 0.01;
    }
#if LOG == 1
    if (abs(ramp - last_ramp) < 0.01) {
        Serial.print("RAMP: ");
        Serial.println(ramp);
        last_ramp = ramp;
    }
#endif


    // Get Command
    bool forward_pressed = digitalRead(FORWARD_PIN) == HIGH;
    bool backward_pressed = digitalRead(BACKWARD_PIN) == HIGH;
    bool forward = forward_pressed && !backward_pressed;
    bool backward = backward_pressed && !forward_pressed;
#if LOG == 1
    if (forward) {
        if (!forward_printed) {
            // Print FORWARD PRESSED if not done already
            Serial.println("FORWARD PRESSED");
            forward_printed = true;
            backward_printed = false;
        }
    } else if (backward) {
        if (!backward_printed) {
            // Print BACKWARD PRESSED if not done already
            Serial.println("BACKWARD PRESSED");
            forward_printed = false;
            backward_printed = true;
        }
    } else if (forward_printed || backward_printed) {
        // Print OFF if not done already
        Serial.println("OFF");
        forward_printed = false;
        backward_printed = false;
    }
#endif

    /*
     * In this state wait for a single button press
     */
    if (state == idle_state) {
#if LOG == 1
        if (!state_printed) {
            Serial.println("IDLE");
            state_printed = true;
        }
#endif
        if (forward != backward) {
            current_speed = 0.0;
            if (forward) {
                state = forward_state;
                set_direction(FORWARD_DIR);
            } else {
                state = backward_state;
                set_direction(BACKWARD_DIR);
            }
#if LOG == 1
            state_printed = false;
#endif
        } else {
            set_speed(0.0);
            return;
        }
    }

    bool same_dir =
        ((state & forward_attr) && forward) != ((state & backward_attr) && backward);

    /*
     * In this state accelerate until max speed is reached and go to next state
     * if no or both buttons are pressed.
     */
    if (state & accelerate_attr) {
#if LOG == 1
        if (!state_printed) {
            Serial.println("ACCELERATE");
            state_printed = true;
        }
#endif
        if (same_dir) {
            if (current_speed != max_speed) {
                current_speed += ramp;
#if LOG == 1
                bool print_speed = false;
#endif
                if (current_speed >= max_speed) {
                    current_speed = max_speed;
#if LOG == 1
                    if (!topped_out_printed) {
                        print_speed = true;
                        topped_out_printed = true;
                    }
                } else {
                    topped_out_printed = false;
                    print_speed = true;
#endif
                }
                set_speed(current_speed);
#if LOG == 1
                if (print_speed) {
                    Serial.print("SPEED: ");
                    Serial.println(current_speed);
                }
#endif
            }
            delay(TICK);
            return;
        } else {
            set_speed(0.0);
            current_speed = 0.0;
            state = (state & ~accelerate_attr) | timeout_attr;
            opposite_dir_tick_count = 0;
#if LOG == 1
            state_printed = false;
            Serial.println("Disable Opposite Direction");
#endif
        }
    }

    /*
     * In this state wait for the same button to be pressed again or for the
     * timeout
     */
    if (state & timeout_attr) {
#if LOG == 1
        if (!state_printed) {
            Serial.println("TIMEOUT");
            state_printed = true;
        }
#endif
        if (same_dir) {
            state = (state & ~timeout_attr) | accelerate_attr;
#if LOG == 1
            state_printed = false;
#endif
        } else {
            if (opposite_dir_tick_count < OPPOSITE_DIR_TOTAL_TICKS) {
                opposite_dir_tick_count++;
                delay(TICK);
            } else {
                state = idle_state;
#if LOG == 1
                state_printed = false;
                Serial.println("Timed Out");
#endif
            }
        }
    }
}
