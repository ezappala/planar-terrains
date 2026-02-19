#include "component.h"

#include "render_pass.h"

TSubclassOf<U_EGP_RenderPass> UTerrainComponent::GetPassType() const { return UTerrainRenderPass::StaticClass(); }
