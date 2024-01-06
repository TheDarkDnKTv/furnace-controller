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
            this->segments = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * lenght));
            if (this->segments == nullptr) {
                Serial.println("ERR malloc failed: setSegments()");
                return;
            }

            memcpy(this->segments, segments, sizeof(uint8_t) * lenght);
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
