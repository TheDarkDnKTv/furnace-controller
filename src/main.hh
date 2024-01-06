#include <Arduino.h>
#include <avr/sleep.h>

#define svoid static void

enum State {
    INIT,
    SLEEP,

    SETTING_TEMP,
    SETTING_TIMER,
    IN_OPERATION,
};

enum Control {
    LEFT,
    MAIN,
    RIGHT
};

svoid handleControlClick(Control control);

svoid handleChangeLongPress(bool increase = true);

svoid handleInterrupt();

svoid setState(State new_state);

svoid updateInputs();

svoid shutdownPeripherals();

svoid updateScreen();
