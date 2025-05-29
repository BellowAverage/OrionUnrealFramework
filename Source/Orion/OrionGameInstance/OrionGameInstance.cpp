// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionGameInstance/OrionGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "OrionCharaManager.h"
#include "Orion/OrionSaveGame/OrionSaveGame.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "Orion/OrionStructure/OrionStructure.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "OrionActorManager.h"
#include "OrionInventoryManager.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

class FSaveGameArchive : public FObjectAndNameAsStringProxyArchive
{
public:
	FSaveGameArchive(FArchive& InInnerArchive, bool bIsSave)
		: FObjectAndNameAsStringProxyArchive(InInnerArchive, /*bLoadIn=*/ !bIsSave)
	{
		ArIsSaveGame = true;
	}
};

void UOrionGameInstance::SaveGameWithDialog()
{
	/* ① 让玩家选一个文件名 */
	IDesktopPlatform* DP = FDesktopPlatformModule::Get();
	if (!DP)
	{
		return;
	}

	void* ParentWindow = nullptr;
	const FString DefaultPath = FPaths::ProjectSavedDir();
	const FString DefaultName = TEXT("MySave.sav");
	const FString FileTypes = TEXT("SaveGame Files (*.sav)|*.sav");

	TArray<FString> OutFiles;
	bool bOk = DP->SaveFileDialog(ParentWindow,
	                              TEXT("Save Orion Game"), // 标题
	                              DefaultPath,
	                              DefaultName,
	                              FileTypes,
	                              EFileDialogFlags::None,
	                              OutFiles);

	if (!bOk || OutFiles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] User cancelled save dialog."));
		return;
	}
	FString SavePath = OutFiles[0];

	/* ② 组装 SaveGame 对象 */
	UOrionSaveGame* SaveObj = Cast<UOrionSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UOrionSaveGame::StaticClass()));

	// 把你现有的收集函数搬过来
	SaveAllCharacters(SaveObj);
	SaveAllBuildings(SaveObj);

	/* ③ 序列化到内存 */
	FBufferArchive MemWriter;
	FSaveGameArchive ProxyAr(MemWriter, /*bIsSave=*/true);

	SaveObj->Serialize(ProxyAr); // <- 核心一步

	if (MemWriter.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[Save] Serialize produced 0 bytes!"));
		return;
	}

	/* ④ 写文件 */
	if (FFileHelper::SaveArrayToFile(MemWriter, *SavePath))
	{
		UE_LOG(LogTemp, Log, TEXT("[Save] OK  ->  %s  (%d bytes)"),
		       *SavePath, MemWriter.Num());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Save] FAILED to write %s"), *SavePath);
	}

	MemWriter.FlushCache();
	MemWriter.Empty();
}

/* ====================================================
 *                  读取
 * ====================================================*/
void UOrionGameInstance::LoadGameWithDialog()
{
	IDesktopPlatform* DP = FDesktopPlatformModule::Get();
	if (!DP)
	{
		return;
	}

	void* ParentWindow = nullptr;
	const FString DefaultPath = FPaths::ProjectSavedDir();
	const FString FileTypes = TEXT("SaveGame Files (*.sav)|*.sav");

	TArray<FString> OutFiles;
	bool bOk = DP->OpenFileDialog(ParentWindow,
	                              TEXT("Load Orion Game"),
	                              DefaultPath,
	                              TEXT(""),
	                              FileTypes,
	                              EFileDialogFlags::None,
	                              OutFiles);

	if (!bOk || OutFiles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Load] User cancelled open dialog."));
		return;
	}
	FString LoadPath = OutFiles[0];

	/* ① 读文件到内存 */
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *LoadPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[Load] Cannot read file %s"), *LoadPath);
		return;
	}

	/* ② 反序列化 */
	FMemoryReader MemReader(FileData);
	FSaveGameArchive ProxyAr(MemReader, /*bIsSave=*/false);

	UOrionSaveGame* LoadObj = NewObject<UOrionSaveGame>();
	LoadObj->Serialize(ProxyAr);

	MemReader.FlushCache();
	MemReader.Close();

	if (!LoadObj)
	{
		UE_LOG(LogTemp, Error, TEXT("[Load] Deserialize failed!"));
		return;
	}

	/* ③ 用自己原先的逻辑恢复世界 */
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	LoadAllCharacters(LoadObj, World);
	LoadAllBuildings(LoadObj, World);

	UE_LOG(LogTemp, Log, TEXT("[Load] OK  <-  %s"), *LoadPath);
}

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

void UOrionGameInstance::SaveGame(const FString& InSlotName)
{
	auto* SaveObj = Cast<UOrionSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UOrionSaveGame::StaticClass()));

	// SaveAllBuildings(SaveObj);
	SaveAllCharacters(SaveObj);
	if (auto* AMgr = GetSubsystem<UOrionActorManager>())
	{
		TArray<FOrionActorFullRecord> Recs;
		AMgr->CollectActorRecords(Recs);
		SaveObj->SavedActors = MoveTemp(Recs);
	}
	if (auto* IMgr = GetSubsystem<UOrionInventoryManager>())
	{
		TArray<FOrionInventorySerializable> Recs;
		IMgr->CollectInventoryRecords(Recs);
		SaveObj->SavedInventories = MoveTemp(Recs);
	}


	UGameplayStatics::SaveGameToSlot(SaveObj, SlotName, 0);
	DumpSaveGame(SaveObj);
	//UE_LOG(LogTemp, Log, TEXT("[Save] Game saved to slot %s"), *SlotName);
}

void UOrionGameInstance::LoadGame(const FString& InSlotName)
{
	auto* LoadObj = Cast<UOrionSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, 0));

	if (!LoadObj)
	{
		//UE_LOG(LogTemp, Warning, TEXT("[Load] No SaveGame found in slot %s"), InSlotName);
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	// LoadAllBuildings(LoadObj, World);

	if (auto* AMgr = GetSubsystem<UOrionActorManager>())
	{
		AMgr->LoadAllActors(World, LoadObj->SavedActors); // ★现在类型匹配
	}
	LoadAllCharacters(LoadObj, World);
	if (auto* IMgr = GetSubsystem<UOrionInventoryManager>())
	{
		IMgr->ApplyInventoryRecords(LoadObj->SavedInventories);
	}
}


void UOrionGameInstance::SaveAllCharacters(UOrionSaveGame* SaveObj) const
{
	if (auto* CharaManager = GetSubsystem<UOrionCharaManager>())
	{
		TArray<FOrionCharaSerializable> Records;
		CharaManager->CollectCharacterRecords(Records);

		SaveObj->SavedCharacters = MoveTemp(Records);
	}
}

void UOrionGameInstance::LoadAllCharacters(UOrionSaveGame* LoadObj, UWorld* World) const
{
	if (auto* CharaManager = GetSubsystem<UOrionCharaManager>())
	{
		CharaManager->RemoveAllCharacters(World);
		CharaManager->LoadAllCharacters(World, LoadObj->SavedCharacters);
	}
}

void UOrionGameInstance::SaveAllBuildings(UOrionSaveGame* SaveObj) const
{
	//TArray<FOrionStructureRecord> Records;

	/* ① Let BuildingManager write all building snapshots to save object */
	if (const auto* BuildingManager = GetSubsystem<UOrionBuildingManager>())
	{
		TArray<FOrionStructureRecord> Records;
		BuildingManager->CollectStructureRecords(Records);

		// Manually write JSON to save a copy to Saved directory
		SaveStructureRecordsToJsonFile_Manual(Records, TEXT("structure_records.json"));

		// Then store Records in SaveGame object
		SaveObj->SavedStructures = MoveTemp(Records);
	}
}

void UOrionGameInstance::LoadAllBuildings(UOrionSaveGame* LoadObj, UWorld* World) const
{
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
