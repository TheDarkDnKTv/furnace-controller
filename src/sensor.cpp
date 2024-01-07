#include <Arduino.h>
#include <settings.hh>
#include <sensor.hh>

class ToshibaSensor : public Sensor {
    public:
        ToshibaSensor(uint8_t pin) : Sensor(pin) {
            pinMode(pin, INPUT);
        }

        /*
        U(out) = U(in) * (R2 / (R1 + R2)) =>
        R2 = ( U(out) * R1 ) / ( U(in) - U(out) )
        */
        uint32_t getTemperature() override {
            uint32_t measured_voltage = this->measureVoltage();

            Serial.print("\n voltage "); // TODO remove it
            Serial.println(measured_voltage);

            uint32_t resistance_kohm = (measured_voltage * TRM_DIVIDER_RESISTANCE) / (TRM_DIVIDER_REF_VOLTAGE - measured_voltage);
            
            Serial.print(" resistance "); // TODO remote it
            Serial.println(resistance_kohm);

            int16_t temperature = floor((resistance_kohm * 1.0F / TRM_BASE_RESISTANCE) * TRM_BASE_RES_DEGREES);
            return max(0, temperature);
        }

    private:
        inline uint32_t measureVoltage() {
            const uint16_t pin_value = analogRead(this->pin);
            const float mod = TRM_DIVIDER_REF_VOLTAGE / 1023.0;
            return (uint32_t) ceil((float) pin_value * mod);
        }
};