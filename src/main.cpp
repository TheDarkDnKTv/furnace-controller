#include <settings.hh>
#include <main.hh>
#include <sensor.hh>
#include <sensor.cpp>
#include <display.hh>
#include <OneButton.h>
#include <util/atomic.h>

/**
 * Hardware controller for TOSHIBA furnace
 * 
 * All settings are located in settings.hh file
 * 
 * @version 1.2.0
 * @author TheDarkDnKTv
 * @since 2024/01/09
*/

#define useFan() (temperature > FAN_ENABLE_TEMP)

Display display(1000);
Sensor* sensor;

static OneButton BTN_LEFT;
static OneButton BTN_MAIN;
static OneButton BTN_RIGHT;
static bool action_perfomed;

volatile State STATE;
volatile uint32_t sleep_count_time;

volatile uint8_t sync_tryies = 4;
volatile uint16_t sync_period = 0;
volatile uint16_t sync_full_period = 0;
volatile uint32_t temp_sync_last_time = 0;
volatile uint16_t termoresistor_temp = 0;
volatile bool do_check_temp = false;
static bool temp_valid = false;
static uint32_t last_read_time;
static uint32_t last_valid_read_time;
static int16_t session_lowest_temp;

static Fault fault = Fault::NONE;
static uint32_t fault_since;
static bool fault_hot_at_entry;
static bool fault_cooling;

static uint32_t display_blink_time;
static bool did_blink_off = false;
static bool do_blink = true;
static uint32_t last_operation_time;
static uint32_t last_timer_time;
static bool showing_time = true;
static bool fine_step = false;

static uint32_t last_fan_switch;

/**
 * The only "now" for main-loop code, taken once per iteration so a timestamp stored
 * during the iteration can never be newer than the time it is later compared against
 */
static uint32_t loop_time;

#ifdef DEBUG
  static bool sync_reported = false;
  static uint16_t last_cooling_report;
#endif

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
      } else if (STATE == SETTING_TEMP) {
        fine_step = !fine_step;
        action_perfomed = true;
        resetBlinking();
        updateScreen();
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

  loadStoredFault();
  if (fault == Fault::NONE) {
    showVersion();
    setState(State::INIT);
  } else {
    setState(State::FAULT);
  }

  sleep_count_time = millis();
  display_blink_time = millis();
  enableWatchdog();
}

#ifdef LOOP_TIMING
  static uint32_t timing_sum_us = 0;
  static uint32_t timing_max_us = 0;
  static uint16_t timing_count = 0;
  static uint32_t timing_report_time = 0;
#endif

void loop() {
  wdt_reset();
  loop_time = millis();
  #ifdef LOOP_TIMING
    uint32_t timing_start_us = micros();
  #endif
  #ifdef DEBUG
    if (!sync_reported && sync_tryies == 0) {
      sync_reported = true;
      Serial.print("Temperature sync done, period: ");
      Serial.print(sync_full_period);
      Serial.println("ms");
    }
  #endif

  // Need to check temp ASAP
  if (STATE == IN_OPERATION || STATE == COOLING || STATE == FAULT) {
    updateTemperature();
  }

  if (STATE == IN_OPERATION || STATE == COOLING) {
    checkFaults();
  }

  updateInputs();
  if (STATE == FAULT) {
    updateFault();
  }

  updateOperationControl();

  if (action_perfomed) {
    resetBlinking();
    sleep_count_time = loop_time;
  }

  if ((do_blink || STATE == State::IN_OPERATION || STATE == State::FAULT) && loop_time - display_blink_time >= 500) { 
    display_blink_time = loop_time;
    did_blink_off = !did_blink_off;
    updateScreen();
  }

  display.update(loop_time);

  #ifdef LOOP_TIMING
    uint32_t timing_us = micros() - timing_start_us;
    timing_sum_us += timing_us;
    timing_count++;
    if (timing_us > timing_max_us) {
      timing_max_us = timing_us;
    }

    if (loop_time - timing_report_time >= 5000) {
      Serial.print(F("L,state="));
      Serial.print(STATE);
      Serial.print(F(",n="));
      Serial.print(timing_count);
      Serial.print(F(",avg_us="));
      Serial.print(timing_sum_us / timing_count);
      Serial.print(F(",max_us="));
      Serial.println(timing_max_us);
      timing_report_time = loop_time;
      timing_sum_us = 0;
      timing_max_us = 0;
      timing_count = 0;
    }
  #endif

  if (STATE != State::IN_OPERATION && STATE != State::FAULT && loop_time - sleep_count_time >= SLEEP_TIMEOUT) {
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
      fine_step = false;
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
      wdt_disable();
      set_sleep_mode(SLEEP_MODE_PWR_DOWN);
      sleep_mode();
      enableWatchdog();
      loop_time = millis();
      sleep_count_time = loop_time;
      attachInterrupt(0, handleSync, FALLING);
      setState(State::INIT);
      break;
    }
    case IN_OPERATION: {
      if (time_hours <= 0 && time_minutes <= 0) {
        setState(INIT);
        return;
      }

      last_timer_time = last_operation_time = loop_time;
      operation_minute_countdown = 0;
      temp_valid = false;
      last_valid_read_time = loop_time;
      session_lowest_temp = INT16_MAX;
      sensor->reset();
      EEPROM.update(EEPROM_SESSION_RUNNING, 1);
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
      last_valid_read_time = loop_time;
      #ifdef DEBUG
        last_cooling_report = 0;
      #endif

      setCooling(1);
      break;
    }
    case FAULT: {
      updateScreen();
      break;
    }
  }
}

svoid updateScreen() {
  switch (STATE) {
    case State::SETTING_TEMP: {
      display.setDecimal(temperature / 10);
      if (fine_step && !did_blink_off) {
        display.updateDigitSegments(SEG_DP, 0, false);
        display.updateDigitSegments(SEG_DP, 3, false);
      }

      break;
    }
    case State::IN_OPERATION:
      if (!showing_time) {
        if (temp_valid) {
          display.setFloat(termoresistor_temp / 10.0);
        } else {
          display.setSegments((const uint8_t[]) { SEG_G, SEG_G | SEG_DP, SEG_G, SEG_G }, 4);
        }

        return;
      }
    case State::SETTING_TIMER_HOURS:
    case State::SETTING_TIMER_MINUTES: {
      display.setDecimal(time_hours * 100 + time_minutes, true);
      break;
    }
    case State::FAULT: {
      uint8_t segments[] = { 0, SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, display.encodeDigit((uint8_t) fault), 0 };
      display.setSegments(segments, 4);
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
      case State::FAULT: {
        for (uint8_t i = 0; i < 4; i++) {
          display.updateDigitSegments(0, i);
        }

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

static inline int16_t temperatureStep() {
  return fine_step ? TEMP_CHANGE_STEP_FINE : TEMP_CHANGE_STEP;
}

svoid handleControlClick(Control control) {
  action_perfomed = true;
  resetBlinking();
  switch (control) {
    case Control::LEFT: {
      if (STATE == State::SETTING_TEMP) {
        temperature = max(MIN_TEMP, (int16_t) temperature - temperatureStep());
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
        temperature = min(MAX_TEMP, (int16_t) temperature + temperatureStep());
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
        if (time_hours > 0 || time_minutes > 0) {
          setState(State::IN_OPERATION);
        }
      }
      break;
    }
  }
}

svoid handleChangeLongPress(bool increase) {
  switch (STATE) {
    case State::SETTING_TEMP: {
      int16_t step = fine_step ? TEMP_CHANGE_STEP : TEMP_CHANGE_STEP_FAST;
      if (increase) {
        temperature = min(MAX_TEMP, (int16_t) temperature + step);
      } else {
        temperature = max(MIN_TEMP, (int16_t) temperature - step);
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

/**
 * Only wakes the CPU from deep sleep, the wake-up work runs right after sleep_mode()
 */
svoid handleInterrupt() {
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
      sync_full_period = sync_period;
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
  if (STATE == State::FAULT) {
    return;
  }

  // Disabling hardware devices
  setHeating(0);
  EEPROM.update(EEPROM_SESSION_RUNNING, 0);

  if (termoresistor_temp > NEED_COOLING_FROM) {
    setState(State::COOLING);  
  } else {
    setAuxiliaryHardware(0);
    setState(State::INIT);
  }
}

/**
 * Reads right after a main board pulse when pulses come, on own timer when they stop.
 * The pulse time comes from the ISR and can be newer than loop_time, so it is compared
 * against a fresh millis() read together with it
 */
svoid updateTemperature() {
  uint32_t now;
  uint32_t sync_last_time;
  bool sync_pending;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    now = millis();
    sync_last_time = temp_sync_last_time;
    sync_pending = do_check_temp;
  }

  if (sync_pending) {
    if (now - sync_last_time >= sync_period) {
      do_check_temp = false;
      readTemperature();
    }
  } else if (now - sync_last_time >= SYNC_LOST_TIMEOUT && now - last_read_time >= SENSOR_READ_INTERVAL) {
    readTemperature();
  }
}

svoid readTemperature() {
  last_read_time = loop_time;
  SensorReading reading = sensor->read();
  if (reading.ok()) {
    termoresistor_temp = reading.temperature;
    temp_valid = true;
    last_valid_read_time = loop_time;
    if (reading.temperature < session_lowest_temp) {
      session_lowest_temp = reading.temperature;
    }
  }
}

svoid showVersion() {
  uint8_t segments[] = {
    0,
    (uint8_t) (display.encodeDigit(VERSION_MAJOR) | SEG_DP),
    (uint8_t) (display.encodeDigit(VERSION_MINOR) | SEG_DP),
    display.encodeDigit(VERSION_PATCH)
  };
  display.setSegments(segments, 4);
  display.update(millis());
  delay(VERSION_DISPLAY_TIME);
}

svoid enableWatchdog() {
  wdt_enable(WDTO_2S);
  WDTCSR |= _BV(WDIE);
}

/**
 * The first watchdog timeout runs this instead of resetting: last chance to cut
 * the heater and record E5 before forcing the reset
 */
ISR(WDT_vect) {
  digitalWrite(DRIVER_HEATER_TOP, 0);
  digitalWrite(DRIVER_HEATER_BOTTOM, 0);
  digitalWrite(HEATING_INDICATOR, 0);
  digitalWrite(DRIVER_MAIN_RELAY, 0);
  EEPROM.update(EEPROM_FAULT_CODE, (uint8_t) Fault::WATCHDOG);
  EEPROM.update(EEPROM_SESSION_RUNNING, 0);
  wdt_enable(WDTO_15MS);
  while (true) {
  }
}

/**
 * A session still marked as running at boot means power was lost mid-session
 */
svoid loadStoredFault() {
  uint8_t code = EEPROM.read(EEPROM_FAULT_CODE);
  if (code == 0xFF) {
    code = (uint8_t) Fault::NONE;
  }

  if (code == (uint8_t) Fault::NONE && EEPROM.read(EEPROM_SESSION_RUNNING) == 1) {
    code = (uint8_t) Fault::POWER_LOSS;
    EEPROM.update(EEPROM_FAULT_CODE, code);
  }

  EEPROM.update(EEPROM_SESSION_RUNNING, 0);
  fault = (Fault) code;
  fault_since = millis();
  fault_hot_at_entry = false;
  fault_cooling = false;
}

svoid checkFaults() {
  if (loop_time - last_valid_read_time > SENSOR_FAULT_TIMEOUT) {
    enterFault(Fault::SENSOR_LOST);
    return;
  }

  if (!temp_valid) {
    return;
  }

  int16_t temp = termoresistor_temp;
  int16_t overheat_limit = max((int16_t) temperature, session_lowest_temp) + OVERHEAT_MARGIN;
  if (temp > OVERHEAT_ABSOLUTE_TEMP) {
    enterFault(Fault::OVERHEAT_ABSOLUTE);
  } else if (STATE == State::IN_OPERATION && temp > overheat_limit) {
    enterFault(Fault::OVERHEAT);
  }
}

/**
 * EEPROM is written before releasing standby, as releasing it may cut our own power
 */
svoid enterFault(Fault code) {
  setHeating(0);
  digitalWrite(DRIVER_MAIN_RELAY, 0);
  EEPROM.update(EEPROM_FAULT_CODE, (uint8_t) code);
  EEPROM.update(EEPROM_SESSION_RUNNING, 0);
  fault = code;
  fault_since = loop_time;
  fault_hot_at_entry = temp_valid && termoresistor_temp > NEED_COOLING_FROM;
  fault_cooling = fault_hot_at_entry;
  setPowerStandby(fault_cooling);
  digitalWrite(DRIVER_FAN, fault_cooling);
  setState(State::FAULT);
}

/**
 * Keeps the fan on while hot to protect the power supply; with no sensor, runs it
 * for a fixed time if the last good reading was hot
 */
svoid updateFault() {
  bool sensor_lost = !temp_valid || loop_time - last_valid_read_time > SENSOR_FAULT_TIMEOUT;
  bool cooling;
  if (sensor_lost) {
    cooling = fault_hot_at_entry && loop_time - fault_since < FAULT_FAN_TIME;
  } else {
    int16_t threshold = fault_cooling ? NEED_COOLING_FROM - FAULT_COOLING_HYSTERESIS : NEED_COOLING_FROM;
    cooling = (int16_t) termoresistor_temp > threshold;
  }

  if (cooling != fault_cooling) {
    fault_cooling = cooling;
    setPowerStandby(cooling);
    digitalWrite(DRIVER_FAN, cooling);
  }

  if (
    BTN_LEFT.isLongPressed() && BTN_RIGHT.isLongPressed() &&
    BTN_LEFT.getPressedMs() >= FAULT_CLEAR_HOLD && BTN_RIGHT.getPressedMs() >= FAULT_CLEAR_HOLD
  ) {
    clearFault();
  }
}

svoid clearFault() {
  EEPROM.update(EEPROM_FAULT_CODE, (uint8_t) Fault::NONE);
  fault = Fault::NONE;
  action_perfomed = true;
  if (fault_cooling) {
    setState(State::COOLING);
  } else {
    setAuxiliaryHardware(0);
    setState(State::INIT);
  }
}

static inline int8_t signum(int16_t val) {
  return val > 0 ? 1 : (val < 0 ? -1 : 0);
}

svoid updateOperationControl() {
  if (STATE == IN_OPERATION) {
    if (useFan() && loop_time - last_fan_switch >= 30) {
      last_fan_switch = loop_time - 15;
      digitalWrite(DRIVER_FAN, !digitalRead(DRIVER_FAN));
    }

    updateOperationTimer(loop_time - last_timer_time);
    last_timer_time = loop_time;
    if (temp_valid && loop_time - last_operation_time >= OP_CHECK_INTERVAL) {
      last_operation_time = loop_time;
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
        if (termoresistor_temp != last_cooling_report) {
          last_cooling_report = termoresistor_temp;
          Serial.print("Cooling: ");
          Serial.println(termoresistor_temp);
        }
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
      operation_minute_countdown = 60000 - time_passed; // Overshoot already belongs to the next minute
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
  setPowerStandby(enabled);
  if (!enabled || useFan()) {
    digitalWrite(DRIVER_FAN, enabled);
  }
}

svoid resetBlinking() {
  if (STATE == State::IN_OPERATION) {
    return;
  }

  did_blink_off = false;
  display_blink_time = loop_time;
}

svoid setCooling(bool enabled) {
  digitalWrite(DRIVER_MAIN_RELAY, 0);
  setPowerStandby(enabled);
  digitalWrite(DRIVER_FAN, enabled);
}

/**
 * Released means high impedance, not driven LOW, so the main board decides
 * about power exactly as it did when this pin was only a pull-up
 */
svoid setPowerStandby(bool enabled) {
  if (enabled) {
    digitalWrite(POWER_STANDBY, HIGH);
    pinMode(POWER_STANDBY, OUTPUT);
  } else {
    pinMode(POWER_STANDBY, INPUT);
  }
}
