
using UnrealBuildTool;
using System.IO;
public class BloomTensorcoreExecuteD3D12RHI : ModuleRules
{
	public BloomTensorcoreExecuteD3D12RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;
		bUseUnity = false;
		CppStandard = CppStandardVersion.Cpp17;
		PublicIncludePaths.AddRange(
			new string[] {
			}
			);
		PublicSystemLibraries.AddRange(new string[] { "shlwapi.lib", "runtimeobject.lib" });


		PrivateIncludePaths.AddRange(
		new string[] {
				Path.Combine(EngineDirectory,"Source/Runtime/D3D12RHI/Private"),
				Path.Combine(EngineDirectory,"Source/Runtime/D3D12RHI/Private/Windows"),
				Path.Combine(Target.WindowsPlatform.WindowsSdkDir,        
												"Include", 
												Target.WindowsPlatform.WindowsSdkVersion, 
												"cppwinrt")
		}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
					"Core",
					"CoreUObject",
					"EngineSettings",
					"Engine",
					"RenderCore",
					"Renderer",
					"RHI",
					"D3D12RHI",
					"BloomTensorcoreExecuteRHI",
			}
			);

		if (ReadOnlyBuildVersion.Current.MajorVersion == 5)
		{
			PrivateDependencyModuleNames.Add("RHICore");
		}
		// those come from the D3D12RHI
		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");

		PublicDefinitions.Add("WITH_NVAPI=0");
		PublicDefinitions.Add("NV_AFTERMATH=0");
        PublicDefinitions.Add("INTEL_EXTENSIONS=0");
		// AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
		// AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
		// AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
		// AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelExtensionsFramework");
	}
}
