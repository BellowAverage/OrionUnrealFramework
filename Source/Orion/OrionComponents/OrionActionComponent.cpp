#include "OrionActionComponent.h"
#include "Orion/OrionChara/OrionChara.h"

UOrionActionComponent::UOrionActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOrionActionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UOrionActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 1. Record previous frame state
	const FString PrevName = LastActionName;
	const EOrionAction PrevType = LastActionType;

	// 2. Execute dispatch logic
	DistributeActions(DeltaTime);

	// 3. Get current state
	const FString CurrName = GetUnifiedActionName();
	const EOrionAction CurrType = GetUnifiedActionType();

	// 4. Detect changes and broadcast (Owner will bind these delegates to handle SwitchingStateHandle)
	if (PrevType != CurrType)
	{
		OnActionTypeChanged.Broadcast(PrevType, CurrType);
	}
	if (PrevName != CurrName)
	{
		OnActionNameChanged.Broadcast(PrevName, CurrName);
	}

	// 5. Update cache
	LastActionName = CurrName;
	LastActionType = CurrType;
}

// Helper: Find action via ID in a queue
static FOrionAction* FindActionByID(FActionQueue& Queue, const FGuid& ID)
{
	if (!ID.IsValid()) return nullptr;
	for (FOrionAction& Action : Queue.Actions)
	{
		if (Action.ActionID == ID) return &Action;
	}
	return nullptr;
}

void UOrionActionComponent::DistributeActions(float DeltaTime)
{
	if (bIsProceduralMode)
	{
		DistributeProceduralAction(DeltaTime);
	}
	else
	{
		DistributeRealTimeAction(DeltaTime);
	}
}

void UOrionActionComponent::SwitchAction(FOrionAction* OldAction, FOrionAction* NewAction, FGuid& TrackedID)
{
	if (OldAction != NewAction)
	{
		if (OldAction)
		{
			if (OldAction->OnExitFunction && OldAction->bHasStarted) 
			{
				OldAction->OnExitFunction(true); // Interrupted
			}
			OldAction->bHasStarted = false;
		}

		if (NewAction)
		{
			TrackedID = NewAction->ActionID;
			if (!NewAction->bHasStarted)
			{
				NewAction->bHasStarted = true;
			}
		}
		else
		{
			TrackedID.Invalidate();
		}
	}
}

void UOrionActionComponent::DistributeRealTimeAction(float DeltaTime)
{
	FOrionAction* CurrentPtr = nullptr;
	// Find previous action by ID to ensure we call Exit on correct instance
	FOrionAction* PreviousPtr = FindActionByID(RealTimeActionQueue, CurrentRealTimeActionID);

	if (!RealTimeActionQueue.Actions.IsEmpty())
	{
		CurrentPtr = RealTimeActionQueue.GetFrontAction();
	}

	// --- [Fix] Validity Check for RealTime Actions (same as Procedural) ---
	if (CurrentPtr)
	{
		FString Reason;
		EActionValidity Validity = CurrentPtr->CheckValidity(Reason);
		
		if (Validity == EActionValidity::PermanentInvalid)
		{
			// Permanently invalid -> Remove immediately
			if (CurrentPtr->OnExitFunction && CurrentPtr->bHasStarted) 
			{
				CurrentPtr->OnExitFunction(true); // Interrupted
			}
			CurrentPtr->bHasStarted = false;
			RealTimeActionQueue.PopFrontAction();
			CurrentRealTimeActionID.Invalidate();
			return; // Exit early, action removed
		}
		else if (Validity == EActionValidity::TemporarySkip)
		{
			// Skip this frame, but keep action in queue
			return;
		}
		// Valid -> Continue to execution
	}
	// ------------------------------------------------------------------------

	// Handle Switch (Enter/Exit)
	if (CurrentPtr != PreviousPtr)
	{
		SwitchAction(PreviousPtr, CurrentPtr, CurrentRealTimeActionID);
	}

	if (CurrentPtr)
	{
		EActionStatus Status = CurrentPtr->ExecuteFunction(DeltaTime);

		if (Status == EActionStatus::Finished || Status == EActionStatus::Skipped)
		{
			if (CurrentPtr->OnExitFunction) 
			{
				CurrentPtr->OnExitFunction(false); // Normal exit
			}
			CurrentPtr->bHasStarted = false;
			RealTimeActionQueue.PopFrontAction();
			CurrentRealTimeActionID.Invalidate();
		}
	}
}

void UOrionActionComponent::DistributeProceduralAction(float DeltaTime)
{
	FOrionAction* PreviousPtr = FindActionByID(ProceduralActionQueue, CurrentProcActionID);
	FOrionAction* NextActivePtr = nullptr;

	for (int32 i = 0; i < ProceduralActionQueue.Actions.Num(); /* i++ inside */)
	{
		FOrionAction& Action = ProceduralActionQueue.Actions[i];

		// --- [Fix 9] Validity Check with Remove Support ---
		FString Reason;
		EActionValidity Validity = Action.CheckValidity(Reason);
		
		if (Validity == EActionValidity::PermanentInvalid)
		{
			// Permanently invalid -> Remove
			// If this action is currently running, SwitchAction will handle its Exit later
			if (Action.ActionID == CurrentProcActionID)
			{
				// Special case: If current action is being removed, need to immediately trigger Exit to prevent logic residue
				if (Action.OnExitFunction && Action.bHasStarted) 
				{
					Action.OnExitFunction(true);
				}
				CurrentProcActionID.Invalidate();
				PreviousPtr = nullptr; // Reset ptr as memory is gone
			}
			
			ProceduralActionQueue.Actions.RemoveAt(i);
			continue; // Check new action at same index
		}
		else if (Validity == EActionValidity::TemporarySkip)
		{
			i++; // Skip
			continue;
		}
		// ------------------------------------------------

		// Logic: If we switch focus, trigger transitions BEFORE Execute
		// Command pattern usually implies Init -> Update -> Exit.
		// If we are newly focusing on this action, we should Switch first.
		if (CurrentProcActionID != Action.ActionID)
		{
			SwitchAction(PreviousPtr, &Action, CurrentProcActionID);
			PreviousPtr = &Action; // Update local tracker
		}

		EActionStatus Status = Action.ExecuteFunction(DeltaTime);

		if (Status == EActionStatus::Finished)
		{
			if (Action.OnExitFunction)
			{
				Action.OnExitFunction(false);
			}
			Action.bHasStarted = false;
			
			// Reset ID before removing to avoid stale ID
			if (CurrentProcActionID == Action.ActionID) 
			{
				CurrentProcActionID.Invalidate();
			}
			PreviousPtr = nullptr;

			ProceduralActionQueue.Actions.RemoveAt(i);
			continue;
		}
		else if (Status == EActionStatus::Skipped)
		{
			i++;
			continue;
		}
		else // Running
		{
			NextActivePtr = &Action;
			break;
		}
	}

	// Handle case where queue is empty or all skipped (NextActivePtr is null, but we might have had a Previous)
	if (NextActivePtr == nullptr && PreviousPtr != nullptr)
	{
		SwitchAction(PreviousPtr, nullptr, CurrentProcActionID);
	}
}

void UOrionActionComponent::InsertAction(const FOrionAction& Action, bool bIsProcedural, int32 Index)
{
	auto& TargetQueue = bIsProcedural ? ProceduralActionQueue.Actions : RealTimeActionQueue.Actions;
	if (Index == INDEX_NONE || Index < 0 || Index > TargetQueue.Num())
	{
		TargetQueue.Add(Action);
	}
	else
	{
		TargetQueue.Insert(Action, Index);
	}
}

void UOrionActionComponent::RemoveAllActions(const FString& Except)
{
	// Force exit current
	FOrionAction* CurrRT = FindActionByID(RealTimeActionQueue, CurrentRealTimeActionID);
	if (CurrRT) 
	{
		if (CurrRT->OnExitFunction) 
		{
			CurrRT->OnExitFunction(true);
		}
		CurrRT->bHasStarted = false; 
	}

	if (Except.IsEmpty())
	{
		RealTimeActionQueue.Actions.Empty();
		CurrentRealTimeActionID.Invalidate();
	}
	else
	{
		// Remove all except the specified action
		int32 BeforeNum = RealTimeActionQueue.Actions.Num();
		RealTimeActionQueue.Actions.GetRaw().RemoveAll([&Except](const FOrionAction& Action) { return Action.Name != Except; });
		if (RealTimeActionQueue.Actions.Num() != BeforeNum)
		{
			RealTimeActionQueue.Actions.OnArrayChanged.Broadcast(TEXT("RemoveAll"));
		}
		// Re-find current action after removal
		if (!RealTimeActionQueue.Actions.IsEmpty())
		{
			CurrentRealTimeActionID = RealTimeActionQueue.Actions[0].ActionID;
		}
		else
		{
			CurrentRealTimeActionID.Invalidate();
		}
	}
	
	FOrionAction* CurrProc = FindActionByID(ProceduralActionQueue, CurrentProcActionID);
	if (CurrProc) 
	{ 
		if (CurrProc->OnExitFunction) 
		{
			CurrProc->OnExitFunction(true);
		}
		CurrProc->bHasStarted = false; 
	}
	
	if (Except.IsEmpty())
	{
		ProceduralActionQueue.Actions.Empty();
		CurrentProcActionID.Invalidate();
	}
	else
	{
		int32 BeforeNum = ProceduralActionQueue.Actions.Num();
		ProceduralActionQueue.Actions.GetRaw().RemoveAll([&Except](const FOrionAction& Action) { return Action.Name != Except; });
		if (ProceduralActionQueue.Actions.Num() != BeforeNum)
		{
			ProceduralActionQueue.Actions.OnArrayChanged.Broadcast(TEXT("RemoveAll"));
		}
		// CurrentProcActionID remains valid if the action with Except name still exists
		// Otherwise it will be invalidated naturally
	}
}

FOrionAction* UOrionActionComponent::GetCurrentAction() const
{
	// Return pointer for UI/Info. Warning: Unsafe if stored.
	if (bIsProceduralMode)
	{
		// Const cast to allow finding in const function
		auto& Queue = const_cast<UOrionActionComponent*>(this)->ProceduralActionQueue; 
		for (auto& Act : Queue.Actions) 
		{
			if (Act.ActionID == CurrentProcActionID) 
			{
				return &Act;
			}
		}
	}
	else
	{
		auto& Queue = const_cast<UOrionActionComponent*>(this)->RealTimeActionQueue;
		if (!Queue.Actions.IsEmpty()) 
		{
			return &Queue.Actions[0];
		}
	}
	return nullptr;
}

FString UOrionActionComponent::GetUnifiedActionName() const
{
	FOrionAction* Act = GetCurrentAction();
	return Act ? Act->Name : TEXT("");
}

EOrionAction UOrionActionComponent::GetUnifiedActionType() const
{
	FOrionAction* Act = GetCurrentAction();
	return Act ? Act->GetActionType() : EOrionAction::Undefined;
}

FString UOrionActionComponent::GetActionValidityReason(int32 ActionIndex, bool bIsProcedural)
{
	auto& TargetQueue = bIsProcedural ? ProceduralActionQueue.Actions : RealTimeActionQueue.Actions;
	if (!TargetQueue.GetRaw().IsValidIndex(ActionIndex))
	{
		return TEXT("Invalid Action Index");
	}
	
	FString Reason;
	TargetQueue[ActionIndex].CheckValidity(Reason);
	return Reason;
}

void UOrionActionComponent::ReorderProceduralAction(int32 DraggedIndex, int32 DropIndex)
{
	auto& CharaProcQueueActionsRef = ProceduralActionQueue.Actions;

	int32 Count = CharaProcQueueActionsRef.Num();
	if (DraggedIndex < 0 || DraggedIndex >= Count)
	{
		return;
	}

	FOrionAction ActionRef = MoveTemp(CharaProcQueueActionsRef[DraggedIndex]);
	CharaProcQueueActionsRef.RemoveAt(DraggedIndex);

	int32 NewIndex = DropIndex;
	if (DropIndex > DraggedIndex)
	{
		NewIndex = DropIndex - 1;
	}

	NewIndex = FMath::Clamp(NewIndex, 0, CharaProcQueueActionsRef.Num());

	CharaProcQueueActionsRef.Insert(MoveTemp(ActionRef), NewIndex);
}

void UOrionActionComponent::RemoveProceduralActionAt(int32 Index)
{
	// [Fix] Handle active removal
	// Use GetRaw() to access internal TArray for IsValidIndex
	if (ProceduralActionQueue.Actions.GetRaw().IsValidIndex(Index))
	{
		FOrionAction& Act = ProceduralActionQueue.Actions[Index];
		if (Act.ActionID == CurrentProcActionID)
		{
			if (Act.OnExitFunction) 
		{
				Act.OnExitFunction(true);
			}
			CurrentProcActionID.Invalidate();
		}
		ProceduralActionQueue.Actions.RemoveAt(Index);
	}
}

FString UOrionActionComponent::GetActionStatusString(int32 ActionIndex, bool bIsProcedural) const
{
	// Only relevant for Procedural queue usually, but handling both for completeness
	const auto& Queue = bIsProcedural ? ProceduralActionQueue.Actions : RealTimeActionQueue.Actions;

	if (!Queue.GetRaw().IsValidIndex(ActionIndex))
	{
		return TEXT("Invalid Index");
	}

	// Check if executing
	const FOrionAction& Action = Queue[ActionIndex];
	FOrionAction* RunningPtr = const_cast<UOrionActionComponent*>(this)->GetCurrentAction();

	if (RunningPtr == &Action)
	{
		FString Desc = Action.GetDescription();
		if (!Desc.IsEmpty())
		{
			return FString::Printf(TEXT("Executing\n%s"), *Desc);
		}
		return TEXT("Executing");
	}

	// Check if blocked by higher priority (only meaningful in Procedural mode)
	if (bIsProcedural && RunningPtr)
	{
		// Find the index of current running action by ID
		for (int32 i = 0; i < Queue.GetRaw().Num(); ++i)
		{
			if (Queue[i].ActionID == CurrentProcActionID)
			{
				if (ActionIndex > i)
		{
			return TEXT("Blocked by Higher Priority Task");
		}
				break;
			}
		}
	}

	// If not running and we are here, it means it was checked but condition not met (Skipped).
	FString Reason;
	EActionValidity Validity = Action.CheckValidity(Reason);
	if (Validity == EActionValidity::Valid)
	{
		return TEXT("Waiting / Ready"); // Should run if priority allows
	}
	
	return FString::Printf(TEXT("Condition Not Met: %s"), *Reason);
}
