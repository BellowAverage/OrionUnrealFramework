#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include <vector>
#include "OrionChara.h"
#include "OrionPlayerController.generated.h"

UCLASS()
class ORION_API AOrionPlayerController : public APlayerController
{
    GENERATED_BODY()

private:
    void OnLeftMouseClick();

public:
    AOrionPlayerController();

    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /* Niagara Interaction Effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara")
    UNiagaraSystem* NiagaraHitResultEffect;

    /* OrionCharaSelection List */
    TSubclassOf<AOrionChara> SubclassOfAOrionChara;
    std::vector<AOrionChara*> OrionCharaSelection;

    /* OrionCharaSelection List Utilities */
    void SelectAll();

    /* Input Interaction Functions*/
    bool bIsSelecting = false;
    bool bHasDragged = false;
    FVector2D InitialClickPos;
    FVector2D CurrentMousePos;
    void OnLeftMouseDown();
    void OnLeftMouseUp();
    void SingleSelectionUnderCursor();
    void BoxSelectionUnderCursor(const FVector2D& StartPos, const FVector2D& EndPos);
    void OnRightMouseDown();
    float MouseX, MouseY;

    float RightMouseDownTime = 0.0f;
    UPROPERTY(EditAnywhere, Category="Input")
	float RightClickHoldThreshold = 0.5f;

	AOrionActor* CachedRightClickedOrionActor = nullptr;
    void OnRightMouseUp();

};
