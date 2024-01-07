#include <TM1637Display.h>
#include <settings.hh>

#define DIGIT_0 0b1000
#define DIGIT_1 0b0100
#define DIGIT_2 0b0010
#define DIGIT_3 0b0001

#define DIGIT_ALL \
    DIGIT_0 |\
    DIGIT_1 |\
    DIGIT_2 |\
    DIGIT_3


const uint8_t SEGMENTS_DISABLED[] = {
    0, 0, 0, 0
};

static inline void arrcpy(const uint8_t* src, uint8_t* dest, uint8_t count, uint8_t src_pos = 0, uint8_t dest_pos = 0) {
    memcpy(
        dest + dest_pos,
        src + src_pos,
        count
    );
}

class Display {
    public:
        /**
         * @param shift_interval text shift interval in ms
        */
        Display(uint16_t shift_interval = 1000) {
            this->segments = nullptr;
            this->shift_interval = shift_interval;
        }

        ~Display() {
            free(this->segments);
        };

        void update(uint32_t time) {
            if (segments != nullptr) {
                if (need_update) {
                    offset = 0;
                    need_update = false;

                    display.setSegments(segments, length);
                } else if (length > 4 && time >= last_update_time + shift_interval) {
                    last_update_time = time;
                    if ((++offset + 4) > length) {
                        offset = 0;
                    }

                    display.setSegments(segments + offset);
                }
            } else if (need_update) {
                need_update = false;
                clear();
            }
        }

        void setSegments(const uint8_t segments[], uint8_t lenght) {
            offset = 0;
            need_update = true;
            this->length = lenght;

            free(this->segments);
            this->segments = (uint8_t*) calloc(4, sizeof(uint8_t));
            if (this->segments == nullptr) {
                Serial.println("ERR malloc failed: setSegments()");
                return;
            }

            memcpy(this->segments, segments, sizeof(uint8_t) * lenght);
        }
        
        void setDecimal(uint16_t number, bool leading_zeros = false) {
            this->offset = 0;
            this->need_update = true;
            
            uint8_t i = 5;
            uint8_t segments[i];
            while (number > 0 && i > 0) {
                segments[i - 1] = display.encodeDigit(number % 10);
                number /= 10;
                i--;
            }

            uint8_t len = 5 - i;
            if (leading_zeros && len < 4) {
                for (int8_t j = 0; j < 5 - len; j++) {
                    segments[j] = display.encodeDigit(0);
                }

                len = 4;
            }

            if (len != this->length) {
                free(this->segments);
                this->length = max(4, len);
                this->segments = (uint8_t*) calloc(this->length, sizeof(uint8_t));
            }

            arrcpy(
                segments,
                this->segments,
                sizeof(uint8_t) * len,
                len < 5 ? 5 - len : 0,
                len < 4 ? 4 - len : 0
            );
        }

        // void 

        void clear() {
            free(segments);
            segments = nullptr;
            length = 0;
            offset = 0;
            display.clear();
        }

        void setBrightness(uint8_t brightness) {
            display.setBrightness(brightness, brightness > 0);
        }

    private:
        TM1637Display display = TM1637Display(DISPLAY_CLOCK, DISPLAY_DATA);

        bool need_update = false;

        uint8_t* segments = nullptr;
        uint8_t length;
        uint8_t offset;

        uint32_t last_update_time;
        uint16_t shift_interval;
};