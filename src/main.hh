#include <Arduino.h>
#include <avr/sleep.h>

#define svoid static void

enum State {
    INIT,
    SLEEP,

    SETTING_TEMP,
    SETTING_TIMER_HOURS,
    SETTING_TIMER_MINUTES,
    IN_OPERATION,
    COOLING
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

svoid updateScreen();

svoid updateScreenBlink();

svoid updateOperationControl(uint32_t* time);

svoid updateOperationTimer(uint16_t time_passed);

svoid setHeating(bool enabled);

svoid setAuxiliaryHardware(bool enabled);

svoid setCooling(bool enabled);

svoid resetBlinking();