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
	// 1) 开头写数组标记
	FString JsonOut = TEXT("[\n");

	for (int32 i = 0; i < Records.Num(); ++i)
	{
		const FOrionStructureRecord& R = Records[i];

		// 拆 Transform
		const FVector Loc = R.Transform.GetLocation();
		const FRotator Rot = R.Transform.GetRotation().Rotator();
		const FVector Scale = R.Transform.GetScale3D();

		// 拼一个对象
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
		// 如果不是最后一个，就加逗号
		if (i < Records.Num() - 1)
		{
			JsonOut += TEXT(",");
		}
		JsonOut += TEXT("\n");
	}

	JsonOut += TEXT("]\n");

	// 2) 写盘
	const FString SaveDir = FPaths::ProjectSavedDir(); // e.g. ".../Saved/"
	const FString FullPath = SaveDir / Filename; // e.g. ".../Saved/structure_records.json"

	if (!FFileHelper::SaveStringToFile(JsonOut, *FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[SaveJSON] 写 JSON 文件失败: %s"), *FullPath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[SaveJSON] 成功写入 JSON: %s"), *FullPath);
	return true;
}

void UOrionGameInstance::SaveGame()
{
	auto* SaveObj = Cast<UOrionSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UOrionSaveGame::StaticClass()));

	//TArray<FOrionStructureRecord> Records;

	/* ① 让 BuildingManager 把所有建筑快照写进存档对象 */
	if (auto* BM = GetSubsystem<UOrionBuildingManager>())
	{
		TArray<FOrionStructureRecord> Records;
		BM->CollectStructureRecords(Records);

		// 手写 JSON 存一份到 Saved 目录
		SaveStructureRecordsToJsonFile_Manual(Records, TEXT("structure_records.json"));

		// 再把 Records 存到 SaveGame 对象里
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

	/* ① — 清空旧建筑 & Socket 池 — */
	for (TActorIterator<AOrionStructure> It(World); It; ++It)
	{
		It->Destroy();
	}
	if (auto* BuildingManager = GetSubsystem<UOrionBuildingManager>())
	{
		BuildingManager->ResetAllSockets(World);
	}

	/* ② — 重新生成建筑（BeginPlay 里会自动 RegisterSocket） — */
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

	/* ……此处可追加恢复其他系统数据…… */

	//UE_LOG(LogTemp, Log, TEXT("[Load] Game loaded from slot %s"), SlotName);
}
