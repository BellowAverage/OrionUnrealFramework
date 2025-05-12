#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraFunctionLibrary.h"
//#include "NiagaraSystem.h"
#include "Orion/OrionStructure/OrionStructure.h"
#include "Orion/OrionChara/OrionChara.h"
#include "WheeledVehiclePawn.h"
#include "Orion/OrionStructure/OrionStructureFoundation.h"
#include "OrionPlayerController.generated.h"


DECLARE_DELEGATE_OneParam(FOnOrionActorSelectionChanged, AOrionActor*);
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
	FOnOrionActorSelectionChanged OnOrionActorSelectionChanged;
	FOnToggleBuildingMode OnToggleBuildingMode;

	void OnKey4Pressed();

	UPROPERTY(EditDefaultsOnly, Category = "Build")
	float SnapInDist = 120.f; // cm，进入吸附
	UPROPERTY(EditDefaultsOnly, Category = "Build")
	float SnapOutDist = 160.f; // cm，离开吸附

	bool bSnapped = false;
	int32 SnappedSocketIdx = INDEX_NONE;
	AOrionStructureFoundation* SnappedTarget = nullptr;


	// 放到 public / protected 任意合适位置
	UPROPERTY(EditDefaultsOnly, Category = "Build")
	TSubclassOf<AOrionStructureFoundation> FoundationBP; // 指向地基蓝图

	// 运行时状态
	bool bPlacingFoundation = false;
	AOrionStructureFoundation* PreviewFoundation = nullptr;

	void OnConfirmPlace();

	void OnKey5Pressed();
	//void OnKey6Pressed();
	//void OnKey7Pressed();


	EOrionInputMode CurrentInputMode = EOrionInputMode::Default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	AOrionStructure* StructureSelected = nullptr;

	AOrionPlayerController();

	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/* Niagara Interaction Effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara")
	UNiagaraSystem* NiagaraHitResultEffect;


	/* Clicked On OrionActor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clicked On OrionActor")
	AOrionActor* ClickedOnOrionActor = nullptr;

	/* OrionCharaSelection List */
	TSubclassOf<AOrionChara> SubclassOfAOrionChara;
	std::vector<AOrionChara*> OrionCharaSelection;

	UFUNCTION(BlueprintCallable, Category = "OrionChara Selection")
	TArray<AOrionChara*> GetOrionCharaSelection() const
	{
		return TArray<AOrionChara*>(OrionCharaSelection.data(), OrionCharaSelection.size());
	}

	std::vector<AWheeledVehiclePawn*> OrionPawnSelection;

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
	float RightClickHoldThreshold = 0.2f;

	AOrionActor* CachedRightClickedOrionActor = nullptr;
	void OnRightMouseUp();
	void OnShiftPressed();
	void OnShiftReleased();
	bool bIsShiftPressed = false;

	/* Server Request */

	std::vector<AOrionChara*> CachedActionSubjects;
	std::vector<AOrionActor*> CachedActionObjects;
	std::vector<FString> CachedRequestCaseNames;

	UFUNCTION(BlueprintCallable, Category = "Server Action Request")
	void RequestAttackOnOrionActor(FVector HitOffset, CommandType inCommandType);


	UFUNCTION(BlueprintCallable, Category = "Server Action Request")
	void CallBackRequestDistributor(FName CallBackRequest);

	UFUNCTION(BlueprintCallable, Category = "Basics")
	void OnBPressed();
};
