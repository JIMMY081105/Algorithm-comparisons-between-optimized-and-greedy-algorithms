#ifndef BACKEND_TEST_HARNESS_H
#define BACKEND_TEST_HARNESS_H

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct TestCase {
    std::string name;
    std::function<void()> run;
};

inline std::vector<TestCase>& testRegistry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct TestRegistrar {
    TestRegistrar(const std::string& name, std::function<void()> run) {
        testRegistry().push_back({name, std::move(run)});
    }
};

#define TEST_CASE(name)                                                       \
    void name();                                                              \
    static TestRegistrar registrar_##name(#name, name);                       \
    void name()

#define REQUIRE_TRUE(condition)                                               \
    do {                                                                      \
        if (!(condition)) {                                                    \
            std::ostringstream oss;                                            \
            oss << "Requirement failed: " #condition;                         \
            throw std::runtime_error(oss.str());                               \
        }                                                                     \
    } while (false)

#define REQUIRE_FALSE(condition) REQUIRE_TRUE(!(condition))

#define REQUIRE_EQ(actual, expected)                                          \
    do {                                                                      \
        const auto actualValue = (actual);                                     \
        const auto expectedValue = (expected);                                 \
        if (!(actualValue == expectedValue)) {                                 \
            std::ostringstream oss;                                            \
            oss << "Expected " #actual " == " #expected                       \
                << ", got " << actualValue << " vs " << expectedValue;       \
            throw std::runtime_error(oss.str());                               \
        }                                                                     \
    } while (false)

#define REQUIRE_NE(actual, expected)                                          \
    do {                                                                      \
        const auto actualValue = (actual);                                     \
        const auto expectedValue = (expected);                                 \
        if (actualValue == expectedValue) {                                    \
            std::ostringstream oss;                                            \
            oss << "Expected " #actual " != " #expected;                     \
            throw std::runtime_error(oss.str());                               \
        }                                                                     \
    } while (false)

inline void requireNear(float actual, float expected, float epsilon,
                        const char* actualExpr, const char* expectedExpr) {
    if (std::fabs(actual - expected) > epsilon) {
        std::ostringstream oss;
        oss << "Expected " << actualExpr << " near " << expectedExpr
            << ", got " << actual << " vs " << expected;
        throw std::runtime_error(oss.str());
    }
}

#define REQUIRE_NEAR(actual, expected, epsilon)                               \
    requireNear(static_cast<float>(actual), static_cast<float>(expected),      \
                static_cast<float>(epsilon), #actual, #expected)

#define REQUIRE_THROWS_AS(expression, exceptionType)                          \
    do {                                                                      \
        bool threwExpected = false;                                            \
        try {                                                                 \
            (void)(expression);                                                \
        } catch (const exceptionType&) {                                       \
            threwExpected = true;                                              \
        } catch (...) {                                                        \
        }                                                                     \
        if (!threwExpected) {                                                  \
            throw std::runtime_error(                                          \
                "Expected exception: " #exceptionType);                       \
        }                                                                     \
    } while (false)

inline int runAllTests() {
    int failed = 0;
    for (const TestCase& test : testRegistry()) {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": " << e.what() << "\n";
        } catch (...) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
        }
    }

    std::cout << testRegistry().size() << " tests run, " << failed
              << " failed\n";
    return failed == 0 ? 0 : 1;
}

#endif // BACKEND_TEST_HARNESS_H
