#include <Arduino.h>
#include <avr/pgmspace.h>
#include <sensor.hh>
#include <median.hh>

#define TEMPERATURE_TABLE_STEP  -50     // -5.0C
#define TEMPERATURE_MAX         2000    // 200.0C
#define TEMPERATURE_MIN         0       // 0.0C
#define READING_COUNT           8
#define MEDIAN_COUNT            5

// 0 - 200 C
const uint16_t termo_table[] PROGMEM = {
    1012, 1011, 1009, 1008, 1006, 1004, 1002, 1000,
    997, 994, 990, 986, 982, 976, 970, 963,
    955, 946, 935, 922, 908, 891, 872, 850,
    824, 795, 763, 726, 685, 640, 591, 540,
    486, 431, 376, 323, 272, 225, 183, 147,
    115
};

class ToshibaSensor : public Sensor {
    public:
        ToshibaSensor(uint8_t pin) : Sensor(pin) {
            pinMode(pin, INPUT);
        }

        /**
         * https://aterlux.ru/article/ntcresistor
        */
        SensorReading read() override {
            uint8_t high_idx = 0;
            uint8_t low_idx = (sizeof(termo_table) / sizeof(termo_table[0])) - 1;
            uint16_t lowest_value = getTempTableValue(low_idx);
            uint16_t cummulitive_reading = analogRead(this->pin);
            if (cummulitive_reading <= lowest_value) {
                return { SensorStatus::TOO_LOW, 0 };
            }

            uint16_t highest_value = getTempTableValue(0);
            if (cummulitive_reading >= highest_value) {
                return { SensorStatus::TOO_HIGH, 0 };
            }

            // Binary search
            while ((low_idx - high_idx) > 1) {
                uint8_t m = (high_idx + low_idx) >> 1;
                uint16_t mid = getTempTableValue(m);
                if (cummulitive_reading > mid) {
                    low_idx = m;
                } else {
                    high_idx = m;
                }
            }

            uint16_t vl = getTempTableValue(high_idx);
            if (cummulitive_reading >= vl) {
                return { SensorStatus::OK, filter(high_idx * TEMPERATURE_TABLE_STEP + TEMPERATURE_MAX) };
            }

            uint16_t vr = getTempTableValue(low_idx);
            uint16_t vd = vl - vr;
            int16_t result = TEMPERATURE_MAX + low_idx * TEMPERATURE_TABLE_STEP; 
            if (vd) {
                // Linear interpolation
                result -= ((TEMPERATURE_TABLE_STEP * (int32_t)(cummulitive_reading - vr) + (vd >> 1)) / vd);
            }

            return { SensorStatus::OK, filter(result) };
        }

        void reset() override {
            reading = 0;
            reading_count = 0;
            median.reset();
        }

    private:
        uint8_t reading = 0;
        uint8_t reading_count = 0;
        int16_t readings[READING_COUNT];
        MedianFilter<MEDIAN_COUNT> median;

        /**
         * Median drops single-sample spikes (lid glitches), the average then smooths noise
         */
        int16_t filter(int16_t value) {
            readings[reading++] = median.add(value);
            if (reading >= READING_COUNT) {
                reading = 0;
            }

            if (reading_count < READING_COUNT) {
                reading_count++;
            }

            int32_t sum = 0;
            for (uint8_t i = 0; i < reading_count; i++) {
                sum += readings[i];
            }

            return sum / reading_count;
        }

        inline uint16_t getTempTableValue(uint8_t idx) {
            return pgm_read_word(&termo_table[idx]);
        }
};