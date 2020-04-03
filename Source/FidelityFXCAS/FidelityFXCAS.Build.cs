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
			}
			);

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
    }
}
