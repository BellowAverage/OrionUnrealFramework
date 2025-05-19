// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionGameInstance/OrionGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Orion/OrionSaveGame/OrionSaveGame.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "Orion/OrionStructure/OrionStructure.h"

void UOrionGameInstance::SaveGame()
{
	auto* SaveObj = Cast<UOrionSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UOrionSaveGame::StaticClass()));

	/* ① 让 BuildingManager 把所有建筑快照写进存档对象 */
	if (const auto* BuildingManager = GetSubsystem<UOrionBuildingManager>())
	{
		BuildingManager->CollectStructureRecords(SaveObj->SavedStructures);
	}

	/* ……此处可追加保存其他系统数据…… */

	UGameplayStatics::SaveGameToSlot(SaveObj, SlotName, 0);
	UE_LOG(LogTemp, Log, TEXT("[Save] Game saved to slot %s"), SlotName);
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

	UE_LOG(LogTemp, Log, TEXT("[Load] Game loaded from slot %s"), SlotName);
}
