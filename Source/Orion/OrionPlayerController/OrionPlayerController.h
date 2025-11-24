#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Orion/OrionCppFunctionLibrary/OrionCppFunctionLibrary.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "OrionPlayerController.generated.h"


class UNiagaraSystem;
class AOrionActor;
class AOrionChara;
class AWheeledVehiclePawn;
class AOrionHUD;
class AOrionStructureFoundation;
class AOrionStructureWall;
class AOrionStructure;


UENUM(BlueprintType)
enum class EOrionInputMode : uint8
{
	Default UMETA(DisplayName = "Default"),
	Building UMETA(DisplayName = "Building"),
	CharacterCustomization UMETA(DisplayName = "CharacterCustomization"),
};


UENUM(BlueprintType)
enum class ECommandType : uint8
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	TSubclassOf<AActor> DoubleWallBP = nullptr;

	virtual void SetupInputComponent() override;
	/*void QuickSave();
	void QuickLoad();*/
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;


	/* Cached Cast Pointers */

	UPROPERTY()
	AOrionHUD* OrionHUD = nullptr;

	/* Basic Player Controller Params & Acquisition */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	EOrionInputMode CurrentInputMode = EOrionInputMode::Default;

	FVector WorldOrigin = FVector(), WorldDirection = FVector();
	FHitResult GroundHit = FHitResult();

	float MouseX = 0.f, MouseY = 0.f;

	void TickBasicPlayerControllerParams();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	AActor* HoveringActor = nullptr;

	/* Sole & Identifiable Pointers */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	AActor* StructureSelected = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int32 ViewLevel = 0;

	constexpr static float ViewLevelHeight = 300.0f;

	/* 1. Orion Chara Selection List */
	TObservableArray<AOrionChara*> OrionCharaSelection;
	void OnOrionCharaSelectionChanged(FName OperationName);

	void EmptyOrionCharaSelection(TObservableArray<AOrionChara*>& OrionCharaSelection);

	UFUNCTION(BlueprintCallable, Category = "OrionChara Selection")
	void BPEmptyOrionCharaSelection()
	{
		EmptyOrionCharaSelection(OrionCharaSelection);
	}

	UPROPERTY()
	TArray<AOrionChara*> CachedOrionCharaSelection = {};

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

	FVector2D InitialClickPos = FVector2D();
	FVector2D CurrentMousePos = FVector2D();

	void DetectDraggingControl();

	FTimerHandle DragDetectTimerHandle;

	/* Building */


	FVector GetAutoPlacementLocation(const FVector& GroundImpactPointLocation,
	                                 const UOrionStructureComponent* StructComp) const;
	bool FindPlacementSurface(const FVector& StartLocation, FVector& OutSurfaceLocation,
	                          FVector& OutSurfaceNormal) const;

	void SwitchFromPlacingStructures(int32 InBuildingId, bool IsChecked);

	UPROPERTY(EditDefaultsOnly, Category = "Build")
	float SnapInDist = 200.f;
	UPROPERTY(EditDefaultsOnly, Category = "Build")
	float SnapOutDist = 220.f;

	const float SnapInDistSqr = SnapInDist * SnapInDist;

	void TickBuildingControl();
	void TickPlacingStructure(AActor*& Preview);


	FVector SnappedSocketLoc = FVector();
	FRotator SnappedSocketRot = FRotator();
	FVector SnappedSocketScale = FVector(1.0f, 1.0f, 1.0f);

	// [Dependency] 缓存吸附目标，用于建立依赖关系
	UPROPERTY()
	TWeakObjectPtr<AActor> CachedSnapTarget = nullptr;


	/* Place Structure */

	UPROPERTY()
	UOrionBuildingManager* BuildingManager = nullptr;

	bool IsPlacingStructure = false;
	bool IsStructureSnapped = false;

	/* 0. Demolishing Mode */

	void OnToggleDemolishingMode(bool bIsChecked);
	void SpawnPreviewStructure(AActor*& InPreviewStructure, TSubclassOf<AActor> ClassToSpawn);
	bool IsDemolishingMode = false;
	void DemolishStructureUnderCursor() const;

	/* 1. Place Foundation Structure */


	UPROPERTY()
	AActor* PreviewStructure = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Build")
	TSubclassOf<AActor> SquareFoundationBP;


	UPROPERTY(EditDefaultsOnly, Category = "Build")
	TSubclassOf<AActor> TriangleFoundationBP;

	/* 2. Place Wall Structure */

	UPROPERTY(EditAnywhere, Category = "Build")
	TSubclassOf<AActor> WallBP;


	/* Developer & Debug */

	void TickDrawOrionActorStatus() const;

	void OnKey4Pressed();
	void OnKey5Pressed();
	void OnKey6Pressed();
	void OnKey7Pressed();
	void OnKey8Pressed();


	/* Niagara Interaction Effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara")
	UNiagaraSystem* NiagaraHitResultEffect = nullptr;


	/* Clicked On OrionActor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clicked On OrionActor")
	AOrionActor* ClickedOnOrionActor = nullptr;


	/* Input Interaction Functions*/


	void ConfirmPlaceStructure(AActor*& PreviewPtr);
	void OnLeftMouseDown();
	void OnLeftMouseUp();
	void SingleSelectionUnderCursor();
	void BoxSelectionUnderCursor(const FVector2D& StartPos, const FVector2D& EndPos);
	void OnRightMouseDown();

	float RightMouseDownTime = 0.0f;
	UPROPERTY(EditAnywhere, Category="Input")
	float RightClickHoldThreshold = 0.2f;

	UPROPERTY()
	AOrionActor* CachedRightClickedOrionActor = nullptr;
	void OnRightMouseUp();
	void OnShiftPressed();
	void OnShiftReleased();
	bool bIsShiftPressed = false;

	/* Server Request */

	UPROPERTY()
	TArray<AOrionChara*> CachedActionSubjects;

	UPROPERTY()
	AOrionActor* CachedActionObjects = nullptr;

	TArray<FString> CachedRequestCaseNames;

	UFUNCTION(BlueprintCallable, Category = "Server Action Request")
	void RequestAttackOnOrionActor(FVector HitOffset, ECommandType inCommandType);


	UFUNCTION(BlueprintCallable, Category = "Server Action Request")
	void CallBackRequestDistributor(FName CallBackRequest);

	UFUNCTION(BlueprintCallable, Category = "Basics")
	void OnBPressed();
};
