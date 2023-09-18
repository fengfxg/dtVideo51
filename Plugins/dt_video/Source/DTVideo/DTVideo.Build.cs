// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DTVideo : ModuleRules
{
	public DTVideo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[] {
					"Core",
				}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[] {
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore",
					"AVEncoder",
					"RHI",
					"GameplayMediaEncoder",
					"RenderCore",
                    "MediaUtils",
					"AudioMixer",
                }
			);

        PrivateDependencyModuleNames.Add("VulkanRHI");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan", "CUDA");

        if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
        {
            PrivateDependencyModuleNames.Add("D3D11RHI");
            PrivateDependencyModuleNames.Add("D3D12RHI");

            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11", "DX12");
        }

        DynamicallyLoadedModuleNames.AddRange(
			new string[]
				{
				}
			);
	}
}
