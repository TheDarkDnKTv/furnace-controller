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


void setup() {
  pinMode(LED_BUILTIN, OUTPUT); // TODO remove

  {
    BTN_LEFT = OneButton(CONTROL_LEFT, true, true);
    BTN_MAIN = OneButton(CONTROL_CENTER, true, true);
    BTN_RIGHT = OneButton(CONTROL_RIGHT, true, true);

    BTN_LEFT.attachClick([]() { handleControlClick(Control::LEFT); });
    BTN_MAIN.attachClick([]() { handleControlClick(Control::MAIN); });
    BTN_RIGHT.attachClick([]() { handleControlClick(Control::RIGHT); });
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
      SEG_B | SEG_C,
      SEG_A | SEG_B | SEG_G | SEG_E | SEG_D,
      SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,
      SEG_F | SEG_G | SEG_B | SEG_C,
      SEG_A | SEG_F | SEG_G | SEG_C | SEG_D
    }, 5);
    break;
  }
  case SETTING_TEMP: {
    
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
    case Control::LEFT:
      break;
    case Control::RIGHT:
      break;
    case Control::MAIN: {
      

      break;
    }
  }
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
