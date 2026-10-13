#include <unity.h>
#include <math.h>
#include <stdio.h>
#include <pid.hh>
#include "../../src/settings.hh"

static const PidProfile LINEAR = { 60000, 0.05f, 600.0f, 6.0f };
static const PidProfile PROFILE = { CTRL_WINDOW_MS, CTRL_KP, CTRL_TI_S, CTRL_I_BAND };

void setUp() {
}

void tearDown() {
}

static void testProportionalOutsideIntegralBand() {
    PidController controller;
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, controller.update(10.0f, LINEAR));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, controller.update(-10.0f, LINEAR));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, controller.getIntegral());
}

static void testPulseSkipsShortAndFillsNearlyFull() {
    TEST_ASSERT_EQUAL_UINT32(0, pulseMs(0.01f, 90000, 1000));
    TEST_ASSERT_EQUAL_UINT32(45000, pulseMs(0.5f, 90000, 1000));
    TEST_ASSERT_EQUAL_UINT32(90000, pulseMs(0.995f, 90000, 1000));
    TEST_ASSERT_EQUAL_UINT32(0, pulseMs(-0.5f, 90000, 1000));
    TEST_ASSERT_EQUAL_UINT32(90000, pulseMs(1.5f, 90000, 1000));
}

static void testIntegralLearnsInsideBandOnly() {
    PidController controller;
    controller.update(1.0f, LINEAR);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.05f / 600.0f * 60.0f, controller.getIntegral());

    controller.reset();
    controller.update(10.0f, LINEAR);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, controller.getIntegral());
}

static void testIntegralSettlesWhereErrorIsZero() {
    PidController controller;
    for (uint8_t i = 0; i < 10; i++) {
        controller.update(1.0f, LINEAR);
    }

    float learned = controller.getIntegral();
    float duty = controller.update(0.0f, LINEAR);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, learned, controller.getIntegral());
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, learned, duty);
}

static void testIntegralFreezesWhileSaturated() {
    PidController controller;
    for (uint16_t i = 0; i < 2000; i++) {
        controller.update(5.0f, LINEAR);
    }

    TEST_ASSERT_TRUE(controller.getIntegral() <= 0.75f + 1e-6f);
    float duty = controller.update(5.0f, LINEAR);
    TEST_ASSERT_TRUE(duty >= 0.97f && duty <= 1.0f);
}

static void testIntegralNeverNegative() {
    PidController controller;
    for (uint8_t i = 0; i < 50; i++) {
        controller.update(-3.0f, LINEAR);
    }

    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, controller.getIntegral());
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, controller.update(-3.0f, LINEAR));
}

static const OvenModel MODEL = { OVEN_HEAT_PER_S, OVEN_DEAD_S, OVEN_LAG_S, OVEN_LOSS_PER_S };

static void testPredictorIsZeroAtRest() {
    SmithPredictor predictor;
    predictor.reset(40.0f, 22.0f);
    for (uint16_t i = 0; i < 600; i++) {
        predictor.tick(false, MODEL);
    }

    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, predictor.inTransit());
}

static void testPredictorCountsHeatNotYetArrived() {
    SmithPredictor predictor;
    predictor.reset(25.0f, 22.0f);
    for (uint8_t i = 0; i < 10; i++) {
        predictor.tick(true, MODEL);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 10 * OVEN_HEAT_PER_S, predictor.inTransit());
    for (uint16_t i = 0; i < 3600; i++) {
        predictor.tick(false, MODEL);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, predictor.inTransit());
}

static void testPredictorHasNoBiasAtSteadyDuty() {
    SmithPredictor predictor;
    predictor.reset(22.0f, 22.0f);
    float sum = 0.0f;
    for (uint32_t second = 0; second < 4UL * 3600UL; second++) {
        predictor.tick(second % 60 < 6, MODEL);
        if (second >= 3UL * 3600UL) {
            sum += predictor.inTransit();
        }
    }

    TEST_ASSERT_FLOAT_WITHIN(0.3f, 0.0f, sum / 3600.0f);
}

static void testMeanInTransitIsZeroAtSteadyDutyWhileSampleIsNot() {
    SmithPredictor predictor;
    predictor.reset(22.0f, 22.0f);
    float sample = 0.0f;
    float mean = 0.0f;
    for (uint32_t second = 0; second < 4UL * 3600UL; second++) {
        if (second % 60 == 0) {
            sample = predictor.inTransit();
            mean = predictor.takeMeanInTransit();
        }

        predictor.tick(second % 60 < 6, MODEL);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, mean);
    TEST_ASSERT_TRUE(sample < -0.5f);
}

static void testHoldingDutyFromModel() {
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, OVEN_LOSS_PER_S * 18.0f / OVEN_HEAT_PER_S, holdingDuty(40.0f, 22.0f, MODEL));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, holdingDuty(20.0f, 22.0f, MODEL));
}

struct Plant {
    float heat;
    uint8_t dead;
    float lag;
    float loss;
};

struct Segment {
    float overshoot;
    float minutesToReach;
    float tailMean;
    float tailMaxDeviation;
    float earlyMean;
    float earlyMaxDeviation;
};

/**
 * Mirrors the firmware's control path on a simulated oven: a predictor tick per second,
 * a PI decision per window on reading + heat in transit (P on the latest value, I on the
 * window mean), I preset from the model at the start and rescaled when the setpoint changes
 */
static void simulateSession(const Plant& plant, float firstSetpoint, float secondSetpoint, uint32_t changeAt,
                            uint32_t totalSeconds, Segment* first, Segment* second) {
    const float room = 22.0f;
    const uint8_t maxDead = 64;
    static float pipeline[maxDead];
    for (uint8_t i = 0; i < maxDead; i++) {
        pipeline[i] = 0.0f;
    }

    float temperature = room;
    float reservoir = 0.0f;
    float setpoint = firstSetpoint;
    float roomEstimate = temperature < OVEN_ROOM_MAX ? temperature : OVEN_ROOM_MAX;
    SmithPredictor predictor;
    PidController controller;
    predictor.reset(temperature, roomEstimate);
    controller.preset(holdingDuty(setpoint, roomEstimate, MODEL));

    float duty = 0.0f;
    uint32_t windowStart = 0;
    bool windowDue = true;
    bool lastHeating = false;
    Segment* segment = first;
    uint32_t segmentStart = 0;
    float tailSum = 0.0f;
    uint32_t tailCount = 0;
    float earlySum = 0.0f;
    uint32_t earlyCount = 0;
    *first = { -100.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    if (second != nullptr) {
        *second = { -100.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    }

    for (uint32_t t = 0; t < totalSeconds; t++) {
        if (second != nullptr && t == changeAt) {
            segment->tailMean = tailCount > 0 ? tailSum / tailCount : 0.0f;
            segment->earlyMean = earlyCount > 0 ? earlySum / earlyCount : 0.0f;
            earlySum = 0.0f;
            earlyCount = 0;
            float old = setpoint;
            setpoint = secondSetpoint;
            if (old - roomEstimate > 2.0f) {
                controller.preset(controller.getIntegral() * (setpoint - roomEstimate) / (old - roomEstimate));
            } else {
                controller.preset(holdingDuty(setpoint, roomEstimate, MODEL));
            }

            windowDue = true;
            segment = second;
            segmentStart = t;
            tailSum = 0.0f;
            tailCount = 0;
        }

        predictor.tick(lastHeating, MODEL);
        if (windowDue || (t - windowStart) * 1000UL >= PROFILE.windowMs) {
            windowDue = false;
            windowStart = t;
            float mean = temperature + predictor.takeMeanInTransit();
            duty = controller.update(setpoint - (temperature + predictor.inTransit()), setpoint - mean, PROFILE);
        }

        bool heating = (t - windowStart) * 1000UL < pulseMs(duty, PROFILE.windowMs, CTRL_MIN_PULSE_MS);
        lastHeating = heating;

        uint8_t slot = t % plant.dead;
        float arrived = pipeline[slot];
        pipeline[slot] = heating ? plant.heat : 0.0f;
        reservoir += arrived;
        float flow = reservoir / plant.lag;
        reservoir -= flow;
        temperature += flow - plant.loss * (temperature - room);

        uint32_t segmentEnd = (segment == first && second != nullptr) ? changeAt : totalSeconds;
        if (segment->minutesToReach < 0.0f && fabsf(temperature - setpoint) <= 1.0f) {
            segment->minutesToReach = (t - segmentStart) / 60.0f;
        }

        if (segment->minutesToReach >= 0.0f && temperature - setpoint > segment->overshoot) {
            segment->overshoot = temperature - setpoint;
        }

        if (segment->minutesToReach >= 0.0f && t - segmentStart < segment->minutesToReach * 60.0f + 1800.0f) {
            float deviation = fabsf(temperature - setpoint);
            earlySum += temperature - setpoint;
            earlyCount++;
            if (deviation > segment->earlyMaxDeviation) {
                segment->earlyMaxDeviation = deviation;
            }
        }

        if (t + 1800 >= segmentEnd) {
            float deviation = fabsf(temperature - setpoint);
            tailSum += temperature - setpoint;
            tailCount++;
            if (deviation > segment->tailMaxDeviation) {
                segment->tailMaxDeviation = deviation;
            }
        }
    }

    segment->tailMean = tailCount > 0 ? tailSum / tailCount : 0.0f;
    segment->earlyMean = earlyCount > 0 ? earlySum / earlyCount : 0.0f;
}

static const float HEATS[] = { 0.32f, 0.36f, 0.40f };
static const uint8_t DEADS[] = { 30, 45, 60 };
static const float LAGS[] = { 50.0f, 60.0f, 70.0f };
static const float LOSSES[] = { 0.0009f, 0.001f, 0.0011f };

static void track(Segment* worst, const Segment& segment) {
    float reach = segment.minutesToReach < 0.0f ? 999.0f : segment.minutesToReach;
    if (segment.overshoot > worst->overshoot) {
        worst->overshoot = segment.overshoot;
    }

    if (reach > worst->minutesToReach) {
        worst->minutesToReach = reach;
    }

    if (fabsf(segment.tailMean) > worst->tailMean) {
        worst->tailMean = fabsf(segment.tailMean);
    }

    if (segment.tailMaxDeviation > worst->tailMaxDeviation) {
        worst->tailMaxDeviation = segment.tailMaxDeviation;
    }

    if (fabsf(segment.earlyMean) > fabsf(worst->earlyMean)) {
        worst->earlyMean = segment.earlyMean;
    }

    if (segment.earlyMaxDeviation > worst->earlyMaxDeviation) {
        worst->earlyMaxDeviation = segment.earlyMaxDeviation;
    }
}

/**
 * Worst values over a realistic spread around the fitted oven: heater +-11 %, dead time
 * 30-60 s (measured hot vs cold), lag +-10 s, heat loss +-10 %
 */
static Segment worstOverPlants(float firstSetpoint, float secondSetpoint, bool useSecond) {
    Segment worst = { -100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    for (float heat : HEATS) {
        for (uint8_t dead : DEADS) {
            for (float lag : LAGS) {
                for (float loss : LOSSES) {
                    Plant plant = { heat, dead, lag, loss };
                    Segment first;
                    Segment second;
                    if (useSecond) {
                        simulateSession(plant, firstSetpoint, secondSetpoint, 7200, 14400, &first, &second);
                        track(&worst, second);
                    } else {
                        simulateSession(plant, firstSetpoint, 0.0f, 0, 7200, &first, nullptr);
                        track(&worst, first);
                    }
                }
            }
        }
    }

    return worst;
}

/**
 * Limits keep a margin over the simulated worst case; overshoot stays well below E2 (+10 C)
 */
static void checkWorst(const char* label, const Segment& worst, float maxOvershoot, float maxMinutes) {
    char message[160];
    snprintf(message, sizeof(message), "%s: overshoot %+.2f reach %.1f min tail mean %.2f tail max %.2f first 30 min mean %+.2f max %.2f",
             label, worst.overshoot, worst.minutesToReach, worst.tailMean, worst.tailMaxDeviation, worst.earlyMean,
             worst.earlyMaxDeviation);
    TEST_MESSAGE(message);
    TEST_ASSERT_TRUE_MESSAGE(worst.overshoot <= maxOvershoot, message);
    TEST_ASSERT_TRUE_MESSAGE(worst.minutesToReach <= maxMinutes, message);
    TEST_ASSERT_TRUE_MESSAGE(worst.tailMean <= 0.5f, message);
    TEST_ASSERT_TRUE_MESSAGE(worst.tailMaxDeviation <= 0.75f, message);
}

static void testControllerOnRealisticOvenRange() {
    const uint16_t setpoints[] = { 30, 40, 50, 60, 80, 100 };
    char label[32];
    for (uint16_t setpoint : setpoints) {
        snprintf(label, sizeof(label), "cold start to %u C", setpoint);
        checkWorst(label, worstOverPlants(setpoint, 0.0f, false), setpoint <= 50 ? 2.5f : 5.5f, setpoint <= 60 ? 15.0f : 20.0f);
    }

    checkWorst("raise 40 -> 60 C", worstOverPlants(40.0f, 60.0f, true), 2.5f, 15.0f);
    checkWorst("lower 60 -> 40 C", worstOverPlants(60.0f, 40.0f, true), 1.5f, 20.0f);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(testProportionalOutsideIntegralBand);
    RUN_TEST(testPulseSkipsShortAndFillsNearlyFull);
    RUN_TEST(testIntegralLearnsInsideBandOnly);
    RUN_TEST(testIntegralSettlesWhereErrorIsZero);
    RUN_TEST(testIntegralFreezesWhileSaturated);
    RUN_TEST(testIntegralNeverNegative);
    RUN_TEST(testPredictorIsZeroAtRest);
    RUN_TEST(testPredictorCountsHeatNotYetArrived);
    RUN_TEST(testPredictorHasNoBiasAtSteadyDuty);
    RUN_TEST(testMeanInTransitIsZeroAtSteadyDutyWhileSampleIsNot);
    RUN_TEST(testHoldingDutyFromModel);
    RUN_TEST(testControllerOnRealisticOvenRange);
    return UNITY_END();
}
