#include <inttypes.h>
#pragma once

enum class SensorStatus : uint8_t {
    OK,
    TOO_LOW,
    TOO_HIGH
};

/**
 * temperature is only meaningful when ok() is true
 */
struct SensorReading {
    SensorStatus status;
    int16_t temperature;

    bool ok() const {
        return status == SensorStatus::OK;
    }
};

class Sensor {
        public:
            /**
             * @param pin board's analog pin number
            */
            Sensor(uint8_t pin) {
                this->pin = pin;
            }

            /**
             * Read current temperature in 0.1 degrees celsium
             */
            virtual SensorReading read() = 0;

            /**
             * Forget previous samples, call on session start
             */
            virtual void reset() = 0;

        protected:
            uint8_t pin;
    };
