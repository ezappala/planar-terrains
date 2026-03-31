#include "DebugViewer.h"

#include "terrain_debug_bridge.h"

#define LOCTEXT_NAMESPACE "FDebugViewerModule"

void FDebugViewerModule::StartupModule() {
    UE_LOGFMT(LogTemp, Log, "Starting up debug viewer module");
    TERRAIN_DEBUG_WINDOW_REQUESTED.AddRaw(this, &FDebugViewerModule::handle_window_requested);
    if (TERRAIN_DEBUG_WINDOW_REQUESTED.IsBound()) {
        UE_LOGFMT(LogTemp, Log, "Successfully bound to TERRAIN_DEBUG_WINDOW_REQUESTED delegate");
    } else {
        UE_LOGFMT(LogTemp, Error, "Failed to bind to TERRAIN_DEBUG_WINDOW_REQUESTED delegate");
    }
}

void FDebugViewerModule::ShutdownModule() {
    TERRAIN_DEBUG_WINDOW_REQUESTED.RemoveAll(this);

    table_widget.Reset();
    window.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDebugViewerModule, DebugViewer)
