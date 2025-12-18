// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OrionFactionManager.generated.h"

class UOrionInventoryManager;
struct FOrionDataItem;
class AOrionPlayerController;

UENUM(BlueprintType)
enum class EFaction : uint8
{
	PlayerFaction UMETA(DisplayName = "Player Faction"),
	Vagrants UMETA(DisplayName = "Vagrants"),
};

struct FFaction
{
	const EFaction Faction;
	const FString Description;

	int Gold;

	FFaction(const EFaction InFaction, const FString& InDescription, const int InGold)
		: Faction(InFaction), Description(InDescription), Gold(InGold) {
	};
};


UCLASS()
class ORION_API UOrionFactionManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	void InitFactions();
	void OnWorldInitializedActors(const FActorsInitializedParams& ActorsInitializedParams);
	virtual void Initialize(FSubsystemCollectionBase&) override;

	bool ModifyFactionGold(const EFaction Faction, const int Gold);
	bool DeduceFactionResources(const EFaction Faction, const int32 ItemId, const int32 Amount);

	static const TMap<int32, TArray<TPair<int32, int32>>> BuildingCostMap;
	bool AffordBuildingCost(int32 BuildingId);

	UFUNCTION(BlueprintCallable, Category = "OrionFactionManager")
	bool IsHostile(const EFaction FactionA, const EFaction FactionB) const;

private:

	friend class AOrionHUD;

	// 检查资源是否充足（不扣减）
	bool CheckFactionResources(const EFaction Faction, const int32 ItemId, const int32 Amount) const;

	UPROPERTY() AOrionPlayerController* PlayerController = nullptr;
	TMap<EFaction, FFaction> FactionsDataMap;

	UPROPERTY() UOrionInventoryManager* InventoryManager;


};
