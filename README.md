# FidelityFXCAS
AMD Fidelity FX Contrast Adaptive Sharpening plugin for Unreal Engine

## Version and support
The current plugin version has been developed and tested on **UE v4.24** on **Windows** platform on a C++ project.

# Description
This plugin provides an implementation of Fidelity FX Contrast Adaptive Sharpening effect into the Unreal Engine Pipeline.
> FidelityFX is a series of optimized shader-based features aimed at improving rendering quality and performance.

## Contrast Adaptive Sharpening (CAS)
> Contrast Adaptive Sharpening (CAS) is the first FidelityFX release. CAS provides a mixed ability to sharpen and optionally scale an image. The algorithm adjusts the amount of sharpening per pixel to target an even level of sharpness across the image. Areas of the input image that are already sharp are sharpened less, while areas that lack detail are sharpened more. This allows for higher overall natural visual sharpness with fewer artifacts.
> CAS was designed to help increase the quality of existing Temporal Anti-Aliasing (TAA) solutions. TAA often introduces a variable amount of blur due to temporal feedback. The adaptive sharpening provided by CAS is ideal to restore detail in images produced after TAA .
> CAS’ optional scaling capability is designed to support Dynamic Resolution Scaling (DRS). DRS changes render resolution every frame, which requires scaling prior to compositing the fixed-resolution User Interface (UI). CAS supports both up-sampling and down-sampling in the same single pass that applies sharpening.

Source: [https://gpuopen.com/gaming-product/fidelityfx/](https://gpuopen.com/gaming-product/fidelityfx/)

## Example screenshots
(open in full resolution for the differences to be fully visible)

Screen space CAS OFF | Screen space CAS ON
---------------------|--------------------
![CAS OFF](https://github.com/amedrycki/FidelityFXCAS/blob/master/Docs/Screenshots/CAS_OFF.png?raw=true) | ![CAS_ON](https://github.com/amedrycki/FidelityFXCAS/blob/master/Docs/Screenshots/CAS_ON.png?raw=true)

Upscaling with screen space CAS OFF | Upscaling with screen space CAS ON
------------------------------------|-----------------------------------
![CAS OFF (50% upscaling)](https://github.com/amedrycki/FidelityFXCAS/blob/master/Docs/Screenshots/CAS_OFF%20(50%25%20upscaling).png?raw=true) | ![CAS_ON (50% upscaling)](https://github.com/amedrycki/FidelityFXCAS/blob/master/Docs/Screenshots/CAS_ON%20(50%25%20upscaling).png?raw=true)

## Plugin Functionality
In short this plugin let's you do 4 things:
- Draw a texture to a render target using CAS.
- Draw a texture to a higher resolution render target using CAS and it's upsampling feature.
- Apply CAS in the UE rendering pipeline as a post process screen space effect.
- Replace UE's default upsample pass in the rendering pipeline with CAS' upsampling (requires small engine code modifications).

It is also a good reference material if you want to learn how to write Global Shaders for Unreal Engine.

# Installation
To add the plugin to your project:
1. Download the repository to the following folder in your project `<ProjectName>/Plugins/FidelityFXCAS/`.
1. Right click your project's `.uproject` file and choose **Generate Visual Studio project files**.
1. Open your projects' solution in Visual Studio and compile.
1. Enable the plugin in your 
  1. Open your project in Unreal Editor and choose **Edit -> Plugins** from the menu to open the **Plugins** manager window.
  1. The plugin should be visible in the category **Project -> Rendering**.
  1. Select the **FidelityFXCAS** plugin and check **Enable**. You may be asked to rebuild the plugin and restart the editor.

## [Optional] Unreal Engine source code modification to enable screen space CAS with upsampling
This step is only required to enable the CAS screen space upsampling plugin functionality. If you skip this step you may still use other plugin functionalities.
Unreal Engine v4.24 does not provide any means to replace the default upsampling pipeline with your custom algorithms. Therefore to add the callback for the plugin to use the following changes in 3 engine source code files are required.

In the file `Engine/Source/Runtime/RenderCore/Public/RendererInterface.h` in line **748** add a delegate declaration and a callback accessor:
```diff
   /** Calls registered post resolve delegates, if any */
   virtual void RenderPostResolvedSceneColorExtension(FRHICommandListImmediate& RHICmdList, class FSceneRenderTargets& SceneContext) = 0;

+  // Delegate that is called when substituting upscale pass with a custom one.
+  DECLARE_DELEGATE_ThreeParams(FOnCustomUpscalePass, class FRDGBuilder& /*GraphBuilder*/, class FRDGTexture* /*SceneColor*/, const FRenderTargetBinding& /*RTBinding*/);
+
+  // Accessor for custom upscale pass delegate
+  virtual FOnCustomUpscalePass& GetCustomUpscalePassCallback() = 0;
+
   virtual void PostRenderAllViewports() = 0;

   virtual IAllocatedVirtualTexture* AllocateVirtualTexture(const FAllocatedVTDescription& Desc) = 0;
```

In the file `Engine/Source/Runtime/Renderer/Private/RendererModule.h` add the callback accessor definition in line **97** and delegate member variable in line **121**:
```diff

   virtual void RenderPostResolvedSceneColorExtension(FRHICommandListImmediate& RHICmdList, class FSceneRenderTargets& SceneContext) override;

+  // Accessor for custom upscale pass delegate
+  virtual FOnCustomUpscalePass& GetCustomUpscalePassCallback()
+  {
+    return CustomUpscalePassCallback;
+  }
+
   virtual void PostRenderAllViewports() override;

   virtual IAllocatedVirtualTexture* AllocateVirtualTexture(const FAllocatedVTDescription& Desc) override;
```

```diff
   FPostOpaqueRenderDelegate PostOpaqueRenderDelegate;
   FPostOpaqueRenderDelegate OverlayRenderDelegate;
   FOnResolvedSceneColor PostResolvedSceneColorCallbacks;
+  // Custom upscale pass delegate
+  FOnCustomUpscalePass CustomUpscalePassCallback;
 };

 extern ICustomCulling* GCustomCullingImpl;
```

In the file `Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessUpscale.cpp` add `EngineModule.h` include in line **5** and call the callback in line **201**:

```diff
 #include "PostProcess/PostProcessUpscale.h"
 #include "SceneFilterRendering.h"
+#include "EngineModule.h"

 namespace
 {
```

```diff
     Output.LoadAction = ERenderTargetLoadAction::EClear;
   }
 
+  // Custom upscale pass replacement callback
+  IRendererModule& RendererModule = GetRendererModule();
+  if (RendererModule.GetCustomUpscalePassCallback().IsBound())
+  {
+    RendererModule.GetCustomUpscalePassCallback().Execute(GraphBuilder, Inputs.SceneColor.Texture, Output.GetRenderTargetBinding());
+    return MoveTemp(Output);	// Don't continue with the default upscale
+  }
+  // End: Custom upscale pass replacement callback
+
   const FScreenPassTextureViewport InputViewport(Inputs.SceneColor);
   const FScreenPassTextureViewport OutputViewport(Output);
```

After you've made the changes to the source code you need to let the module know it can use them.
Change the following line in the `CASTest1/Plugins/FidelityFXCAS/Source/FidelityFXCAS/FidelityFXCAS.Build.cs` file:
```diff
    // Change the following to 1 to enable upscaling, after you've modified the UE sources by adding upscale callback
-    PublicDefinitions.Add("FX_CAS_CUSTOM_UPSCALE_CALLBACK=0");
+    PublicDefinitions.Add("FX_CAS_CUSTOM_UPSCALE_CALLBACK=1");
```

After making the changes to the source code you will have to compile your project.

# Usage

## Usage example
For a working example see the [FidelityFXCASExample](https://github.com/amedrycki/FidelityFXCASExample) repository containing a working Unreal Engine (v4.24) C++ project.

## Screen space CAS
After running your game open the console (by pressing `` ` ``) and use the follwing console variables:
- `r.fxcas.DisplayInfo` - Enables onscreen inormation display for AMD FidelityFX CAS plugin.
  - `0` Disabled - no information is displayed (default)
  - `1` Enabled - displays information about the CAS plugin status
- `r.fxcas.SSCAS` - Enables screen space contrast adaptive sharpening.
  - `0` Disabled (default)
  - `1` Enabled
- `r.fxcas.SSCASSharpness` - Sets the screen space CAS sharpness parameter value (default: 0.5).
  - `0` minimum (lower ringing)
  - `1` maximum (higher ringing)

## Screen space CAS with upsampling
After running your game open the console (by pressing `` ` ``) and change the render resolution to half the size using the console variable `r.ScreenPercentage 50` and enable FX CAS with `r.fxcass.SSCAS 1`.

If you enabled the custom upsampling callback by applying the engine source code modifications described in the section **[Optional] Unreal Engine source code modification to enable screen space CAS with upsampling** above, the rendering pipeline will automatically use the FX CAS upsampling. If you turn off FX CAS with `r.fxcass.SSCAS 0` the render pipeline will switch back to the default upsampling.

If you did not apply the engine source code modifications the screen space CAS will still work and sharpen the image in a postprocess before the upsampling takes plase. Then the render pipeline will apply the default upsampling algorithms.

## Rendering a texture to a render target
To render a texture to a render target use the `DrawToRenderTarget` method provided by the plugin's blueprint library.
```cpp
UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS", meta = (WorldContext = "WorldContextObject"))
static void DrawToRenderTarget(class UTextureRenderTarget2D* InOutputRenderTarget, class UTexture2D* InInputTexture);
```

## Pre-initializing compute shader outputs
The plugin needs buffers for compute shader to work. There are two buffers needed for the screen space CAS and one buffer for each texture render target you use. The plugin will do the automatic lazy initialization of the necessary buffers during the first render pass. However, you can-preinitialize the necessary buffers to avoid any possible performance drops later.

To pre-initialize the screen space buffers you can use the blueprint method `void InitSSCASCSOutputs(const FIntPoint& Size)` or the C++ method `void InitSSCASCSOutputs(const FIntPoint& Size)` provided by the module.

To pre-initialize the render target buffers you can use the blueprint method `void InitCSOutput(class UTextureRenderTarget2D* InOutputRenderTarget)`.

## Module API methods
- Module access methods
  - `static bool IsAvailable()` - returns true if the module is loaded
  - `static FFidelityFXCASModule& Get()` - returns the loaded module reference
- Screen space CAS controls
  - `bool GetIsSSCASEnabled() const` - returns true if SS CAS is enabled
  - `void SetIsSSCASEnabled(bool Enabled)` - enables / disables SS CAS
  - `void EnableSSCAS()` - enables SS CAS
  - `void DisableSSCAS()` - disables SS CAS
- Screen space CAS shader parameters
  - `float GetSSCASSharpness() const` - returns the current value of the Sharpness parameter
  - `void SetSSCASSharpness(float Sharpness)` - sets the Sharpness parameter (the Sharpness value should be in the range [0, 1])
  - `bool GetUseFP16() const` - returns true if SS CAS is using the half-precision version of the shader
  - `void SetUseFP16(bool UseFP16)` enables / disables the use of half-presicions shader for SS CAS
- Initialization
  - `void InitSSCASCSOutputs(const FIntPoint& Size)` - initializes the compute shader outputs for SS CAS
- SS CAS resolution
  - `void GetSSCASResolutionInfo(FIntPoint& OutInputResolution, FIntPoint& OutOutputResolution) const` - gets the input and output resolutions for SS CAS
  - `FIntPoint GetSSCASInputResolution() const` - returns the current input resolution for SS CAS
  - `FIntPoint GetSSCASOutputResolution() const` - return the current output resolution for SS CAS

## Blueprint library API
- Screen space CAS controls
  - `bool GetIsSSCASEnabled()` - returns true if screen space CAS is enabled
  - `void SetIsSSCASEnabled(bool bEnabled)` - enables / disables screen space CAS
  - `void ToggleIsSSCASEnabled()` - toggles SS CAS between enabled and disabled
  - `void EnableSSCAS()` - enables SS CAS
  - `void DisableSSCAS()` - disables SS CAS
  - `void InitSSCASCSOutputs(const FIntPoint& Size)` - initializes SS CAS compute shader output buffers
- Screen space CAS shader parameters
  - `float GetSSCASSharpness()` - returns the current value of the Sharpness parameter
  - `void SetSSCASSharpness(float Sharpness)` - sets the Sharpness parameter (the Sharpness value should be in the range [0, 1])
  - `bool GetUseFP16()` - returns true if SS CAS is using the half-precision version of the shader
  - `void SetUseFP16(bool UseFP16)` - enables / disables the use of half-presicions shader for SS CAS
- Render to render target methods
  - `void InitCSOutput(class UTextureRenderTarget2D* InOutputRenderTarget)` - initializes compute shader output buffer for a given render target
  - void DrawToRenderTarget(class UTextureRenderTarget2D* InOutputRenderTarget, class UTexture2D* InInputTexture)` - renders a texture to a render target and aplies CAS and upscaling (if the render target resolution is greater than the texture resolution).

# References and reading resources:
- More info about Fidelity FX and CAS: [https://gpuopen.com/gaming-product/fidelityfx/](https://gpuopen.com/gaming-product/fidelityfx/)
- UE Screen Percentage with Temporal Upsample overview: [https://docs.unrealengine.com/en-US/Engine/Rendering/ScreenPercentage/index.html](https://docs.unrealengine.com/en-US/Engine/Rendering/ScreenPercentage/index.html)
- UE RDG 101 A Crash Course presentation: [https://epicgames.ent.box.com/s/ul1h44ozs0t2850ug0hrohlzm53kxwrz](https://epicgames.ent.box.com/s/ul1h44ozs0t2850ug0hrohlzm53kxwrz)
- UE Overview Shader in Plugins (for UE v4.17): [https://docs.unrealengine.com/en-US/Programming/Rendering/ShaderInPlugin/Overview/index.html](https://docs.unrealengine.com/en-US/Programming/Rendering/ShaderInPlugin/Overview/index.html)
- UE global shader tutorial: [https://docs.unrealengine.com/en-US/Programming/Rendering/ShaderInPlugin/QuickStart/index.html](https://docs.unrealengine.com/en-US/Programming/Rendering/ShaderInPlugin/QuickStart/index.html)
- How to Add Global Shaders to UE4 article: [https://www.unrealengine.com/en-US/tech-blog/how-to-add-global-shaders-to-ue4](https://www.unrealengine.com/en-US/tech-blog/how-to-add-global-shaders-to-ue4)
- Fredrik Lindh's shader plugin demo: [https://github.com/Temaran/UE4ShaderPluginDemo](https://github.com/Temaran/UE4ShaderPluginDemo)
- @GeorgeR's ShaderPlus plugin: [https://github.com/GeorgeR/ShadersPlus](https://github.com/GeorgeR/ShadersPlus)
- Valentin Kraft's compute shader tutorial: [http://www.valentinkraft.de/compute-shader-in-unreal-tutorial/](http://www.valentinkraft.de/compute-shader-in-unreal-tutorial/)
