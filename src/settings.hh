#define SLEEP_TIMEOUT           5000 // Miliseconds
#define LONG_PRESS_INTERVAL     400  // Milliseconds

#define MIN_TEMP                25
#define MAX_TEMP                200
#define TEMP_CHANGE_STEP        5
#define TEMP_CHANGE_STEP_FAST   25

/* TERMISTOR */
#define TRM_RESISTANCE_DELTA    2300    // Ohm per celsium
#define TRM_BASE_RESISTANCE     10      // Base resistance, kOhm. Measure at some temperature and set resistance and that temperature
#define TRM_BASE_RES_DEGREES    253     // Temperature base resistance measured at (last digit is decimal point, 25.3)

#define TRM_DIVIDER_REF_VOLTAGE 4690    // millivolts
#define TRM_DIVIDER_RESISTANCE  100     // kOhm

/* PINS */
#define CONTROL_RIGHT       2
#define CONTROL_CENTER      3
#define CONTROL_LEFT        4

#define DISPLAY_CLOCK       5
#define DISPLAY_DATA        6

#define DRIVER_MAIN_RELAY       7
#define DRIVER_FAN              8
#define DRIVER_HEATER_TOP       9
#define DRIVER_HEATER_BOTTOM    10

#define TEMPERATURE_SENSOR      A7