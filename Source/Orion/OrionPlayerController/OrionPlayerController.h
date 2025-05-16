#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraFunctionLibrary.h"
#include "Orion/OrionStructure/OrionStructure.h"
#include "Orion/OrionChara/OrionChara.h"
#include "WheeledVehiclePawn.h"
#include "Orion/OrionStructure/OrionStructureFoundation.h"
#include "Orion/OrionStructure/OrionStructureWall.h"

#include "Orion/OrionCppFunctionLibrary/OrionCppFunctionLibrary.h"

#include "OrionPlayerController.generated.h"


class AOrionHUD;
//DECLARE_DELEGATE_OneParam(FOnOrionActorSelectionChanged, AOrionActor*);
DECLARE_DELEGATE_OneParam(FOnToggleBuildingMode, bool);

UENUM(BlueprintType)
enum class EOrionInputMode : uint8
{
	Default UMETA(DisplayName = "Default"),
	Building UMETA(DisplayName = "Building"),
	CharacterCustomization UMETA(DisplayName = "CharacterCustomization"),
};


UENUM(BlueprintType)
enum class CommandType : uint8
{
	Force UMETA(DisplayName = "Clear ActionQueue and Add"),
	Append UMETA(DisplayName = "Push Action into ActionQueue"),
};

UCLASS()
class ORION_API AOrionPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AOrionPlayerController();
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;


	/* Delegation */

	//FOnOrionActorSelectionChanged OnOrionActorSelectionChanged;
	FOnToggleBuildingMode OnToggleBuildingMode;

	/* Cached Cast Pointers */

	UPROPERTY()
	AOrionHUD* OrionHUD = nullptr;

	/* Basic Player Controller Params & Acquisition */

	EOrionInputMode CurrentInputMode = EOrionInputMode::Default;

	FVector WorldOrigin, WorldDirection;
	FHitResult GroundHit;

	float MouseX, MouseY;

	void UpdateBasicPlayerControllerParams();

	/* Sole & Identifiable Pointers */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	AOrionStructure* StructureSelected = nullptr;

	/* 1. Orion Chara Selection List */
	TObservableArray<AOrionChara*> OrionCharaSelection;
	void OnOrionCharaSelectionChanged(FName OperationName);
	void EmptyOrionCharaSelection(TObservableArray<AOrionChara*>& OrionCharaSelection);

	UFUNCTION(BlueprintCallable, Category = "OrionChara Selection")
	TArray<AOrionChara*> GetOrionCharaSelection() const
	{
		return OrionCharaSelection;
	}

	void SelectAll();

	/* 2. Orion Vehicle Selection List */
	UPROPERTY()
	TArray<AWheeledVehiclePawn*> OrionPawnSelection;


	/* Dragging Or Selecting Detection */

	bool bIsSelecting = false;
	bool bHasDragged = false;

	FVector2D InitialClickPos;
	FVector2D CurrentMousePos;

	void DetectDraggingControl();

	FTimerHandle DragDetectTimerHandle;

	/* Building */

	UPROPERTY(EditDefaultsOnly, Category = "Build")
	float SnapInDist = 150.f; // cm，进入吸附
	UPROPERTY(EditDefaultsOnly, Category = "Build")
	float SnapOutDist = 180.f; // cm，离开吸附

	const float SnapInDistSqr = SnapInDist * SnapInDist;

	void UpdateBuildingControl();


	FVector SnappedSocketLoc;
	FRotator SnappedSocketRot;
	FVector SnappedSocketScale;


	/* Place Structure */

	bool bIsSpawnPreviewStructure = false;

	template <typename TOrionStructure>
	void TogglePlacingStructure(TSubclassOf<TOrionStructure> BPOrionStructure,
	                            TOrionStructure*& PreviewStructurePtr);

	bool bPlacingStructure = false;
	bool bStructureSnapped = false;

	template <typename TOrionStructure>
	void UpdatePlacingStructure(TSubclassOf<TOrionStructure> BPOrionStructure, TOrionStructure*& PreviewStructure);

	template <typename TOrionStructure>
	void ConfirmPlaceStructure(TSubclassOf<TOrionStructure> BPOrionStructure, TOrionStructure*& PreviewStructurePtr);

	/* 0. Demolishing Mode */

	void OnToggleDemolishingMode();
	bool bDemolishingMode = false;
	void DemolishStructureUnderCursor();

	/* 1. Place Foundation Structure */

	void OnTogglePlacingFoundation();

	UPROPERTY()
	AOrionStructureFoundation* PreviewFoundation = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Build")
	TSubclassOf<AOrionStructureFoundation> FoundationBP;

	/* 2. Place Wall Structure */
	void OnTogglePlacingWall();

	UPROPERTY(EditAnywhere, Category = "Build")
	TSubclassOf<AOrionStructureWall> WallBP;

	UPROPERTY()
	AOrionStructureWall* PreviewWall = nullptr;

	/* Developer & Debug */

	void DrawOrionActorStatus();

	void OnKey4Pressed();
	void OnKey5Pressed();
	void OnKey6Pressed();
	// void OnKey7Pressed();


	/* Niagara Interaction Effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara")
	UNiagaraSystem* NiagaraHitResultEffect;


	/* Clicked On OrionActor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clicked On OrionActor")
	AOrionActor* ClickedOnOrionActor = nullptr;


	/* Input Interaction Functions*/


	void OnLeftMouseDown();
	void OnLeftMouseUp();
	void SingleSelectionUnderCursor();
	void BoxSelectionUnderCursor(const FVector2D& StartPos, const FVector2D& EndPos);
	void OnRightMouseDown();

	float RightMouseDownTime = 0.0f;
	UPROPERTY(EditAnywhere, Category="Input")
	float RightClickHoldThreshold = 0.2f;

	AOrionActor* CachedRightClickedOrionActor = nullptr;
	void OnRightMouseUp();
	void OnShiftPressed();
	void OnShiftReleased();
	bool bIsShiftPressed = false;

	/* Server Request */

	TArray<AOrionChara*> CachedActionSubjects;
	AOrionActor* CachedActionObjects;
	TArray<FString> CachedRequestCaseNames;

	UFUNCTION(BlueprintCallable, Category = "Server Action Request")
	void RequestAttackOnOrionActor(FVector HitOffset, CommandType inCommandType);


	UFUNCTION(BlueprintCallable, Category = "Server Action Request")
	void CallBackRequestDistributor(FName CallBackRequest);

	UFUNCTION(BlueprintCallable, Category = "Basics")
	void OnBPressed();
};
