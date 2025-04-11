// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "OrionChara.h"
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OrionGameMode.generated.h"


USTRUCT(BlueprintType)
struct ORION_API FCharaStats
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 CharaID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	UTexture2D* CharaSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FName CharaName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 level;

	FCharaStats() : CharaID(0), CharaSnapshot(nullptr), CharaName(NAME_None), Health(0), level(0) {};
};

UCLASS(Blueprintable)
class ORION_API AOrionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AOrionGameMode();

	void BeginPlay() override;

    void Tick(float DeltaTime) override;

	/* Gameplay Stats */


	/* Developer */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Developer")
	TSubclassOf<AOrionChara> SubclassOfOrionChara;

	UFUNCTION(BlueprintCallable, Category = "Developer")
	void SpawnCharaInstance(FVector SpawnLocation);


};
