#include <inttypes.h>
#pragma once

class Sensor {
        public:
            /**
             * @param pin board's analog pin number
            */
            Sensor(uint8_t pin) {
                this->pin = pin;
            }

            /**
             * Get current temperature in degrees celsium
             * @return temperature
             */
            virtual int16_t getTemperature();

        protected:
            uint8_t pin;
    };