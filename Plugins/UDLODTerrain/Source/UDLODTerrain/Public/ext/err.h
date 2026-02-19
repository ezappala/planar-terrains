#pragma once
#include <expected>
#include <Logging/StructuredLog.h>

#include "UDLODTerrain.h"

namespace ext::err {
enum class EUDLODTerrainError : uint8 {
    None,
    RenderTargetNotFound
};

inline const char* msg(const EUDLODTerrainError err) {
    switch (err) {
    case EUDLODTerrainError::None: return "No error";
    case EUDLODTerrainError::RenderTargetNotFound: return "Render target not found";
    default: return "Unknown error";
    }
}

inline const char* msg(const std::expected<void, EUDLODTerrainError>& result) {
    if (result) {
        return "Success";
    }
    return msg(result.error());
}

constexpr void log(const EUDLODTerrainError err) {
    UE_LOGFMT(LogUDLODTerrain, Error, "{}", *FString(msg(err)));
}

inline void log(const EUDLODTerrainError err, const ELogVerbosity::Type verbosity) {
    do {
        if (
            (verbosity & ELogVerbosity::VerbosityMask) == ELogVerbosity::Fatal ||
            ((verbosity & ELogVerbosity::VerbosityMask) <=
                ELogVerbosity::VeryVerbose &&
                (verbosity & ELogVerbosity::VerbosityMask) <=
                LogUDLODTerrain.CompileTimeVerbosity)
        ) {
            static UE::Logging::Private::FStaticLogDynamicData LOG_Dynamic;
            static UE::Logging::Private::FStaticLogRecord LOG_Static{
                L"{}", __builtin_FILE(), __builtin_LINE(), verbosity, LOG_Dynamic
            };
            if ((verbosity & ELogVerbosity::VerbosityMask) == ELogVerbosity::Fatal) {
                {
                    FatalLogWithFields(LogUDLODTerrain, LOG_Static, *FString(msg(err)));
                }
            }
            if (!LogUDLODTerrain.IsSuppressed(verbosity)) {
                {
                    LogWithFields(LogUDLODTerrain, LOG_Static, *FString(msg(err)));
                }
            }
        }
    }
    while (false);
}
} // namespace ext::err