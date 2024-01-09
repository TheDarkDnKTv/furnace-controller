#define SLEEP_TIMEOUT           20000 // Milliseconds
#define LONG_PRESS_INTERVAL     400   // Milliseconds
#define OP_CHECK_INTERVAL       500   // Milliseconds
#define TERMOMETER_THRESHOLD    30    // Degrees celsuium (3.0C)

#define MIN_TEMP                250     // 25.0
#define MAX_TEMP                2000    // 200.0
#define TEMP_CHANGE_STEP        50      // 5.0
#define TEMP_CHANGE_STEP_FAST   250     // 25.0

#define TIME_HOURS_DEFAULT      0
#define TIME_MINUTES_DEFAULT    0
#define TIME_HOURS_MAX          99
#define TIME_MINUTES_MAX        60
#define TIME_CHANGE_STEP_FAST   10

/* PINS */
#define CONTROL_RIGHT       7
#define CONTROL_CENTER      3 // Interruptable PIN only
#define CONTROL_LEFT        4

#define DISPLAY_CLOCK       5
#define DISPLAY_DATA        6

#define TEMPERATURE_SYNC        2  // Interruptable PIN only
#define DRIVER_MAIN_RELAY       8
#define DRIVER_FAN              9
#define DRIVER_HEATER_TOP       10
#define DRIVER_HEATER_BOTTOM    11
#define POWER_STANDBY           12

#define HEATING_INDICATOR       LED_BUILTIN

#define TEMPERATURE_SENSOR      A6

// Define if DEBUG mode
// #define DEBUG true