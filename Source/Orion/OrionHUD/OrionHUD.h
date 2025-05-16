#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Orion/OrionHUD/OrionUserWidgetCharaInfo.h"
#include "OrionHUD.generated.h"

UCLASS()
class ORION_API AOrionHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/* Developer UI Base */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> WB_DeveloperUIBase;


	/* Player Operation Menu */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> WB_PlayerOperationMenu;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TArray<FName> ArrOperationAvailable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FHitResult PlayerOperationSpawnReference;

	void ShowPlayerOperationMenu(
		float MouseX,
		float MouseY,
		const FHitResult& HitResult = FHitResult(),
		const TArray<FString>& ArrOptionNames = TArray<FString>()
	);

	/* Chara Info Dashboard */

	UFUNCTION(BlueprintCallable, Category = "UI")
	void InitCharaInfoPanel();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowCharaInfoPanel();


	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideCharaInfoPanel();

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UOrionUserWidgetCharaInfo> WB_CharaInfoPanel;

	UPROPERTY(Transient)
	UOrionUserWidgetCharaInfo* CharaInfoPanel = nullptr;

	bool bShowCharaInfoPanel = false;
	bool PreviousbShowCharaInfoPanel = false;
	AOrionChara* InfoChara = nullptr;
	FString PreviousInfoChara = "";

	/* OrionActor Hoving Info Panel */
	TArray<FString> InfoLines;
	bool bShowActorInfo;
	void ShowInfoAtMouse(const TArray<FString>& InLines);
};
