#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OrionChara.h"
#include <vector>
#include "OrionHUD.generated.h"

UCLASS()
class ORION_API AOrionHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

    void BeginPlay();

    void Tick(float DeltaTime) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> WB_DeveloperUIBase;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> WB_PlayerOperationMenu;
    void ShowPlayerOperationMenu(
        float MouseX,
        float MouseY,
        const FHitResult& HitResult = FHitResult(),  // 一个空的HitResult作为默认
        const std::vector<std::string>& ArrOptionNames = {},
        AOrionChara* InTarget = nullptr,
		AOrionActor* InTargetActor = nullptr
    );

    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    //TSubclassOf<UUserWidget> WB_CharaSelectionMenu;
    //UUserWidget* ActiveCharaSelectionMenu = nullptr;
    //void ListenChangeCharaSelection();
    //static std::vector<AOrionChara*> PreviousCharaSelection;

    TArray<FString> InfoLines;
    bool bShowActorInfo;
    void ShowInfoAtMouse(const TArray<FString>& InLines);

	//UFUNCTION(BlueprintCallable, Category = "UI Command")
 //   void PlayerOperationOptionDispatcher(const FString& InOptionName);

};
