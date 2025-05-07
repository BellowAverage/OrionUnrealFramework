// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Orion : ModuleRules
{
	public Orion(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        //AddEngineThirdPartyPrivateStaticDependencies(Target, "Eigen");

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "Niagara", "UMG", "Slate", "SlateCore", "ChaosVehicles" });

        PrivateDependencyModuleNames.AddRange(new string[] { "NavigationSystem" });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }

}
