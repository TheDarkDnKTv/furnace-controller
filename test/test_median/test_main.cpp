#include <unity.h>
#include <median.hh>

void setUp() {
}

void tearDown() {
}

static void sortAndExpect(int16_t* values, const int16_t* expected, uint8_t count) {
    insertionSort(values, count);
    TEST_ASSERT_EQUAL_INT16_ARRAY(expected, values, count);
}

static void testSortEmptyAndSingle() {
    int16_t empty[1] = { 42 };
    insertionSort(empty, 0);
    TEST_ASSERT_EQUAL_INT16(42, empty[0]);

    int16_t single[] = { 7 };
    const int16_t expected[] = { 7 };
    sortAndExpect(single, expected, 1);
}

static void testSortAlreadySorted() {
    int16_t values[] = { 1, 2, 3, 4, 5 };
    const int16_t expected[] = { 1, 2, 3, 4, 5 };
    sortAndExpect(values, expected, 5);
}

static void testSortReversed() {
    int16_t values[] = { 5, 4, 3, 2, 1 };
    const int16_t expected[] = { 1, 2, 3, 4, 5 };
    sortAndExpect(values, expected, 5);
}

static void testSortDuplicatesAndNegatives() {
    int16_t values[] = { 402, -15, 402, 0, -15 };
    const int16_t expected[] = { -15, -15, 0, 402, 402 };
    sortAndExpect(values, expected, 5);
}

static void testSortOnlyTouchesCount() {
    int16_t values[] = { 3, 1, 2, 99, -99 };
    const int16_t expected[] = { 1, 2, 3, 99, -99 };
    sortAndExpect(values, expected, 3);
    TEST_ASSERT_EQUAL_INT16(99, values[3]);
    TEST_ASSERT_EQUAL_INT16(-99, values[4]);
}

static void testMedianSingleSpikeNeverWins() {
    MedianFilter<5> median;
    const int16_t samples[] = { 401, 402, 402, 150, 403, 402, 402, 403, 403, 403 };
    const int16_t expected[] = { 401, 401, 402, 401, 402, 402, 402, 402, 403, 403 };
    for (uint8_t i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT16(expected[i], median.add(samples[i]));
    }
}

static void testMedianTwoBadSamplesInARowNeverWin() {
    MedianFilter<5> median;
    const int16_t samples[] = { 400, 400, 400, 150, 150, 400, 400, 400 };
    for (int16_t sample : samples) {
        TEST_ASSERT_EQUAL_INT16(400, median.add(sample));
    }
}

static void testMedianThreeBadSamplesWin() {
    MedianFilter<5> median;
    for (uint8_t i = 0; i < 5; i++) {
        median.add(400);
    }

    TEST_ASSERT_EQUAL_INT16(400, median.add(150));
    TEST_ASSERT_EQUAL_INT16(400, median.add(150));
    TEST_ASSERT_EQUAL_INT16(150, median.add(150));
}

static void testMedianFollowsRiseWithDelay() {
    MedianFilter<5> median;
    for (uint8_t i = 0; i < 5; i++) {
        median.add(400);
    }

    TEST_ASSERT_EQUAL_INT16(400, median.add(410));
    TEST_ASSERT_EQUAL_INT16(400, median.add(420));
    TEST_ASSERT_EQUAL_INT16(410, median.add(430));
    TEST_ASSERT_EQUAL_INT16(420, median.add(440));
    TEST_ASSERT_EQUAL_INT16(430, median.add(450));
}

static void testMedianFollowsFallWithDelay() {
    MedianFilter<5> median;
    for (uint8_t i = 0; i < 5; i++) {
        median.add(450);
    }

    TEST_ASSERT_EQUAL_INT16(450, median.add(440));
    TEST_ASSERT_EQUAL_INT16(450, median.add(430));
    TEST_ASSERT_EQUAL_INT16(440, median.add(420));
    TEST_ASSERT_EQUAL_INT16(430, median.add(410));
    TEST_ASSERT_EQUAL_INT16(420, median.add(400));
}

static void testMedianBeforeWindowIsFull() {
    MedianFilter<5> median;
    TEST_ASSERT_EQUAL_INT16(401, median.add(401));
    TEST_ASSERT_EQUAL_INT16(401, median.add(402));
    TEST_ASSERT_EQUAL_INT16(402, median.add(402));
    TEST_ASSERT_EQUAL_INT16(401, median.add(150));
}

static void testMedianResetForgetsHistory() {
    MedianFilter<5> median;
    for (uint8_t i = 0; i < 7; i++) {
        median.add(900);
    }

    median.reset();
    TEST_ASSERT_EQUAL_INT16(250, median.add(250));
    TEST_ASSERT_EQUAL_INT16(250, median.add(260));
    TEST_ASSERT_EQUAL_INT16(255, median.add(255));
}

static void testMedianOldValuesLeaveWindow() {
    MedianFilter<3> median;
    median.add(100);
    median.add(100);
    median.add(100);
    median.add(500);
    TEST_ASSERT_EQUAL_INT16(500, median.add(500));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(testSortEmptyAndSingle);
    RUN_TEST(testSortAlreadySorted);
    RUN_TEST(testSortReversed);
    RUN_TEST(testSortDuplicatesAndNegatives);
    RUN_TEST(testSortOnlyTouchesCount);
    RUN_TEST(testMedianSingleSpikeNeverWins);
    RUN_TEST(testMedianTwoBadSamplesInARowNeverWin);
    RUN_TEST(testMedianThreeBadSamplesWin);
    RUN_TEST(testMedianFollowsRiseWithDelay);
    RUN_TEST(testMedianFollowsFallWithDelay);
    RUN_TEST(testMedianBeforeWindowIsFull);
    RUN_TEST(testMedianResetForgetsHistory);
    RUN_TEST(testMedianOldValuesLeaveWindow);
    return UNITY_END();
}
