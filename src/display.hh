#include <TM1637Display.h>
#include <settings.hh>

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
                    
                    Serial.println("arr3: ");
                    for (int k = 0; k < length; k++) {
                        Serial.print(' ');
                        Serial.println(segments[k], BIN);
                    }

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
        
        void setDecimal(uint16_t number) {
            this->offset = 0;
            this->need_update = true;

            if (number <= 0) {
                free(this->segments);
                this->segments = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * 4));
                for (uint8_t i = 0; i < 3; i++) {
                    this->segments[i] = 0;
                }

                this->segments[3] = display.encodeDigit(0);
                this->length = 4;
                return;
            }
            
            uint8_t i = 5;
            uint8_t segments[i];
            while (number > 0 && i > 0) {
                segments[i - 1] = display.encodeDigit(number % 10);
                number /= 10;
                i--;
            }

            free(this->segments);
            uint8_t len = 5 - i;
            this->length = max(4, len);
            this->segments = (uint8_t*) calloc(this->length, sizeof(uint8_t));
            memcpy(
                this->segments + (len < 4 ? 4 - len : 0),
                segments + (len < 5 ? 5 - len : 0),
                sizeof(uint8_t) * len
            );

            
        }

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
