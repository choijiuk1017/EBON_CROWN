// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Third_Person_RPG : ModuleRules
{
	public Third_Person_RPG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core",
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput",
            "UMG",
			"Slate",       
			"SlateCore" ,  

			"NavigationSystem",
			"AIModule",
			"GameplayTasks",
            "LevelSequence",
            "MovieScene",        
			"MovieSceneTracks"
        });

		PrivateDependencyModuleNames.AddRange(new string[] { "AssetRegistry" });

        if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new string[] { "UMGEditor", "UnrealEd" });
        PublicIncludePaths.Add("Third_Person_RPG");
        RuntimeDependencies.Add("$(ProjectDir)/Content/UI/InputPrompts/...", StagedFileType.NonUFS);

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
