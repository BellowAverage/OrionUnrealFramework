#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Misc/Guid.h"
#include "Orion/OrionCppFunctionLibrary/OrionCppFunctionLibrary.h"
#include "OrionActionComponent.generated.h"

class AOrionChara;

UENUM(BlueprintType)
enum class EOrionAction : uint8
{
	AttackOnChara UMETA(DisplayName = "Attack On Character"),
	InteractWithActor UMETA(DisplayName = "Interact With Actor"),
	InteractWithProduction UMETA(DisplayName = "Interact With Production"),
	InteractWithStorage UMETA(DisplayName = "Interact With Storage"),
	MoveToLocation UMETA(DisplayName = "Move To Location"),
	CollectCargo UMETA(DisplayName = "Collect Cargo"),
	CollectBullets UMETA(DisplayName = "Collect Bullets"),
	Undefined UMETA(DisplayName = "Undefined"),
};

USTRUCT()
struct FOrionActionParams
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	EOrionAction OrionActionType = EOrionAction::Undefined;

	UPROPERTY(SaveGame)
	FVector TargetLocation = FVector::ZeroVector;
	UPROPERTY(SaveGame)
	FGuid TargetActorId;
	UPROPERTY(SaveGame)
	FVector HitOffset = FVector::ZeroVector;
	UPROPERTY(SaveGame)
	int32 Quantity = 0;


	friend bool operator==(const FOrionActionParams& A,
		const FOrionActionParams& B)
	{
		return A.OrionActionType == B.OrionActionType &&
			A.TargetLocation.Equals(B.TargetLocation, 1e-4f) &&
			A.TargetActorId == B.TargetActorId &&
			A.HitOffset.Equals(B.HitOffset, 1e-4f) &&
			A.Quantity == B.Quantity;
	}

	friend bool operator!=(const FOrionActionParams& A,
		const FOrionActionParams& B)
	{
		return !(A == B);
	}
};

// --- [New] State Enum Definition ---
UENUM(BlueprintType)
enum class EActionStatus : uint8
{
	Running     UMETA(DisplayName = "Running"),     // Executing (blocks lower priority tasks)
	Finished    UMETA(DisplayName = "Finished"),    // Execution finished (request removal from queue)
	Skipped     UMETA(DisplayName = "Skipped")      // Condition not met (keep in queue, but yield to next task)
};

// --- [New] Action Validity Status (Fix 9) ---
UENUM(BlueprintType)
enum class EActionValidity : uint8
{
	Valid              UMETA(DisplayName = "Valid"),              // Valid, can execute
	TemporarySkip      UMETA(DisplayName = "Temporary Skip"),      // Temporarily condition not met (keep in queue, skip)
	PermanentInvalid   UMETA(DisplayName = "Permanent Invalid")    // Permanently invalid (remove from queue)
};

class FOrionAction
{
public:
	FString Name;

	// [New] Unique ID to prevent pointer invalidation from array reallocation (Fix 4)
	FGuid ActionID;

	EOrionAction OrionActionType = EOrionAction::Undefined;

	FOrionActionParams Params;

	// --- [Refactor] Return value changed to EActionStatus ---
	TFunction<EActionStatus(float)> ExecuteFunction;

	// [New] Modified check function signature, returns detailed status (Fix 9)
	TFunction<EActionValidity(FString&)> ValidityCheckFunction;
	TFunction<FString()> GetDescriptionFunction;
	
	// [Fix] Add OnExit callback for cleanup when action is removed/interrupted
	TFunction<void(bool bInterrupted)> OnExitFunction;

	// [New] Track if action has started (for OnEnter/OnExit)
	bool bHasStarted = false;

	// Updated Constructor
	FOrionAction(const FString& ActionName, EOrionAction InActionType, 
				 TFunction<EActionStatus(float)> Func,
				 TFunction<EActionValidity(FString&)> CheckFunc = nullptr,
				 TFunction<FString()> DescFunc = nullptr,
				 TFunction<void(bool)> ExitFunc = nullptr)
		: Name(ActionName)
		  , ActionID(FGuid::NewGuid()) // Auto generate ID
		  , OrionActionType(InActionType)
		  , ExecuteFunction(MoveTemp(Func))
		  , ValidityCheckFunction(MoveTemp(CheckFunc))
		  , GetDescriptionFunction(MoveTemp(DescFunc))
		  , OnExitFunction(MoveTemp(ExitFunc))
	{
	}

	// Default constructor needed
	FOrionAction() : ActionID(FGuid::NewGuid()) {}

	FString GetDescription() const
	{
		if (GetDescriptionFunction)
		{
			return GetDescriptionFunction();
		}
		return FString();
	}

	// [New] Helper wrapper for validity check
	EActionValidity CheckValidity(FString& OutReason) const
	{
		if (ValidityCheckFunction)
		{
			return ValidityCheckFunction(OutReason);
		}
		return EActionValidity::Valid;
	}

	EOrionAction GetActionType() const
	{
		return OrionActionType;
	}

	// Overload == operator for ID comparison
	bool operator==(const FOrionAction& Other) const { return ActionID == Other.ActionID; }
};

class FActionQueue
{
public:
	TObservableArray<FOrionAction> Actions;

	FORCEINLINE FOrionAction* GetFrontAction() { return Actions.IsEmpty() ? nullptr : &Actions[0]; }

	FORCEINLINE void PopFrontAction()
	{
		if (!Actions.IsEmpty())
		{
			Actions.RemoveAt(0, 1, EAllowShrinking::Yes);
		}
	}
};

// --------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionStateChanged, EOrionAction, PrevType, EOrionAction, CurrType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionNameChanged, FString, PrevName, FString, CurrName);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ORION_API UOrionActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UOrionActionComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/* Core queues */
	FActionQueue RealTimeActionQueue;
	FActionQueue ProceduralActionQueue;

	// [Refactor] No longer store raw pointers, use ID or temporary lookup (Fix 4)
	// Runtime only cache ID for change detection
	FGuid CurrentRealTimeActionID;
	FGuid CurrentProcActionID;

	/* State tracking */
	FString LastActionName;
	EOrionAction LastActionType = EOrionAction::Undefined;

	/* Delegates: for notifying Owner of state changes (replaces original Chara Tick detection logic) */
	UPROPERTY(BlueprintAssignable)
	FOnActionStateChanged OnActionTypeChanged;

	UPROPERTY(BlueprintAssignable)
	FOnActionNameChanged OnActionNameChanged;

	/* External control interface */
	void InsertAction(const FOrionAction& Action, bool bIsProcedural, int32 Index = INDEX_NONE);
	void RemoveAllActions(const FString& Except = FString());

	/* Getters using lookup */
	FOrionAction* GetCurrentAction() const; // Helper to find by ID

	FString GetUnifiedActionName() const;
	EOrionAction GetUnifiedActionType() const;
	bool IsProcedural() const { return bIsProceduralMode; }
	void SetProcedural(bool bEnable) { bIsProceduralMode = bEnable; }

	// Helper: for compatibility with old UI logic, provide validity check by Index
	FString GetActionValidityReason(int32 ActionIndex, bool bIsProcedural);

	// Helper: allow OrionAIControllerInstance and HUD to access queues (reference return)
	FActionQueue& GetRealTimeQueue() { return RealTimeActionQueue; }
	FActionQueue& GetProceduralQueue() { return ProceduralActionQueue; }

	// Helper: for reordering procedural actions
	void ReorderProceduralAction(int32 DraggedIndex, int32 DropIndex);
	void RemoveProceduralActionAt(int32 Index);

	FString GetActionStatusString(int32 ActionIndex, bool bIsProcedural) const;

private:
	/* Internal dispatch logic */
	void DistributeActions(float DeltaTime);
	void DistributeRealTimeAction(float DeltaTime);
	void DistributeProceduralAction(float DeltaTime);
	
	// Helper to handle state transitions
	void SwitchAction(FOrionAction* OldAction, FOrionAction* NewAction, FGuid& TrackedID);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config(Non-null)")
	bool bIsProceduralMode = false;
};
