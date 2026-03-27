#include "DebugViewer.h"

#include "terrain_debug_bridge.h"

#define LOCTEXT_NAMESPACE "FDebugViewerModule"

void FDebugViewerModule::StartupModule() {
    GTerrainDebugWindowRequested.AddRaw(this, &FDebugViewerModule::HandleWindowRequested);
}

void FDebugViewerModule::ShutdownModule() {
    GTerrainDebugWindowRequested.RemoveAll(this);

    TableWidget.Reset();
    Window.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDebugViewerModule, DebugViewer)
