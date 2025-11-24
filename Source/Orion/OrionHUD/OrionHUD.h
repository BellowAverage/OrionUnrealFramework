#pragma once

#include "CoreMinimal.h"
#include "OrionUserWidgetBuildingMenu.h"
#include "OrionUserWidgetUIBase.h"
#include "GameFramework/HUD.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Orion/OrionHUD/OrionUserWidgetCharaInfo.h"
#include "OrionHUD.generated.h"

class AOrionPlayerController;
class UOrionBuildingManager;

UCLASS()
class ORION_API AOrionHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	virtual void BeginPlay() override;
	void TickCharaInfoPanel();

	virtual void Tick(float DeltaTime) override;

	/* Reference to External Game Resources */
	UPROPERTY()
	UOrionBuildingManager* BuildingManagerInstance;

	UPROPERTY()
	AOrionPlayerController* OrionPlayerControllerInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config (Non-null)")
	TSubclassOf<UOrionUserWidgetUIBase> WB_DeveloperUIBase;// Developer UI Base

	UPROPERTY(EditAnywhere, Category = "Config (Non-null)")
	TSubclassOf<UOrionUserWidgetCharaInfo> WB_CharaInfoPanel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config (Non-null")
	TSubclassOf<UUserWidget> WB_PlayerOperationMenu;

	UPROPERTY(Transient)
	UOrionUserWidgetCharaInfo* CharaInfoPanel = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UOrionUserWidgetBuildingMenu> WB_BuildingMenu;

	UPROPERTY()
	UOrionUserWidgetBuildingMenu* BuildingMenu = nullptr;;

	/* Player Operation Menu */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blueprint Use Only")
	TArray<FName> ArrOperationAvailable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blueprint Use Only")
	FHitResult PlayerOperationSpawnReference;

	void ShowPlayerOperationMenu(
		float MouseX,
		float MouseY,
		const FHitResult& HitResult = FHitResult(),
		const TArray<FString>& ArrOptionNames = TArray<FString>()
	);
	void TickDrawOrionActorInfoInPlace() const;
	void TickShowInfoAtMouse();

	/* Chara Info Panel */

	UFUNCTION(BlueprintCallable, Category = "Blueprint Use Only")
	void ShowCharaInfoPanel() const;

	UFUNCTION(BlueprintCallable, Category = "Blueprint Use Only")
	void HideCharaInfoPanel() const;

	bool IsShowCharaInfoPanel = false;
	bool IsPreviousShowCharaInfoPanel = false;

	UPROPERTY()
	TWeakObjectPtr<AOrionChara> InfoChara = nullptr;
	FString PreviousInfoChara = "";

	/* OrionActor Hovering Info Panel */
	TArray<FString> InfoAtMouseCache;
	bool IsShowActorInfo = false;
	void ShowInfoAtMouse(const TArray<FString>& InLines);


	/* Building Menu */

	FOnBuildingOptionSelected OnBuildingOptionSelected;
	FOnToggleDemolishMode OnToggleDemolishMode;


	void ShowBuildingMenu();
	void HideBuildingMenu();
};
