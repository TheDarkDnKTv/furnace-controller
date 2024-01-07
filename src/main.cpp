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

static uint32_t display_blink_time;
static bool did_blink_off = false;
static uint32_t last_operation_time;
static uint32_t last_timer_time;

uint16_t temperature = MIN_TEMP;
uint8_t time_hours = TIME_HOURS_DEFAULT;
uint8_t time_minutes = TIME_MINUTES_DEFAULT;
uint16_t operation_minute_countdown;

void setup() {
  pinMode(DRIVER_MAIN_RELAY, OUTPUT);
  pinMode(DRIVER_FAN, OUTPUT);
  pinMode(DRIVER_HEATER_TOP, OUTPUT);
  pinMode(DRIVER_HEATER_BOTTOM, OUTPUT);
  pinMode(HEATING_INDICATOR, OUTPUT);

  {
    BTN_LEFT = OneButton(CONTROL_LEFT, true, true);
    BTN_MAIN = OneButton(CONTROL_CENTER, true, true);
    BTN_RIGHT = OneButton(CONTROL_RIGHT, true, true);

    BTN_LEFT.attachClick([]() { handleControlClick(Control::LEFT); });
    BTN_MAIN.attachClick([]() { handleControlClick(Control::MAIN); });
    BTN_RIGHT.attachClick([]() { handleControlClick(Control::RIGHT); });

    BTN_MAIN.attachLongPressStart([]() { 
      action_perfomed = true;
      stopOperation();
    });
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
  display_blink_time = millis();
}

void loop() {
  uint32_t time = millis();
  updateInputs();
  updateOperationControl(&time);

  if (action_perfomed) {
    display_blink_time = time;
    sleep_count_time = time;
  }

  if (time - display_blink_time >= 500) { 
    display_blink_time = time;
    did_blink_off = !did_blink_off;
    updateScreen();
  }

  display.update(&time);

  if (STATE != State::IN_OPERATION && time >= SLEEP_TIMEOUT + sleep_count_time) {
    setState(State::SLEEP);
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
    }
    case SETTING_TIMER_HOURS: {
      time_hours = TIME_HOURS_DEFAULT;
      time_minutes = TIME_MINUTES_DEFAULT;
      updateScreen();
      break;
    }
    case SETTING_TIMER_MINUTES: {
      updateScreen();
      break;
    }
    case SLEEP: {
      shutdownPeripherals();
      delay(50);
      set_sleep_mode(SLEEP_MODE_PWR_DOWN);
      sleep_mode();
      break;
    }
    case IN_OPERATION: {
      if (time_hours <= 0 && time_minutes <= 0) {
        setState(INIT);
      }

      last_timer_time = last_operation_time = millis();
      operation_minute_countdown = 0;
      digitalWrite(DRIVER_MAIN_RELAY, 1);
      digitalWrite(DRIVER_FAN, 1);
      break;
    }
  }
}

svoid updateScreen() {
  switch (STATE) {
    case State::SETTING_TEMP: {
      display.setDecimal(temperature / 10);
      break;
    }
    case State::IN_OPERATION:
    case State::SETTING_TIMER_HOURS:
    case State::SETTING_TIMER_MINUTES: {
      display.setDecimal(time_hours * 100 + time_minutes, true);
      break;
    }
    default: break;
  }

  updateScreenBlink();
}

svoid updateScreenBlink() {
  if (did_blink_off) {
    switch (STATE) {
      case State::SETTING_TIMER_HOURS: {
        display.updateDigitSegments(0, 0);
        display.updateDigitSegments(0, 1);
        break;
      }
      case State::SETTING_TIMER_MINUTES: {
        display.updateDigitSegments(0, 2);
        display.updateDigitSegments(0, 3);
        break;
      }
      case State::IN_OPERATION: {
        display.updateDigitSegments(SEG_DP, 1, false);
        break;
      }
      default: break;
    }
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
      } else if (STATE == State::SETTING_TIMER_HOURS) {
        time_hours = max(TIME_HOURS_DEFAULT, time_hours - 1);
        updateScreen();
      } else if (STATE == State::SETTING_TIMER_MINUTES) {
        time_minutes = max(TIME_MINUTES_DEFAULT, time_minutes - 1);
        updateScreen();
      }

      break;
    }
    case Control::RIGHT: {
      if (STATE == State::SETTING_TEMP) {
        temperature = min(MAX_TEMP, temperature + TEMP_CHANGE_STEP);
        updateScreen();
      } else if (STATE == State::SETTING_TIMER_HOURS) {
        time_hours = min(TIME_HOURS_MAX, time_hours + 1);
        updateScreen();
      } else if (STATE == State::SETTING_TIMER_MINUTES) {
        time_minutes = min(TIME_MINUTES_MAX, time_minutes + 1);
        updateScreen();
      }

      break;
    }
    case Control::MAIN: {
      if (STATE == State::INIT) {
        setState(State::SETTING_TEMP);
      } else if (STATE == State::SETTING_TEMP) {
        setState(State::SETTING_TIMER_HOURS);
      } else if (STATE == State::SETTING_TIMER_HOURS) {
        setState(State::SETTING_TIMER_MINUTES);
      } else if (STATE == State::SETTING_TIMER_MINUTES) {
        setState(State::IN_OPERATION);
      }
      break;
    }
  }
}

svoid handleChangeLongPress(bool increase) {
  switch (STATE) {
    case State::SETTING_TEMP: {
      if (increase) {
        temperature = min(MAX_TEMP, temperature + TEMP_CHANGE_STEP_FAST);
      } else {
        temperature = max(MIN_TEMP, temperature - TEMP_CHANGE_STEP_FAST);
      }

      break;
    }
    case State::SETTING_TIMER_HOURS: {
      if (increase) {
        time_hours = min(TIME_HOURS_MAX, time_hours + TIME_CHANGE_STEP_FAST);
      } else {
        time_hours = max(TIME_HOURS_DEFAULT, time_hours - TIME_CHANGE_STEP_FAST);
      }

      break;
    }
    case State::SETTING_TIMER_MINUTES: {
      if (increase) {
        time_minutes = min(TIME_MINUTES_MAX, time_minutes + TIME_CHANGE_STEP_FAST);
      } else {
        time_minutes = max(TIME_MINUTES_DEFAULT, time_minutes - TIME_CHANGE_STEP_FAST);
      }

      break;
    }
    default: break;
  }

  updateScreen();
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

svoid stopOperation() {
  // Disabling hardware devices
  digitalWrite(DRIVER_MAIN_RELAY, 0);
  digitalWrite(DRIVER_FAN, 0);
  setHeating(0);

  setState(State::INIT);
}

svoid updateOperationControl(uint32_t* time) {
  if (STATE == IN_OPERATION) {
    updateOperationTimer(*time - last_timer_time);
    last_timer_time = *time;
    if (*time - last_operation_time >= OP_CHECK_INTERVAL) {
      uint32_t measured_temp = sensor->getTemperature();
      
      Serial.print("Temperature: ");
      Serial.println(measured_temp / 10);

      if (measured_temp >= temperature) {
        setHeating(false);
      } else if (measured_temp + TERMOMETER_THRESHOLD < temperature) {
        setHeating(true);
      }

      last_operation_time = *time;
    }
  }
}

svoid updateOperationTimer(uint16_t time_passed) {
    if (time_passed <= operation_minute_countdown) {
      operation_minute_countdown -= time_passed;
    } else {
        if (time_minutes <= 0) {
          if (time_hours <= 0) {
            action_perfomed = true;
            stopOperation();
            return;
          }

          time_hours--;
          time_minutes = 60; // Minutes in one hours
        }

        time_minutes--;
        time_passed = time_passed - operation_minute_countdown;
        operation_minute_countdown = time_passed + ((uint16_t)1000 * 60); // Added rest of millis to times
        updateScreen();
    }
}

svoid setHeating(bool enabled) {
  digitalWrite(DRIVER_HEATER_TOP, enabled);
  digitalWrite(DRIVER_HEATER_BOTTOM, enabled);
  digitalWrite(HEATING_INDICATOR, enabled);
}
