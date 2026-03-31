#pragma once

#include "CoreMinimal.h"
#include "STerrainDebugTable.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

class FDebugViewerModule final : public IModuleInterface {
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    TWeakPtr<SWindow> window;
    TSharedPtr<STerrainDebugTable> table_widget;

    void handle_window_requested(ATerrainParentActor* actor) {
        UE_LOGFMT(
            LogTemp,
            Log,
            "[FDebugViewerModule::HandleWindowRequested] Received request to open terrain debug window for actor: {n}",
            actor != nullptr ? actor->GetName() : TEXT("null")
        );
        if (actor == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "[FDebugViewerModule::HandleWindowRequested] Received request to open terrain debug window with null actor"
            );
            return;
        }

        if (!window.IsValid()) {
            SAssignNew(table_widget, STerrainDebugTable);
            table_widget->Refresh(actor);

            const TSharedRef<SWindow> new_window = SNew(SWindow)
                .Title(FText::FromString(TEXT("Terrain Debug Table")))
                .ClientSize(FVector2D(1200.0f, 800.0f))
                .SupportsMaximize(true)
                .SupportsMinimize(true)
                [
                    table_widget.ToSharedRef()
                ];

            window = new_window;

            new_window->SetOnWindowClosed(
                FOnWindowClosed::CreateRaw(this, &FDebugViewerModule::handle_window_closed)
            );

            FSlateApplication::Get().AddWindow(new_window);
            return;
        }

        if (table_widget.IsValid()) { table_widget->Refresh(actor); }

        const TSharedPtr<SWindow> existing_window = window.Pin();
        if (existing_window.IsValid()) {
            existing_window->BringToFront(true);
            existing_window->FlashWindow();
        }
    }

    void handle_window_closed(const TSharedRef<SWindow>& closed_window) {
        table_widget.Reset();
        window.Reset();
    }
};
