#include <Arduino.h>
#include <OneButton.h>

#define BTN_PIN 2 // D2

typedef unsigned long ulong;

ulong last_switched = 0;
const uint16_t PERIOD = 333;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
}

void loop() {
  if (!digitalRead(BTN_PIN)) {
    digitalWrite(LED_BUILTIN, false);
    return;
  }

  ulong time = millis();
  if (time - last_switched >= PERIOD) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    last_switched = time;
  }  
}