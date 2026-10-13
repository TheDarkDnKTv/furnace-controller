#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <EEPROM.h>

#define svoid static void

enum State {
    INIT,
    SLEEP,

    SETTING_TEMP,
    SETTING_TIMER_HOURS,
    SETTING_TIMER_MINUTES,
    IN_OPERATION,
    COOLING,
    FAULT
};

/**
 * Values are the codes shown on the display and stored in EEPROM
 */
enum class Fault : uint8_t {
    NONE = 0,
    SENSOR_LOST = 1,
    OVERHEAT = 2,
    OVERHEAT_ABSOLUTE = 3,
    POWER_LOSS = 4,
    WATCHDOG = 5
};

enum Control {
    LEFT,
    MAIN,
    RIGHT
};

svoid handleControlClick(Control control);

svoid handleChangeLongPress(bool increase = true);

svoid handleInterrupt();

svoid handleSync();

svoid setState(State new_state);

svoid updateInputs();

svoid shutdownPeripherals();

svoid stopOperation();

svoid showVersion();

svoid enableWatchdog();

svoid loadStoredFault();

svoid checkFaults();

svoid enterFault(Fault code);

svoid updateFault();

svoid clearFault();

svoid updateScreen();

svoid updateScreenBlink();

svoid showSetpoint(uint16_t value);

svoid editSetpoint(int16_t delta);

svoid finishSetpointEdit();

svoid updateTemperature();

svoid readTemperature();

svoid updateOperationControl();

svoid updateOperationTimer(uint16_t time_passed);

svoid setHeating(bool enabled);

svoid setAuxiliaryHardware(bool enabled);

svoid setCooling(bool enabled);

svoid setPowerStandby(bool enabled);

svoid resetBlinking();