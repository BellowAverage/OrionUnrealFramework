// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Orion/OrionSaveGame/OrionSaveGame.h"
#include "OrionGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class ORION_API UOrionGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	void SaveAllCharacters(UOrionSaveGame* SaveObj) const;
	void LoadAllCharacters(UOrionSaveGame* LoadObj, UWorld* World) const;


	void SaveAllBuildings(UOrionSaveGame* SaveObj) const;
	void LoadAllBuildings(UOrionSaveGame* LoadObj, UWorld* World) const;

	UFUNCTION(BlueprintCallable)
	void SaveGame(const FString& InSlotName);

	UFUNCTION(BlueprintCallable)
	void LoadGame(const FString& InSlotName);

	UFUNCTION(BlueprintCallable)
	void SaveGameWithDialog();

	UFUNCTION(BlueprintCallable)
	void LoadGameWithDialog();

	// 建筑数据表配置（可在编辑器中指定）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Data", meta = (AllowedClasses = "DataTable"))
	TObjectPtr<UDataTable> BuildingDataTable = nullptr;

private:
	static constexpr const TCHAR* SlotName = TEXT("OrionSlot");

	static const TCHAR* OrionActionToString(EOrionAction Type)
	{
		switch (Type)
		{
		case EOrionAction::MoveToLocation: return TEXT("MoveToLocation");
		case EOrionAction::AttackOnChara: return TEXT("AttackOnChara");
		case EOrionAction::InteractWithActor: return TEXT("InteractWithActor");
		case EOrionAction::InteractWithProduction: return TEXT("InteractWithProduction");
		case EOrionAction::CollectCargo: return TEXT("CollectCargo");
		case EOrionAction::CollectBullets: return TEXT("CollectBullets");
		default: return TEXT("Undefined");
		}
	}

	static void DumpSaveGame(const UOrionSaveGame* SaveObj)
	{
		if (!SaveObj)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DumpSave] SaveObj == null"));
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("=======  DumpSaveGame  ========"));

		/* ① 角色 */
		for (int32 i = 0; i < SaveObj->SavedCharacters.Num(); ++i)
		{
			const FOrionCharaSerializable& S = SaveObj->SavedCharacters[i];

			UE_LOG(LogTemp, Warning,
			       TEXT("[%02d] CharaId:%s  Pos:(%s)  Rot:(%s)  ProcActions:%d"),
			       i,
			       *S.CharaGameId.ToString(EGuidFormats::Digits),
			       *FString::Printf(TEXT("%.1f,%.1f,%.1f"),
				       S.CharaLocation.X, S.CharaLocation.Y, S.CharaLocation.Z),
			       *FString::Printf(TEXT("%.1f,%.1f,%.1f"),
				       S.CharaRotation.Pitch, S.CharaRotation.Yaw, S.CharaRotation.Roll),
			       S.SerializedProcActions.Num());

			for (int32 j = 0; j < S.SerializedProcActions.Num(); ++j)
			{
				const FOrionActionParams& P = S.SerializedProcActions[j];

				UE_LOG(LogTemp, Warning,
				       TEXT("      └─[%02d] %-22s  Target:%s  Loc:(%.1f,%.1f,%.1f)"),
				       j,
				       OrionActionToString(P.OrionActionType),
				       *P.TargetActorId.ToString(EGuidFormats::Digits),
				       P.TargetLocation.X, P.TargetLocation.Y, P.TargetLocation.Z);
			}
		}

		/* ② 建筑等……(如有需要自行补充) */

		UE_LOG(LogTemp, Warning, TEXT("=======  Dump End  ========"));
	}
};
