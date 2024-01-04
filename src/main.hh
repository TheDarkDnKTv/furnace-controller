#include <Arduino.h>
#include <avr/sleep.h>

enum State {
    IDLE,
    SLEEP,
    IN_OPERATION
};

enum Control {
    LEFT,
    MAIN,
    RIGHT
};

static void handleControlClick(Control control);

static void handleInterrupt();

static void hibernate();

static void updateInputs();