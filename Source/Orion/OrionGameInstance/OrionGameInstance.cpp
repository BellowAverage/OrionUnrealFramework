// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionGameInstance/OrionGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Orion/OrionSaveGame/OrionSaveGame.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "Orion/OrionStructure/OrionStructure.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool SaveStructureRecordsToJsonFile_Manual(
	const TArray<FOrionStructureRecord>& Records,
	const FString& Filename)
{
	// 1) Write array marker at the beginning
	FString JsonOut = TEXT("[\n");

	for (int32 i = 0; i < Records.Num(); ++i)
	{
		const FOrionStructureRecord& R = Records[i];

		// Decompose Transform
		const FVector Loc = R.Transform.GetLocation();
		const FRotator Rot = R.Transform.GetRotation().Rotator();
		const FVector Scale = R.Transform.GetScale3D();

		// Assemble an object
		JsonOut += TEXT("  {\n");
		JsonOut += FString::Printf(
			TEXT("    \"ClassPath\": \"%s\",\n"),
			*R.ClassPath
		);
		JsonOut += TEXT("    \"Translation\": {\n");
		JsonOut += FString::Printf(
			TEXT("      \"X\": %.6f, \"Y\": %.6f, \"Z\": %.6f\n"),
			Loc.X, Loc.Y, Loc.Z
		);
		JsonOut += TEXT("    },\n");

		JsonOut += TEXT("    \"Rotation\": {\n");
		JsonOut += FString::Printf(
			TEXT("      \"Pitch\": %.6f, \"Yaw\": %.6f, \"Roll\": %.6f\n"),
			Rot.Pitch, Rot.Yaw, Rot.Roll
		);
		JsonOut += TEXT("    },\n");

		JsonOut += TEXT("    \"Scale\": {\n");
		JsonOut += FString::Printf(
			TEXT("      \"X\": %.6f, \"Y\": %.6f, \"Z\": %.6f\n"),
			Scale.X, Scale.Y, Scale.Z
		);
		JsonOut += TEXT("    }\n");

		JsonOut += TEXT("  }");
		// If not the last one, add comma
		if (i < Records.Num() - 1)
		{
			JsonOut += TEXT(",");
		}
		JsonOut += TEXT("\n");
	}

	JsonOut += TEXT("]\n");

	// 2) Write to disk
	const FString SaveDir = FPaths::ProjectSavedDir(); // e.g. ".../Saved/"
	const FString FullPath = SaveDir / Filename; // e.g. ".../Saved/structure_records.json"

	if (!FFileHelper::SaveStringToFile(JsonOut, *FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[SaveJSON] Failed to write JSON file: %s"), *FullPath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[SaveJSON] Successfully wrote JSON: %s"), *FullPath);
	return true;
}

void UOrionGameInstance::SaveGame()
{
	auto* SaveObj = Cast<UOrionSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UOrionSaveGame::StaticClass()));

	//TArray<FOrionStructureRecord> Records;

	/* ① Let BuildingManager write all building snapshots to save object */
	if (auto* BM = GetSubsystem<UOrionBuildingManager>())
	{
		TArray<FOrionStructureRecord> Records;
		BM->CollectStructureRecords(Records);

		// Manually write JSON to save a copy to Saved directory
		SaveStructureRecordsToJsonFile_Manual(Records, TEXT("structure_records.json"));

		// Then store Records in SaveGame object
		SaveObj->SavedStructures = MoveTemp(Records);
	}

	UGameplayStatics::SaveGameToSlot(SaveObj, SlotName, 0);
	//UE_LOG(LogTemp, Log, TEXT("[Save] Game saved to slot %s"), *SlotName);
}

void UOrionGameInstance::LoadGame()
{
	auto* LoadObj = Cast<UOrionSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, 0));

	if (!LoadObj)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Load] No SaveGame found in slot %s"), SlotName);
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	/* ① — Clear old buildings & Socket pool — */
	for (TActorIterator<AOrionStructure> It(World); It; ++It)
	{
		It->Destroy();
	}
	if (auto* BuildingManager = GetSubsystem<UOrionBuildingManager>())
	{
		BuildingManager->ResetAllSockets(World);
	}

	/* ② — Regenerate buildings (BeginPlay will automatically RegisterSocket) — */
	for (const FOrionStructureRecord& Rec : LoadObj->SavedStructures)
	{
		UClass* StructClass =
			LoadClass<AOrionStructure>(nullptr, *Rec.ClassPath);
		if (!StructClass)
		{
			UE_LOG(LogTemp, Error,
			       TEXT("[Load] Failed to load class %s"), *Rec.ClassPath);
			continue;
		}
		World->SpawnActor<AOrionStructure>(StructClass, Rec.Transform);
	}

	/* ……Additional restoration of other system data can be added here…… */

	//UE_LOG(LogTemp, Log, TEXT("[Load] Game loaded from slot %s"), SlotName);
}
