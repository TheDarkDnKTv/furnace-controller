#pragma once
#include <math.h>
#include <stdint.h>

struct PidProfile {
    uint32_t windowMs;
    float kp;
    float tiSeconds;
    float integralBand;
};

/**
 * NaN also maps to 0 so a broken calculation can never become a full-on heater pulse
 */
inline float clampUnit(float value) {
    if (!(value > 0.0f)) {
        return 0.0f;
    }

    if (value > 1.0f) {
        return 1.0f;
    }

    return value;
}

/**
 * Pulses too short for the relay are skipped, near-full windows run full
 */
inline uint32_t pulseMs(float duty, uint32_t windowMs, uint32_t minPulseMs) {
    uint32_t onMs = (uint32_t) (clampUnit(duty) * windowMs);
    if (onMs < minPulseMs) {
        return 0;
    }

    if (onMs + minPulseMs > windowMs) {
        return windowMs;
    }

    return onMs;
}

class PidController {
    public:
        /**
         * Call once per window with error = setpoint - temperature in C.
         * I only learns near the setpoint (outside the band P dominates and I would wind
         * up during warm-up) and never while the output is pinned against a limit
         * @return heater duty 0..1
         */
        float update(float error, const PidProfile& profile) {
            return update(error, error, profile);
        }

        /**
         * P reacts to the latest predicted error. I learns from the prediction averaged over
         * the window: the latest one is biased by where the pulse sits in the window, and the
         * plain reading lags so far behind during warm-up that I would wind up
         */
        float update(float proportionalError, float integralError, const PidProfile& profile) {
            float p = profile.kp * proportionalError;
            if (fabsf(integralError) <= profile.integralBand) {
                float next = integral + profile.kp / profile.tiSeconds * integralError * (profile.windowMs / 1000.0f);
                float output = p + next;
                bool saturated = (output > 1.0f && integralError > 0.0f) || (output < 0.0f && integralError < 0.0f);
                if (!saturated) {
                    integral = clampUnit(next);
                }
            }

            return clampUnit(p + integral);
        }

        void reset() {
            integral = 0.0f;
        }

        void preset(float value) {
            integral = clampUnit(value);
        }

        float getIntegral() const {
            return integral;
        }

    private:
        float integral = 0.0f;
};

struct OvenModel {
    float heatPerSecond;
    uint8_t deadSeconds;
    float lagSeconds;
    float lossPerSecond;
};

/**
 * Steady duty that balances heat loss at the setpoint, so I starts near its final value
 */
inline float holdingDuty(float setpoint, float room, const OvenModel& model) {
    return clampUnit(model.lossPerSecond * (setpoint - room) / model.heatPerSecond);
}

/**
 * Smith predictor: two simulated ovens get the same heater history, one heats instantly and
 * one with the real dead time and lag. Their difference is heat already given that the sensor
 * can't show yet; it is zero in steady state, so adding it to the reading adds no bias
 */
class SmithPredictor {
    public:
        void reset(float temperature, float roomTemperature) {
            history = 0;
            reservoir = 0.0f;
            instant = temperature;
            delayed = temperature;
            room = roomTemperature;
            transitSum = 0.0f;
            transitTicks = 0;
        }

        /**
         * Call once per second with the heater state during that second
         */
        void tick(bool heaterOn, const OvenModel& model) {
            float heat = heaterOn ? model.heatPerSecond : 0.0f;
            instant += heat - model.lossPerSecond * (instant - room);

            bool arrived = heaterOn;
            if (model.deadSeconds > 0) {
                arrived = (history >> (model.deadSeconds - 1)) & 1;
                history = (history << 1) | (heaterOn ? 1 : 0);
            }

            reservoir += arrived ? model.heatPerSecond : 0.0f;
            float flow = reservoir / model.lagSeconds;
            reservoir -= flow;
            delayed += flow - model.lossPerSecond * (delayed - room);
            transitSum += instant - delayed;
            transitTicks++;
        }

        float inTransit() const {
            return instant - delayed;
        }

        /**
         * Mean since the previous call. A single sample swings with the pulse inside each window,
         * the mean over a whole window doesn't, so it is zero at any steady duty
         */
        float takeMeanInTransit() {
            float mean = transitTicks > 0 ? transitSum / transitTicks : inTransit();
            transitSum = 0.0f;
            transitTicks = 0;
            return mean;
        }

    private:
        uint64_t history = 0;
        float reservoir = 0.0f;
        float instant = 0.0f;
        float delayed = 0.0f;
        float room = 0.0f;
        float transitSum = 0.0f;
        uint16_t transitTicks = 0;
};
