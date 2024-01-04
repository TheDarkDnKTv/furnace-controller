#include <settings.hh>
#include <main.hh>
#include <sensor.hh>
#include <sensor.cpp>
#include <OneButton.h>
#include <TM1637Display.h>

TM1637Display display = TM1637Display(DISPLAY_CLOCK, DISPLAY_DATA);
Sensor* sensor;

static OneButton BTN_LEFT;
static OneButton BTN_MAIN;
static OneButton BTN_RIGHT;
static bool action_perfomed;

volatile State STATE;
volatile uint32_t sleep_count_time;

uint16_t clicked;

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
  display.showNumberDec(clicked);

  STATE = State::IDLE;
  sleep_count_time = millis();
}

void loop() {
  uint32_t time = millis();
  updateInputs();

  display.showNumberDec(clicked);

  if (STATE != State::IN_OPERATION) {
    if (action_perfomed) {
      sleep_count_time = time;
    }

    if (time >= SLEEP_TIMEOUT + sleep_count_time) {
      hibernate();
    }
  }
}

static void updateInputs() {
  action_perfomed = false;
  BTN_LEFT.tick();
  BTN_MAIN.tick();
  BTN_RIGHT.tick();
}

static void handleControlClick(Control control) {
  action_perfomed = true;
  switch (control) {
    case Control::LEFT:
      clicked--;
      break;
    case Control::RIGHT:
      clicked++;
      break;
    case Control::MAIN:
      clicked = 0;
      Serial.print("Temperature: ");
      Serial.println(sensor->getTemperature());
      break;
  }
}

static void handleInterrupt() {
  noInterrupts();
  if (STATE == State::SLEEP) {
    STATE = State::IDLE;
    sleep_count_time = millis();
  }

  interrupts();
}

static void hibernate() {
  STATE = State::SLEEP;
  display.clear();
  digitalWrite(LED_BUILTIN, 0);

  delay(50);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();
}