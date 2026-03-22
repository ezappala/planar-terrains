#include "preprocess_progress.h"

#include "Async/Async.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformProcess.h"
#include "Logging/StructuredLog.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ScopeLock.h"
#include "Templates/Atomic.h"

namespace preprocess {
namespace {
class FNullPreprocessProgress final : public IPreprocessProgress {
public:
    virtual void push_scope(float total_work, const FText& default_msg) override {}
    virtual void pop_scope() override {}
    virtual void enter_progress_frame(float expected_work, const FText& text) override {}
    virtual void tick_progress() override {}
    virtual void make_dialog_delayed(float threshold_seconds, bool show_cancel_button) override {}
    virtual bool should_cancel() const override { return false; }
};

class FSlowTaskPreprocessProgress final : public IPreprocessProgress {
public:
    virtual void push_scope(float total_work, const FText& default_msg) override {
        scopes.Emplace(MakeUnique<FScopedSlowTask>(total_work, default_msg));
    }

    virtual void pop_scope() override {
        if (!scopes.IsEmpty()) {
            scopes.Pop();
        }
    }

    virtual void enter_progress_frame(const float expected_work, const FText& text) override {
        if (!scopes.IsEmpty()) {
            scopes.Last()->EnterProgressFrame(expected_work, text);
        }
    }

    virtual void tick_progress() override {
        if (!scopes.IsEmpty()) {
            scopes.Last()->TickProgress();
        }
    }

    virtual void make_dialog_delayed(const float threshold_seconds, const bool show_cancel_button) override {
        if (!scopes.IsEmpty()) {
            scopes[0]->MakeDialogDelayed(threshold_seconds, show_cancel_button);
        }
    }

    virtual bool should_cancel() const override {
        return !scopes.IsEmpty() && scopes.Last()->ShouldCancel();
    }

private:
    TArray<TUniquePtr<FScopedSlowTask>> scopes;
};

class FConsolePreprocessProgress final : public IPreprocessProgress {
public:
    FConsolePreprocessProgress() {
        printer = Async(EAsyncExecution::Thread, [this] { run_printer(); });
    }

    virtual ~FConsolePreprocessProgress() override {
        stop_requested.Store(true);
        if (printer.IsValid()) {
            printer.Wait();
        }
        print_snapshot(true);
    }

    virtual void push_scope(const float total_work, const FText& default_msg) override {
        FScopeLock _(&mutex);

        FScopeState scope;
        scope.total_work = total_work;
        scope.default_msg = default_msg;
        scope.parent_index = scope_stack.IsEmpty() ? INDEX_NONE : scope_stack.Last();

        const int32 new_scope_index = scopes.Add(MoveTemp(scope));
        if (scope.parent_index != INDEX_NONE) {
            scopes[scope.parent_index].active_child_index = new_scope_index;
        }

        scope_stack.Add(new_scope_index);
    }

    virtual void pop_scope() override {
        FScopeLock _(&mutex);
        if (scope_stack.IsEmpty()) {
            return;
        }

        const int32 ScopeIndex = scope_stack.Pop(EAllowShrinking::No);
        FScopeState& scope = scopes[ScopeIndex];
        scope.completed_work = scope.total_work;
        scope.active_frame_work = 0.0f;
        scope.active_child_index = INDEX_NONE;

        if (scope.parent_index != INDEX_NONE) {
            FScopeState& parent = scopes[scope.parent_index];
            parent.completed_work = FMath::Min(
                parent.total_work,
                parent.completed_work + parent.active_frame_work);
            parent.active_frame_work = 0.0f;
            parent.frame_msg = FText();
            parent.active_child_index = INDEX_NONE;
        }
    }

    virtual void enter_progress_frame(const float expected_work, const FText& text) override {
        FScopeLock _(&mutex);
        if (scope_stack.IsEmpty()) {
            return;
        }

        FScopeState& scope = scopes[scope_stack.Last()];
        if (scope.active_child_index == INDEX_NONE && scope.active_frame_work > 0.0f) {
            scope.completed_work = FMath::Min(
                scope.total_work,
                scope.completed_work + scope.active_frame_work);
        }

        scope.active_frame_work = expected_work;
        scope.frame_msg = text;
    }

    virtual void tick_progress() override {}

    virtual void make_dialog_delayed(float threshold_seconds, bool show_cancel_button) override {}

    virtual bool should_cancel() const override { return false; }

private:
    struct FScopeState {
        float total_work = 0.0f;
        float completed_work = 0.0f;
        float active_frame_work = 0.0f;
        FText default_msg;
        FText frame_msg;
        int32 parent_index = INDEX_NONE;
        int32 active_child_index = INDEX_NONE;
    };

    struct FSnapshot {
        double fraction = 0.0;
        FString message;
    };

    void run_printer() {
        while (!stop_requested.Load()) {
            print_snapshot(false);
            FPlatformProcess::Sleep(0.25f);
        }
    }

    FSnapshot snapshot() const {
        FScopeLock _(&mutex);
        if (scopes.IsEmpty()) {
            return {1.0, TEXT("Preprocessing complete")};
        }

        return {compute_fraction(0), compute_message(0)};
    }

    double compute_fraction(const int32 scope_index) const {
        const FScopeState& scope = scopes[scope_index];
        if (scope.total_work <= 0.0f) {
            return 1.0;
        }

        double current_work = scope.completed_work;
        if (scope.active_child_index != INDEX_NONE) {
            current_work += scope.active_frame_work * compute_fraction(scope.active_child_index);
        }

        return FMath::Clamp(current_work / scope.total_work, 0.0, 1.0);
    }

    FString compute_message(const int32 scope_index) const {
        const FScopeState& scope = scopes[scope_index];
        if (scope.active_child_index != INDEX_NONE) {
            return compute_message(scope.active_child_index);
        }

        const FText message = scope.frame_msg.IsEmpty()
            ? scope.default_msg
            : scope.frame_msg;
        return message.IsEmpty() ? TEXT("Preprocessing terrain") : message.ToString();
    }

    void print_snapshot(const bool force) {
        const auto [fraction, message] = snapshot();
        const int32 percent = FMath::RoundToInt(fraction * 100.0);
        const FString rendered = FString::Printf(TEXT("[%3d%%] %s"), percent, *message);

        FScopeLock _(&print_mutex);
        if (!force && rendered == last_rendered) {
            return;
        }

        last_rendered = rendered;
        UE_LOGFMT(LogTemp, Display, "{line}", rendered);
    }

    mutable FCriticalSection mutex;
    FCriticalSection print_mutex;
    TArray<FScopeState> scopes;
    TArray<int32> scope_stack;
    TFuture<void> printer;
    TAtomic<bool> stop_requested{false};
    FString last_rendered;
};
} // namespace

TUniquePtr<IPreprocessProgress> create_progress(const EPreprocessProgressMode mode) {
    switch (mode) {
    case EPreprocessProgressMode::EditorDialog: return MakeUnique<FSlowTaskPreprocessProgress>();
    case EPreprocessProgressMode::Console: return MakeUnique<FConsolePreprocessProgress>();
    case EPreprocessProgressMode::None:
    default: return MakeUnique<FNullPreprocessProgress>();
    }
}
} // namespace preprocess
