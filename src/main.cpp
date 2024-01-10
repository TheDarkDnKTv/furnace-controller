#include <settings.hh>
#include <main.hh>
#include <sensor.hh>
#include <sensor.cpp>
#include <display.hh>
#include <OneButton.h>

/**
 * Hardware controller for TOSHIBA furnace
 * 
 * All settings are located in settings.hh file
 * 
 * @version 1.2
 * @author TheDarkDnKTv
 * @since 2024/01/09
*/

Display display = Display(1000);
Sensor* sensor;

static OneButton BTN_LEFT;
static OneButton BTN_MAIN;
static OneButton BTN_RIGHT;
static bool action_perfomed;

volatile State STATE;
volatile uint32_t sleep_count_time;

volatile uint8_t sync_tryies = 4;
volatile uint16_t sync_period = 0;
volatile uint32_t temp_sync_last_time = 0;
volatile uint16_t termoresistor_temp = 0;
volatile bool do_check_temp = false;

static uint32_t display_blink_time;
static bool did_blink_off = false;
static bool do_blink = true;
static uint32_t last_operation_time;
static uint32_t last_timer_time;
static bool showing_time = true;

static uint32_t last_fan_switch;

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
  pinMode(TEMPERATURE_SYNC, INPUT);
  pinMode(POWER_STANDBY, INPUT);

  {
    BTN_LEFT = OneButton(CONTROL_LEFT, true, true);
    BTN_MAIN = OneButton(CONTROL_CENTER, true, true);
    BTN_RIGHT = OneButton(CONTROL_RIGHT, true, true);

    BTN_LEFT.attachClick([]() { handleControlClick(Control::LEFT); });
    BTN_MAIN.attachClick([]() { handleControlClick(Control::MAIN); });
    BTN_RIGHT.attachClick([]() { handleControlClick(Control::RIGHT); });

    void (*noBlink)(void) = []() {
      do_blink = false;
    };

    void (*doBlink)(void) = []() {
      do_blink = true;
    };

    BTN_MAIN.attachLongPressStart([]() { 
      action_perfomed = true;
      stopOperation();
    });
    BTN_LEFT.attachDuringLongPress([]() { handleChangeLongPress(false); });
    BTN_LEFT.attachLongPressStart(noBlink);
    BTN_LEFT.attachLongPressStop(doBlink);
    BTN_RIGHT.attachDuringLongPress([]() { handleChangeLongPress(true); });
    BTN_RIGHT.attachLongPressStart(noBlink);
    BTN_RIGHT.attachLongPressStop(doBlink);

    BTN_LEFT.setLongPressIntervalMs(LONG_PRESS_INTERVAL);
    BTN_RIGHT.setLongPressIntervalMs(LONG_PRESS_INTERVAL);

    BTN_MAIN.attachDoubleClick([]{
      if (STATE == IN_OPERATION) {
        showing_time = !showing_time;
      }
    });

    BTN_MAIN.setClickMs(150);
  }

  sensor = new ToshibaSensor(TEMPERATURE_SENSOR);

  attachInterrupt(0, handleSync, FALLING);
  attachInterrupt(1, handleInterrupt, FALLING);

  #ifdef DEBUG
    Serial.begin(9600);
  #endif
  
  display.setBrightness(7);
  display.clear();

  setState(State::INIT);
  sleep_count_time = millis();
  display_blink_time = millis();
}

void loop() {
  uint32_t time = millis();
  // Need to check temp ASAP
  if ((STATE == IN_OPERATION || STATE == COOLING) && do_check_temp && time - temp_sync_last_time >= sync_period) {
    termoresistor_temp = sensor->getTemperature();
    do_check_temp = false;
  }

  updateInputs();
  updateOperationControl(&time);

  if (action_perfomed) {
    resetBlinking();
    sleep_count_time = time;
  }

  if ((do_blink || STATE == State::IN_OPERATION) && time - display_blink_time >= 500) { 
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
      showing_time = true;
      setAuxiliaryHardware(0);
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
      setAuxiliaryHardware(1);
      break;
    }
    case COOLING: {
      display.setSegments((const uint8_t[]) {
        SEG_A | SEG_E | SEG_F | SEG_D,
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
        SEG_E | SEG_F | SEG_D
      }, 4);
      setCooling(1);
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
      if (!showing_time) {
        display.setFloat(termoresistor_temp / 10.0);
        return;
      }
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
        if (showing_time)
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
  resetBlinking();
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
        if (sync_tryies <= 0) {
          setState(State::SETTING_TEMP);
        }
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

  resetBlinking();
  updateScreen();
  action_perfomed = true;
}

svoid handleInterrupt() {
  noInterrupts();
  if (STATE == State::SLEEP) {
    sleep_count_time = millis();
    attachInterrupt(0, handleSync, FALLING);
    setState(State::INIT);
  }

  interrupts();
}

svoid handleSync() {
  if (sync_tryies == 0 || temp_sync_last_time <= 0) {
    temp_sync_last_time = millis();
    do_check_temp = true;
    return;
  }

  if (sync_tryies-- > 0) {
    sync_period += (millis() - temp_sync_last_time);
    temp_sync_last_time = millis();
    if (sync_tryies <= 0) {
      sync_period /= 4;
      #ifdef DEBUG
        Serial.print("Temperature sync done, period: ");
        Serial.print(sync_period);
        Serial.println("ms");
      #endif
      sync_period /= 2 * 3; // 1/3 of half period
    }
  }
}

svoid shutdownPeripherals() {
  display.clear();
  digitalWrite(LED_BUILTIN, 0);
  detachInterrupt(0);
}

svoid stopOperation() {
  // Disabling hardware devices
  setHeating(0);

  if (termoresistor_temp > NEED_COOLING_FROM) {
    setState(State::COOLING);  
  } else {
    setAuxiliaryHardware(0);
    setState(State::INIT);
  }
}

static inline int8_t signum(int16_t val) {
  return val > 0 ? 1 : (val < 0 ? -1 : 0);
}

svoid updateOperationControl(uint32_t* time) {
  if (STATE == IN_OPERATION) {
    if (millis() - last_fan_switch >= 30) {
      last_fan_switch = millis() - 15;
      digitalWrite(DRIVER_FAN, !digitalRead(DRIVER_FAN));
    }

    updateOperationTimer(*time - last_timer_time);
    last_timer_time = *time;
    if (*time - last_operation_time >= OP_CHECK_INTERVAL) {
      last_operation_time = *time;
      if (!showing_time) {
        updateScreen();
      }
      
      static uint16_t previous_temperature = termoresistor_temp;
      static bool heater_throttling = false;
      static int8_t heater_delay = 0;

      int16_t rate = (int16_t)termoresistor_temp - (int16_t)previous_temperature;
      previous_temperature = termoresistor_temp;

      int16_t change_mod = rate < 0 ? (OP_TEMP_CONTROL_RATIO / 3) : OP_TEMP_CONTROL_RATIO; // cooling way slower than heating
      int16_t expected_temp = termoresistor_temp + rate * change_mod;
      int8_t signal = (
        (int8_t)signum(expected_temp - (int16_t)temperature - OP_TEMP_HYSTERESIS / 2) +
        (int8_t)signum(expected_temp - (int16_t)temperature + OP_TEMP_HYSTERESIS / 2)
      ) / 2;

      if (signal <= 0) {
        // Do throttle if heater enabled and temperature haven't been changed for some cycles
        if (heater_throttling) {
          heater_throttling = --heater_delay > -3;
          if (!heater_throttling)
            heater_delay = 0;
        } else {
          if (rate <= 2) { // lower than 0.2C per interval
            heater_throttling = ++heater_delay >= 4;
          } else {
            heater_delay = 0;
          }
        }
      } else {
        heater_throttling = false;
        heater_delay = 0;
      }

      if (heater_throttling) {
        signal = 1;
      }

      #ifdef DEBUG
        Serial.print("Set: ");
        Serial.print(temperature / 10.0);
        Serial.print(" # Expected: ");
        Serial.print(expected_temp / 10.0);
        Serial.print(" # Rate: ");
        Serial.print(rate / 10.0);
        Serial.print(" # throttle: ");
        Serial.println(heater_throttling);
      #endif

      if (signal == 1) {
        setHeating(false);
      } else if (signal == -1) {
        setHeating(true);
      }
    }
  } else if (STATE == COOLING) {
    if (termoresistor_temp <= NEED_COOLING_FROM) {
      setCooling(0);
      setState(INIT);
    } else {
      #ifdef DEBUG
      Serial.print("Cooling: ");
      Serial.println(termoresistor_temp);
      #endif
    }

    action_perfomed = true;
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
  #ifdef DEBUG
    Serial.print("Heating: ");
    Serial.println(enabled);
  #endif

  digitalWrite(DRIVER_HEATER_TOP, enabled);
  digitalWrite(DRIVER_HEATER_BOTTOM, enabled);
  digitalWrite(HEATING_INDICATOR, enabled);
}

svoid setAuxiliaryHardware(bool enabled) {
  digitalWrite(DRIVER_MAIN_RELAY, enabled);
  digitalWrite(POWER_STANDBY, enabled);
  digitalWrite(DRIVER_FAN, enabled);
}

svoid resetBlinking() {
  if (STATE == State::IN_OPERATION) {
    return;
  }

  did_blink_off = false;
  display_blink_time = millis();
}

svoid setCooling(bool enabled) {
  digitalWrite(DRIVER_MAIN_RELAY, 0);
  digitalWrite(POWER_STANDBY, enabled);
  digitalWrite(DRIVER_FAN, enabled);
}
