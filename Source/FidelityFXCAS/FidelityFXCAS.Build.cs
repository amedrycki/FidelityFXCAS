// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FidelityFXCAS : ModuleRules
{
    public FidelityFXCAS(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // List of modules names (no path needed) with header files that our module's public headers needs access to, but we don't need to "import" or link against.
        PublicIncludePathModuleNames.AddRange(
            new string[]
            {
            });

        // List of public dependency module names (no path needed) (automatically does the private/public include). These are modules that are required by our public source files.
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "Engine",
                "RenderCore",
                "RHI"
				// ... add other public dependencies that you statically link with here ...
			});

        // List of modules name (no path needed) with header files that our module's private code files needs access to, but we don't need to "import" or link against.
        PrivateIncludePathModuleNames.AddRange(
            new string[]
            {
            });

        // List of private dependency module names. These are modules that our private code depends on but nothing in our public include files depend on.
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Projects",
                "Renderer"
				// ... add private dependencies that you statically link with here ...	
			});

        // (This setting is currently not need as we discover all files from the 'Public' folder) List of all paths to include files that are exposed to other modules
        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
			});

        // List of all paths to this module's internal include files, not exposed to other modules (at least one include to the 'Private' path, more if we want to avoid relative paths)
        PrivateIncludePaths.AddRange(
            new string[] {
				// ... add other private include paths required here ...
			});

        // Addition modules this module may require at run-time 
        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			});

        // Enable / disable whole plugin depenging on the platform
        // NOTE: If you change this rule, you may also need to update the
        // FFidelityFXCASShaderCompilationRules::ShouldCompilePermutation method
        if (Target.Platform == UnrealTargetPlatform.Win64
			|| Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			PublicDefinitions.Add("FX_CAS_PLUGIN_ENABLED=1");
		}
		else
		{
			// The plugin will still compile, but with dummy method stubs to prevent possible compilation errors
			// in other places in the code or in blueprints
			PublicDefinitions.Add("FX_CAS_PLUGIN_ENABLED=0");
		}

        // Enable the FP16 compute shader version depenging on the platform
        // NOTE: If you change this rule, you may also need to update the
        // FFidelityFXCASShaderCompilationRules::ShouldCompilePermutationCS method
        if (Target.Platform == UnrealTargetPlatform.Win64)	// FP16 doesn't cook on XboxOne
        {
            PublicDefinitions.Add("FX_CAS_FP16_ENABLED=1");
        }
        else
        {
            // The plugin will still compile, but with dummy method stubs to prevent possible compilation errors
            // in other places in the code or in blueprints
            PublicDefinitions.Add("FX_CAS_FP16_ENABLED=0");
        }

        // Change the following to 1 to enable upscaling, after you've modified the UE sources by adding upscale callback
        PublicDefinitions.Add("FX_CAS_CUSTOM_UPSCALE_CALLBACK=0");
	}
}
