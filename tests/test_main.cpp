#include <cstdio>

#include <gtest/gtest.h>

int main(int argc, char** argv) {
    std::fputs("autopilot_tests: own main() invoked\n", stderr);
    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    std::fprintf(stderr, "autopilot_tests: RUN_ALL_TESTS returned %d\n", result);
    std::fprintf(stderr, "autopilot_tests: total registered tests = %d\n",
                 ::testing::UnitTest::GetInstance()->total_test_count());
    return result;
}
