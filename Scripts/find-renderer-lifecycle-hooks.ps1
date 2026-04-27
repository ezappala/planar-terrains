$Hooks = @(
  "SetupViewFamily",
  "SetupView",
  "SetupViewPoint",
  "SetupViewProjectionMatrix",
  "BeginRenderViewFamily",
  "PostCreateSceneRenderer",
  "PreRenderViewFamily_RenderThread",
  "PreRenderView_RenderThread",
  "PreInitViews_RenderThread",
  "PreRenderBasePass_RenderThread",
  "PostRenderBasePassDeferred_RenderThread",
  "PostRenderBasePassMobile_RenderThread",
  "PostTLASBuild_RenderThread",
  "PrePostProcessPass_RenderThread",
  "PrePostProcessPassMobile_RenderThread",
  "SubscribeToPostProcessingPass",
  "PostRenderViewFamily_RenderThread",
  "PostRenderView_RenderThread",
  "GetPriority",
  "IsActiveThisFrame",
  "GetFlags"
)

$Pattern = ($Hooks -join "|")

$CurrentFolder = pwd

rg -n --pcre2 "(?:->|\.)\s*(?:$Pattern)\s*\(" `
  "C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Engine" `
  "C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Renderer" `
  -g "*.h" -g "*.cpp" -g "*.inl" `
  > SceneViewExtension-hook-callSites.txt

rg -n "FSceneViewExtensions|ISceneViewExtension|ViewExtensions|ActiveViewExtension|SceneViewExtension" `
  "C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Engine" `
  "C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Renderer" `
  -g "*.h" -g "*.cpp" -g "*.inl" `
  > SceneViewExtension-dispatchers.txt
