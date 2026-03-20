#pragma once
#include "Logging/StructuredLog.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

namespace ext::logging {
inline FString time_elapsed_to_string(const FTimespan& timespan) {
    const int32 hours = timespan.GetHours();
    const int32 minutes = timespan.GetMinutes();
    const int32 seconds = timespan.GetSeconds();
    const int32 milliseconds = timespan.GetFractionMilli();

    return FString::Printf(TEXT("%02d:%02d:%02d.%03d"), hours, minutes, seconds, milliseconds);
}

inline FString time_elapsed_to_string(const FDateTime& start_time) {
    return time_elapsed_to_string(FDateTime::UtcNow() - start_time);
}

inline void log_time(const FDateTime& start_time, const FString& msg) {
    UE_LOGFMT(
        LogTemp,
        Log,
        "[{t}] {m}",
        time_elapsed_to_string(start_time),
        msg);
}
}
