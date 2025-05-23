#pragma once

#include "CoreMinimal.h"
#include "OrionUserWidgetBuildingMenu.h"
#include "OrionUserWidgetUIBase.h"
#include "GameFramework/HUD.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Orion/OrionHUD/OrionUserWidgetCharaInfo.h"
#include "Orion/OrionGlobals/OrionDataBuilding.h"
#include "OrionHUD.generated.h"

DECLARE_DELEGATE(FOnViewLevelUp);
DECLARE_DELEGATE(FOnViewLevelDown);

DECLARE_DELEGATE_TwoParams(FOnBuildingOptionSelected, int32, bool);
DECLARE_DELEGATE_OneParam(FOnToggleDemolishMode, bool);

//DECLARE_DELEGATE(FOnSaveGame);
//DECLARE_DELEGATE(FOnLoadGame);

UCLASS()
class ORION_API AOrionHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/* Building Menu */

	FOnBuildingOptionSelected OnBuildingOptionSelected;
	FOnToggleDemolishMode OnToggleDemolishMode;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UOrionUserWidgetBuildingMenu> WB_BuildingMenu;

	UPROPERTY()
	UOrionUserWidgetBuildingMenu* BuildingMenu = nullptr;;

	static const TArray<FOrionDataBuilding> BuildingMenuOptions;

	static const TMap<int32, FOrionDataBuilding> BuildingMenuOptionsMap;


	/* Developer UI Base */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UOrionUserWidgetUIBase> WB_DeveloperUIBase;

	FOnViewLevelUp OnViewLevelUp;
	FOnViewLevelUp OnViewLevelDown;
	//FOnSaveGame OnSaveGame;
	//FOnLoadGame OnLoadGame;


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
	void ShowBuildingMenu();
	void HideBuildingMenu();


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
