#pragma once

#include "CoreMinimal.h"
#include "STerrainDebugTable.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

class FDebugViewerModule final : public IModuleInterface {
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    TWeakPtr<SWindow> Window;
    TSharedPtr<STerrainDebugTable> TableWidget;

    void HandleWindowRequested(ATerrainParentActor* Actor) {
        if (!Actor) { return; }

        if (!Window.IsValid()) {
            SAssignNew(TableWidget, STerrainDebugTable);
            TableWidget->RefreshFromActor(Actor);

            const TSharedRef<SWindow> NewWindow = SNew(SWindow)
                .Title(FText::FromString(TEXT("Terrain Debug Table")))
                .ClientSize(FVector2D(1200.0f, 800.0f))
                .SupportsMaximize(true)
                .SupportsMinimize(true)
                [
                    TableWidget.ToSharedRef()
                ];

            Window = NewWindow;

            NewWindow->SetOnWindowClosed(
                FOnWindowClosed::CreateRaw(this, &FDebugViewerModule::HandleWindowClosed)
            );

            FSlateApplication::Get().AddWindow(NewWindow);
            return;
        }

        if (TableWidget.IsValid()) { TableWidget->RefreshFromActor(Actor); }

        const TSharedPtr<SWindow> ExistingWindow = Window.Pin();
        if (ExistingWindow.IsValid()) {
            ExistingWindow->BringToFront(true);
            ExistingWindow->FlashWindow();
        }
    }

    void HandleWindowClosed([[maybe_unused]] const TSharedRef<SWindow>& ClosedWindow) {
        TableWidget.Reset();
        Window.Reset();
    }
};
