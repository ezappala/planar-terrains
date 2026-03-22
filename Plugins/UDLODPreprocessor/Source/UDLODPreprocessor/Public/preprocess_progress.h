#pragma once

#include "Internationalization/Text.h"
#include "Templates/UniquePtr.h"

namespace preprocess {
class IPreprocessProgress {
public:
    virtual ~IPreprocessProgress() = default;

    virtual void push_scope(float total_work, const FText& default_msg) = 0;
    virtual void pop_scope() = 0;
    virtual void enter_progress_frame(
        float expected_work_this_frame = 1.0f,
        const FText& text = FText()
    ) = 0;
    virtual void tick_progress() = 0;
    virtual void make_dialog_delayed(float threshold_seconds, bool show_cancel_btn = false) = 0;
    virtual bool should_cancel() const = 0;
};

class FScopedPreprocessProgress {
public:
    FScopedPreprocessProgress(
        IPreprocessProgress* in_progress,
        const float in_total_work,
        const FText& in_default_msg
    ) : Progress(in_progress) {
        if (Progress != nullptr) {
            Progress->push_scope(in_total_work, in_default_msg);
        }
    }

    ~FScopedPreprocessProgress() {
        if (Progress != nullptr) {
            Progress->pop_scope();
        }
    }

    FScopedPreprocessProgress(const FScopedPreprocessProgress&) = delete;
    FScopedPreprocessProgress& operator=(const FScopedPreprocessProgress&) = delete;

private:
    IPreprocessProgress* Progress = nullptr;
};

enum class EPreprocessProgressMode : uint8 {
    None,
    EditorDialog,
    Console,
};

UDLODPREPROCESSOR_API TUniquePtr<IPreprocessProgress> create_progress(
    EPreprocessProgressMode mode
);
} // namespace preprocess
