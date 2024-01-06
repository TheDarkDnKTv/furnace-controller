#include <settings.hh>
#include <main.hh>
#include <sensor.hh>
#include <sensor.cpp>
#include <display.hh>
#include <OneButton.h>

Display display = Display(1000);
Sensor* sensor;

static OneButton BTN_LEFT;
static OneButton BTN_MAIN;
static OneButton BTN_RIGHT;
static bool action_perfomed;

volatile State STATE;
volatile uint32_t sleep_count_time;

uint8_t temperature = MIN_TEMP;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); // TODO remove

  {
    BTN_LEFT = OneButton(CONTROL_LEFT, true, true);
    BTN_MAIN = OneButton(CONTROL_CENTER, true, true);
    BTN_RIGHT = OneButton(CONTROL_RIGHT, true, true);

    BTN_LEFT.attachClick([]() { handleControlClick(Control::LEFT); });
    BTN_MAIN.attachClick([]() { handleControlClick(Control::MAIN); });
    BTN_RIGHT.attachClick([]() { handleControlClick(Control::RIGHT); });

    BTN_MAIN.attachLongPressStart([]() { setState(State::INIT); });
    BTN_LEFT.attachDuringLongPress([]() { handleChangeLongPress(false); });
    BTN_RIGHT.attachDuringLongPress([]() { handleChangeLongPress(true); });

    BTN_LEFT.setLongPressIntervalMs(LONG_PRESS_INTERVAL);
    BTN_RIGHT.setLongPressIntervalMs(LONG_PRESS_INTERVAL);
  }

  sensor = new ToshibaSensor(TEMPERATURE_SENSOR);

  attachInterrupt(1, handleInterrupt, FALLING);

  Serial.begin(9600);
  display.setBrightness(7);
  display.clear();

  setState(State::INIT);
  sleep_count_time = millis();
}

void loop() {
  uint32_t time = millis();
  updateInputs();

  display.update(time);
  if (STATE != State::IN_OPERATION) {
    if (action_perfomed) {
      sleep_count_time = time;
    }

    if (time >= SLEEP_TIMEOUT + sleep_count_time) {
      setState(State::SLEEP);
    }
  }
}

svoid setState(State new_state) {
  switch (STATE = new_state) {
  case INIT: {
    display.setSegments((const uint8_t[]) {
      SEG_A | SEG_B | SEG_G | SEG_F,
      SEG_G | SEG_C | SEG_D | SEG_E,
      SEG_A | SEG_B | SEG_G | SEG_F,
      SEG_G | SEG_C | SEG_D | SEG_E,
      SEG_A | SEG_B | SEG_G | SEG_F
    }, 5);
    break;
  }
  case SETTING_TEMP: {
    temperature = MIN_TEMP;
    updateScreen();
    break;
  };
  case SLEEP: {
    shutdownPeripherals();
    delay(50);
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_mode();
    break;
  }
  
  default:
    break;
  }
}

svoid updateScreen() {
  switch (STATE) {
    case State::SETTING_TEMP: {
      display.setDecimal(temperature);
      break;
    }
    default: break;
  }
}

svoid updateInputs() {
  action_perfomed = false;
  BTN_LEFT.tick();
  BTN_MAIN.tick();
  BTN_RIGHT.tick();
}

svoid handleControlClick(Control control) {
  action_perfomed = true;
  Serial.print("Control pressed: ");
  Serial.println(control);
  switch (control) {
    case Control::LEFT: {
      if (STATE == State::SETTING_TEMP) {
        temperature = max(MIN_TEMP, temperature - TEMP_CHANGE_STEP);
        updateScreen();
      }

      break;
    }
    case Control::RIGHT: {
      if (STATE == State::SETTING_TEMP) {
        temperature = min(MAX_TEMP, temperature + TEMP_CHANGE_STEP);
        updateScreen();
      }

      break;
    }
    case Control::MAIN: {
      if (STATE == State::INIT) {
        setState(State::SETTING_TEMP);
      }
      break;
    }
  }
}

svoid handleChangeLongPress(bool increase) {
  if (STATE == State::SETTING_TEMP) {
    if (increase) {
      temperature = min(MAX_TEMP, temperature + TEMP_CHANGE_STEP_FAST);
    } else {
      temperature = max(MIN_TEMP, temperature - TEMP_CHANGE_STEP_FAST);
    }

    updateScreen();
  }

  action_perfomed = true;
}

svoid handleInterrupt() {
  noInterrupts();
  if (STATE == State::SLEEP) {
    sleep_count_time = millis();
    setState(State::INIT);
  }

  interrupts();
}

svoid shutdownPeripherals() {
  display.clear();
  digitalWrite(LED_BUILTIN, 0);
}
