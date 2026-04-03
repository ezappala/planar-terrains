#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "ext_logging.h"

using namespace udlodext::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtLoggingFormattingTest,
    "UDLODExt.Logging.Formatting",
    TestFlags)

bool FUDLODExtLoggingFormattingTest::RunTest(const FString& Parameters) {
    const FString formatted = ext::logging::time_elapsed_to_string(
        FTimespan::FromMilliseconds(3723004));
    TestEqual(TEXT("Timespan formatting uses hh:mm:ss.mmm"), formatted, FString(TEXT("01:02:03.004")));

    const FString from_start = ext::logging::time_elapsed_to_string(
        FDateTime::UtcNow() - FTimespan::FromMilliseconds(1200));
    TestTrue(
        TEXT("Start-time formatting preserves clock shape"),
        from_start.MatchesWildcard(TEXT("??:??:??.???")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtLoggingLogTimeTest,
    "UDLODExt.Logging.LogTimeWritesMessage",
    TestFlags)

bool FUDLODExtLoggingLogTimeTest::RunTest(const FString& Parameters) {
    AddExpectedMessagePlain(
        TEXT("timed log message"),
        ELogVerbosity::Log,
        EAutomationExpectedMessageFlags::Contains,
        1);

    ext::logging::log_time(FDateTime::UtcNow(), TEXT("timed log message"));
    return true;
}

#endif
