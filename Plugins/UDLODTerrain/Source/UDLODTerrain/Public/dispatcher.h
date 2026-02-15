#pragma once

#include "config.h"
#include "RendererInterface.h"
#include "Delegates/IDelegateInstance.h"
#include "Engine/Texture2D.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostOpaqueRender, FPostOpaqueRenderParameters&)

class FCSDispatcher {
public:
    FCSDispatcher();

    TObjectPtr<UTexture2D> heightmap;
    FRDGTexture* heightmap_rdg;

    void begin_pass();
    void end_pass() const;
#if defined(BASIC_MODE) && BASIC_MODE
    void add_basic_draw_pass();
#endif

private:
    FDelegateHandle on_post_opaque_render_delegate;

    void add_compute_pass(FPostOpaqueRenderParameters& PostOpaqueRenderParameters);

    void add_udlod_passes(
        FRDGBuilder& gb, const FViewInfo& view, FRDGTexture* scene_color, FRDGTexture* scene_depth
    ) const;
};

