#define VERSION_MAJOR           1
#define VERSION_MINOR           3
#define VERSION_PATCH           0
#define VERSION_DISPLAY_TIME    1000  // Milliseconds, shown once on boot

#define SLEEP_TIMEOUT           20000 // Milliseconds
#define LONG_PRESS_INTERVAL     400   // Milliseconds
#define OP_CHECK_INTERVAL       5000  // Milliseconds
#define OP_TEMP_HYSTERESIS      30    // Degrees celsuium (1.5C)
#define OP_TEMP_CONTROL_RATIO   25    // k (relay control algorithm)
#define NEED_COOLING_FROM       500   // If temp more than (50.0C) will cooldown
#define SYNC_LOST_TIMEOUT       2500  // Milliseconds without main board pulse before reading sensor on own timer
#define SENSOR_READ_INTERVAL    500   // Milliseconds, own timer reading interval

#define SENSOR_FAULT_TIMEOUT    10000     // Milliseconds without valid reading before E1
#define OVERHEAT_MARGIN         100       // 10.0C above setpoint or session's lowest temp before E2
#define OVERHEAT_ABSOLUTE_TEMP  1200      // 120.0C, E3
#define FAULT_FAN_TIME          900000UL  // Milliseconds of fan after a fault when hot but sensor lost (15 min)
#define FAULT_CLEAR_HOLD        3000      // Milliseconds holding LEFT + RIGHT to clear a fault
#define FAULT_COOLING_HYSTERESIS 20       // 2.0C, fan keeps running until this far below NEED_COOLING_FROM

#define EEPROM_FAULT_CODE       0
#define EEPROM_SESSION_RUNNING  1

#define MIN_TEMP                250     // 25.0
#define MAX_TEMP                1000    // 100.0
#define TEMP_CHANGE_STEP_FINE   10      // 1.0, toggled by double-clicking MAIN on the temperature screen
#define TEMP_CHANGE_STEP        50      // 5.0
#define TEMP_CHANGE_STEP_FAST   250     // 25.0
#define SETPOINT_EDIT_TIMEOUT   5000    // Milliseconds without a press before the in-session setpoint screen closes

#define TIME_HOURS_DEFAULT      0
#define TIME_MINUTES_DEFAULT    0
#define TIME_HOURS_MAX          99
#define TIME_MINUTES_MAX        60
#define TIME_CHANGE_STEP_FAST   10

#define FAN_ENABLE_TEMP         700   // If temp is higher than this, cooling will be enabled

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
#define DRIVER_HEATER_BOTTOM    10 /*11*/ // Disabled if same pin
#define POWER_STANDBY           12

#define HEATING_INDICATOR       LED_BUILTIN

#define TEMPERATURE_SENSOR      A6



// Define if DEBUG mode
// #define DEBUG true