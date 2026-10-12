#pragma once
#include <stdint.h>

inline void insertionSort(int16_t* values, uint8_t count) {
    for (uint8_t i = 1; i < count; i++) {
        int16_t value = values[i];
        uint8_t j = i;
        while (j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            j--;
        }

        values[j] = value;
    }
}

/**
 * Sliding median: a spike can't win unless more than half the window is bad,
 * while a real trend passes through with a delay of SIZE / 2 samples
 */
template <uint8_t SIZE>
class MedianFilter {
    public:
        /**
         * @return median of the last SIZE values, or of all values since reset() while
         * fewer were added; lower middle for an even count
         */
        int16_t add(int16_t value) {
            window[next] = value;
            next = (next + 1) % SIZE;
            if (count < SIZE) {
                count++;
            }

            int16_t sorted[SIZE];
            for (uint8_t i = 0; i < count; i++) {
                sorted[i] = window[i];
            }

            insertionSort(sorted, count);
            return sorted[(count - 1) / 2];
        }

        void reset() {
            next = 0;
            count = 0;
        }

    private:
        int16_t window[SIZE];
        uint8_t next = 0;
        uint8_t count = 0;
};
