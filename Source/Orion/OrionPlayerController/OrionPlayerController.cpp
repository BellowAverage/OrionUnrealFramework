#include "OrionPlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include <Kismet/GameplayStatics.h>
#include <algorithm>
#include "Orion/OrionActor/OrionActorOre.h"
#include "Orion/OrionInterface/OrionInterfaceSelectable.h"
#include "Orion/OrionAIController/OrionAIController.h"
#include "Orion/OrionHUD/OrionHUD.h"
#include "Orion/OrionActor/OrionActor.h"
#include "Orion/OrionGameMode/OrionGameMode.h"
#include "Orion/OrionComponents/OrionStructureComponent.h"

// forward declarz in .h
#include "Orion/OrionGameInstance/OrionGameInstance.h"
#include "Orion/OrionStructure/OrionStructureFoundation.h"


AOrionPlayerController::AOrionPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AOrionPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindAction("LeftMouseClick", IE_Pressed, this, &AOrionPlayerController::OnLeftMouseDown);
		InputComponent->BindAction("LeftMouseClick", IE_Released, this, &AOrionPlayerController::OnLeftMouseUp);
		InputComponent->BindAction("RightMouseClick", IE_Pressed, this, &AOrionPlayerController::OnRightMouseDown);
		InputComponent->BindAction("CtrlA", IE_Pressed, this, &AOrionPlayerController::SelectAll);

		InputComponent->BindAction("BPressed", IE_Pressed, this, &AOrionPlayerController::OnBPressed);

		InputComponent->BindAction("Key4Pressed", IE_Pressed, this, &AOrionPlayerController::OnKey4Pressed);
		InputComponent->BindAction("Key5Pressed", IE_Pressed, this, &AOrionPlayerController::OnKey5Pressed);
		InputComponent->BindAction("Key6Pressed", IE_Pressed, this, &AOrionPlayerController::OnKey6Pressed);
		InputComponent->BindAction("Key7Pressed", IE_Pressed, this, &AOrionPlayerController::OnKey7Pressed);

		InputComponent->BindAction("QuickSave", IE_Pressed, this, &AOrionPlayerController::QuickSave);
		InputComponent->BindAction("QuickLoad", IE_Pressed, this, &AOrionPlayerController::QuickLoad);

		InputComponent->BindAction("RightMouseClick", IE_Released, this, &AOrionPlayerController::OnRightMouseUp);
		InputComponent->BindAction("ShiftPress", IE_Pressed, this, &AOrionPlayerController::OnShiftPressed);
		InputComponent->BindAction("ShiftPress", IE_Released, this, &AOrionPlayerController::OnShiftReleased);
	}
}

void AOrionPlayerController::QuickSave()
{
	if (auto* GI = GetGameInstance<UOrionGameInstance>())
	{
		GI->SaveGame();
	}
}

void AOrionPlayerController::QuickLoad()
{
	if (auto* GI = GetGameInstance<UOrionGameInstance>())
	{
		GI->LoadGame();
	}
}

void AOrionPlayerController::BeginPlay()
{
	Super::BeginPlay();

	/* Setup Input Mode */

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	/* Bind Custom Events */

	OrionCharaSelection.OnArrayChanged.AddUObject(this, &AOrionPlayerController::OnOrionCharaSelectionChanged);

	/* Init Variables */

	OrionHUD = Cast<AOrionHUD>(GetHUD());

	if (OrionHUD == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Cast to OrionHUD failed!"));
	}

	if (BuildingManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOrionBuildingManager>() : nullptr;
		!BuildingManager)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get OrionBuildingManager subsystem!"));
	}

	if (OrionHUD)
	{
		OrionHUD->OnViewLevelUp.BindLambda([this]()
		{
			++ViewLevel;
			UE_LOG(LogTemp, Log, TEXT("View Level Up"));
		});

		OrionHUD->OnViewLevelDown.BindLambda([this]()
		{
			if (ViewLevel > 0)
			{
				--ViewLevel;
				UE_LOG(LogTemp, Log, TEXT("View Level Down"));
			}
		});
	}
}

void AOrionPlayerController::OnKey4Pressed()
{
	UE_LOG(LogTemp, Log, TEXT("Key 4 Pressed"));

	BuildBP = SquareFoundationBP;

	TogglePlacingStructure(BuildBP, PreviewStructure);
}

void AOrionPlayerController::OnKey7Pressed()
{
	UE_LOG(LogTemp, Log, TEXT("Key 7 Pressed"));

	BuildBP = TriangleFoundationBP;

	TogglePlacingStructure(BuildBP, PreviewStructure);
}

void AOrionPlayerController::OnKey5Pressed()
{
	UE_LOG(LogTemp, Log, TEXT("Key 5 Pressed"));

	BuildBP = WallBP;

	TogglePlacingStructure(BuildBP, PreviewStructure);
}

void AOrionPlayerController::OnKey6Pressed()
{
	UE_LOG(LogTemp, Log, TEXT("Key 6 Pressed"));
	if (CurrentInputMode == EOrionInputMode::Building)
	{
		OnToggleDemolishingMode();
	}
}

void AOrionPlayerController::OnBPressed()
{
	UE_LOG(LogTemp, Log, TEXT("B Pressed"));

	if (CurrentInputMode != EOrionInputMode::Building)
	{
		CachedOrionCharaSelectionInBuilding = OrionCharaSelection;

		EmptyOrionCharaSelection(OrionCharaSelection);

		CurrentInputMode = EOrionInputMode::Building;
		OnToggleBuildingMode.ExecuteIfBound(true);
		if (StructureSelected)
		{
			StructureSelected = nullptr;
		}
	}
	else
	{
		if (CachedOrionCharaSelectionInBuilding.Num() > 0)
		{
			for (auto& EachChara : CachedOrionCharaSelectionInBuilding)
			{
				OrionCharaSelection.Add(EachChara);
			}

			CachedOrionCharaSelectionInBuilding.Empty();
		}

		CurrentInputMode = EOrionInputMode::Default;
		OnToggleBuildingMode.ExecuteIfBound(false);
	}
}

void AOrionPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateBasicPlayerControllerParams();

	DrawOrionActorStatus();

	UpdateBuildingControl();
}

void AOrionPlayerController::OnOrionCharaSelectionChanged(FName OperationName)
{
	UE_LOG(LogTemp, Log, TEXT("Orion Chara Selection Changed: %s"), *OperationName.ToString());

	if (OrionCharaSelection.Num() == 1)
	{
		OrionHUD->bShowCharaInfoPanel = true;
		OrionHUD->InfoChara = OrionCharaSelection[0];
	}
	else
	{
		OrionHUD->bShowCharaInfoPanel = false;
		OrionHUD->InfoChara = nullptr;
	}
}

void AOrionPlayerController::UpdateBasicPlayerControllerParams()
{
	if (GetMousePosition(MouseX, MouseY))
	{
		CurrentMousePos = FVector2D(MouseX, MouseY);
	}
	else
	{
		//UE_LOG(LogTemp, Warning, TEXT("GetMousePosition failed."));
	}

	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		//UE_LOG(LogTemp, Warning, TEXT("DeprojectMousePositionToWorld failed."));
	}

	if (!GetWorld()->LineTraceSingleByChannel(
		GroundHit, WorldOrigin, WorldOrigin + WorldDirection * 100000.f, ECC_GameTraceChannel1))
	{
		//UE_LOG(LogTemp, Warning, TEXT("LineTraceSingleByChannel failed."));
		return;
	}


	const float GroundZOffset = ViewLevel * ViewLevelHeight;

	bool bGotPlaneHit = false;
	FVector HitLocation = FVector::ZeroVector;

	{
		if (const float Denom = WorldDirection.Z; !FMath::IsNearlyZero(Denom)) // 射线不平行于水平面
		{
			if (const float T = (GroundZOffset - WorldOrigin.Z) / Denom; T > 0.f) // 只要朝前方
			{
				bGotPlaneHit = true;
				HitLocation = WorldOrigin + WorldDirection * T;
			}
		}
	}

	/* ───────────────────── 4. 回退：真实地形 Trace ───────────────────── */
	if (!bGotPlaneHit)
	{
		if (GetWorld()->LineTraceSingleByChannel(
			GroundHit,
			WorldOrigin,
			WorldOrigin + WorldDirection * 100000.f,
			ECC_GameTraceChannel1))
		{
			HitLocation = GroundHit.ImpactPoint;
		}
		else
		{
			// 完全无命中 → 给个远点，或保持上一次结果
			return;
		}
	}

	/* ───────────────────── 5. 最终写回 GroundHit ─────────────────────
	   只用 Location / ImpactPoint 即可，其他字段按需填写
	*/
	GroundHit.Location = HitLocation;
	GroundHit.ImpactPoint = HitLocation;
	GroundHit.bBlockingHit = true;
	GroundHit.Distance = (HitLocation - WorldOrigin).Size();
}

void AOrionPlayerController::DetectDraggingControl()
{
	CurrentMousePos = FVector2D(MouseX, MouseY);

	// Selecting yet not determined as dragging.
	if (bIsSelecting && !bHasDragged)
	{
		constexpr float DragThreshold = 5.f;
		if (FVector2D::Distance(CurrentMousePos, InitialClickPos) > DragThreshold)
		{
			bHasDragged = true;
		}
	}
}

void AOrionPlayerController::UpdateBuildingControl()
{
	if (bPlacingStructure && PreviewStructure)
	{
		UpdatePlacingStructure(BuildBP, PreviewStructure);
	}

	if (bPlacingStructure && PreviewStructure)
	{
		UpdatePlacingStructure(BuildBP, PreviewStructure);
	}
}

void AOrionPlayerController::UpdatePlacingStructure(TSubclassOf<AActor> /*unused*/,
                                                    AActor*& Preview)
{
	if (!Preview)
	{
		return;
	}

	/* ---------- ① 组件指针 ---------- */
	UOrionStructureComponent* StructComp =
		Preview->FindComponentByClass<UOrionStructureComponent>();
	if (!StructComp)
	{
		return; // 预览体没有结构组件 → 直接自由放置
	}

	const EOrionStructure Kind = StructComp->OrionStructureType;

	FVector DesiredLocation = GroundHit.ImpactPoint;
	if (Kind == EOrionStructure::Wall)
	{
		DesiredLocation.Z += 150.f;
	}
	else if (Kind == EOrionStructure::BasicSquareFoundation || Kind == EOrionStructure::BasicTriangleFoundation)
	{
		UE_LOG(LogTemp, Log, TEXT("UpdatePlacingStructure: %s"), *Preview->GetName());
		DesiredLocation.Z += 20.f;
	}

	if (IsInputKeyDown(EKeys::Up))
	{
		UE_LOG(LogTemp, Warning, TEXT("Up key is pressed"));
		PreviewStructure->AddActorLocalRotation(FRotator(0.f, 1.0f, 0.f));
	}

	if (IsInputKeyDown(EKeys::Down))
	{
		UE_LOG(LogTemp, Warning, TEXT("Down key is pressed"));
		PreviewStructure->AddActorLocalRotation(FRotator(0.f, -1.0f, 0.f));
	}

	/* ---------- ② 搜索最近空闲 Socket ---------- */
	float BestDistSqr = SnapInDistSqr;
	bool bFoundBest = false;
	FOrionGlobalSocket BestSocket;

	for (const FOrionGlobalSocket& S : BuildingManager->GetSnapSockets())
	{
		if (S.Kind != Kind || S.bOccupied)
		{
			continue;
		}

		const float D2 = FVector::DistSquared(DesiredLocation, S.Location);
		if (D2 < BestDistSqr)
		{
			BestDistSqr = D2;
			BestSocket = S;
			bFoundBest = true;
		}
	}

	/* ---------- ③ 位置 / 吸附处理 ---------- */
	if (bFoundBest)
	{
		bStructureSnapped = true;
		SnappedSocketLoc = BestSocket.Location;
		SnappedSocketRot = BestSocket.Rotation;

		Preview->SetActorLocation(SnappedSocketLoc);
		Preview->SetActorRotation(SnappedSocketRot);
	}
	else
	{
		if (bStructureSnapped &&
			FVector::DistSquared(DesiredLocation, SnappedSocketLoc) > SnapOutDist * SnapOutDist)
		{
			bStructureSnapped = false;
		}
		if (!bStructureSnapped)
		{
			Preview->SetActorLocation(DesiredLocation);
		}
	}
}

void AOrionPlayerController::TogglePlacingStructure(TSubclassOf<AActor> BPClass,
                                                    AActor*& PreviewPtr)
{
	if (CurrentInputMode != EOrionInputMode::Building || !BPClass) { return; }

	if (!bPlacingStructure)
	{
		SpawnPreviewStructure(BPClass, PreviewPtr); // 进入放置
	}
	else
	{
		if (PreviewPtr)
		{
			PreviewPtr->Destroy();
		}
		PreviewPtr = nullptr;
		bPlacingStructure = false;
		bStructureSnapped = false;
	}
}

void AOrionPlayerController::SpawnPreviewStructure(TSubclassOf<AActor> BPClass,
                                                   AActor*& OutPtr)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	bIsSpawnPreviewStructure = true;

	OutPtr = World->SpawnActor<AActor>(BPClass, FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (OutPtr)
	{
		OutPtr->SetActorEnableCollision(false);
		bPlacingStructure = true;
	}
}

void AOrionPlayerController::ConfirmPlaceStructure(TSubclassOf<AActor> BPClass,
                                                   AActor*& PreviewPtr)
{
	if (!bPlacingStructure || !PreviewPtr)
	{
		return;
	}

	FTransform SpawnXform = bStructureSnapped
		                        ? FTransform(SnappedSocketRot, SnappedSocketLoc, SnappedSocketScale)
		                        : PreviewPtr->GetTransform();

	if (!BuildingManager->ConfirmPlaceStructure(BPClass, PreviewPtr,
	                                            bStructureSnapped, SpawnXform))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("[Building] Failed to place structure. Snapped: %s"),
		       bStructureSnapped ? TEXT("true") : TEXT("false"));
		return;
	}

	// 保持放置模式
	SpawnPreviewStructure(BPClass, PreviewPtr);
	bStructureSnapped = false;
}

void AOrionPlayerController::OnToggleDemolishingMode()
{
	if (!bDemolishingMode)
	{
		bDemolishingMode = true;
	}
	else
	{
		bDemolishingMode = false;
	}
}

void AOrionPlayerController::DemolishStructureUnderCursor() const
{
	UE_LOG(LogTemp, Log, TEXT("Demolishing structure under cursor."));

	FHitResult HitResult;
	GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false,
		HitResult
	);
	AActor* HitStructure = Cast<AActor>(HitResult.GetActor());
	if (HitStructure)
	{
		UE_LOG(LogTemp, Log, TEXT("Demolishing structure: %s"), *HitStructure->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("No structure found under cursor."));
	}
	if (HitStructure)
	{
		HitStructure->Destroy();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("No structure found under cursor."));
	}
}

void AOrionPlayerController::OnLeftMouseDown()
{
	bIsSelecting = true;
	bHasDragged = false;
	InitialClickPos = CurrentMousePos = FVector2D::ZeroVector;

	InitialClickPos = CurrentMousePos = FVector2D(MouseX, MouseY);

	GetWorld()->GetTimerManager().SetTimer(DragDetectTimerHandle, this, &AOrionPlayerController::DetectDraggingControl,
	                                       0.01f, true);


	GetMousePosition(InitialClickPos.X, InitialClickPos.Y);
}

void AOrionPlayerController::OnLeftMouseUp()
{
	GetWorld()->GetTimerManager().ClearTimer(DragDetectTimerHandle);

	if (!bIsSelecting)
	{
		return;
	}

	bIsSelecting = false;

	if (CurrentInputMode == EOrionInputMode::Default)
	{
		if (!bHasDragged)
		{
			SingleSelectionUnderCursor();
		}
		else
		{
			BoxSelectionUnderCursor(InitialClickPos, CurrentMousePos);
		}
	}
	else if (CurrentInputMode == EOrionInputMode::Building)
	{
		if (!bHasDragged)
		{
			if (PreviewStructure)
			{
				ConfirmPlaceStructure(BuildBP, PreviewStructure);
			}
			else if (bDemolishingMode)
			{
				DemolishStructureUnderCursor();
			}
		}
		else
		{
			BoxSelectionUnderCursor(InitialClickPos, CurrentMousePos);
		}
	}
}

void AOrionPlayerController::EmptyOrionCharaSelection(TObservableArray<AOrionChara*>& OrionCharaSelectionRef)
{
	if (!OrionCharaSelectionRef.IsEmpty())
	{
		for (AOrionChara* SelectedChara : OrionCharaSelectionRef)
		{
			IOrionInterfaceSelectable* SelectedCharaSelectable = Cast<IOrionInterfaceSelectable>(
				SelectedChara);
			SelectedCharaSelectable->OnRemoveFromSelection(this);
		}

		OrionCharaSelectionRef.Empty();
	}
}

void AOrionPlayerController::SingleSelectionUnderCursor()
{
	FHitResult HitResult;
	GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false,
		HitResult
	);

	//bool bIsShiftKeyPressed = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);

	if (!HitResult.bBlockingHit)
	{
		// Pass Custom Channel directly if that overload exists: ECC_GameTraceChannel1 is mapped as "Landscape" in Editor. See DefaultEngine.ini.
		// Try to trace landscape using landscape channel

		GetHitResultUnderCursorByChannel(
			UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1),
			false,
			HitResult
		);

		if (HitResult.bBlockingHit)
		{
			UE_LOG(LogTemp, Log, TEXT("Hit On Ground. "));

			/* Clicked On Ground or Others */
			if (!bIsShiftPressed)
			{
				EmptyOrionCharaSelection(OrionCharaSelection);

				OrionPawnSelection.Empty();
				ClickedOnOrionActor = nullptr;
				UE_LOG(LogTemp, Log, TEXT("No OrionChara or Vehicle selected. Selection cleared."));
			}

			return;
		}
	}


	if (AActor* HitActor = HitResult.GetActor())
	{
		UE_LOG(LogTemp, Log, TEXT("Left Clicked Actor Name: %s | Class: %s"),
		       *HitActor->GetName(),
		       *HitActor->GetClass()->GetName());

		if (!HitActor->GetClass()->ImplementsInterface(UOrionInterfaceSelectable::StaticClass()))
		{
			UE_LOG(LogTemp, Log, TEXT("Hit Actor does not implement IOrionInterfaceSelectable."));

			if (!bIsShiftPressed)
			{
				EmptyOrionCharaSelection(OrionCharaSelection);
				OrionPawnSelection.Empty();
			}

			return;
		}

		IOrionInterfaceSelectable* ISelectable = Cast<IOrionInterfaceSelectable>(HitActor);

		switch (const ESelectable SelectableType = ISelectable->GetSelectableType())
		{
		case ESelectable::OrionChara:
			{
				if (AOrionChara* Chara = Cast<AOrionChara>(HitActor))
				{
					if (bIsShiftPressed)
					{
						if (!OrionCharaSelection.Contains(Chara))

						{
							OrionCharaSelection.Add(Chara);
							ISelectable->OnSelected(this);
						}
						else
						{
							OrionCharaSelection.Remove(Chara);
							ISelectable->OnRemoveFromSelection(this);
						}
					}
					else /* bIsShiftPressed == false */
					{
						EmptyOrionCharaSelection(OrionCharaSelection);

						OrionCharaSelection.Add(Chara);
						ISelectable->OnSelected(this);
					}
				}

				if (!OrionPawnSelection.IsEmpty())
				{
					OrionPawnSelection.Empty();
				}

				break;
			}
		case ESelectable::OrionActor:
			{
				if (AOrionActor* ClickedOrionActor = Cast<AOrionActor>(HitActor))
				{
					ClickedOnOrionActor = ClickedOrionActor;
					//OnOrionActorSelectionChanged.ExecuteIfBound(ClickedOnOrionActor);
				}
				break;
			}
		case ESelectable::VehiclePawn:
			{
				if (AWheeledVehiclePawn* HitPawn = Cast<AWheeledVehiclePawn>(HitActor))
				{
					EmptyOrionCharaSelection(OrionCharaSelection);


					OrionPawnSelection.Empty();
					if (bIsShiftPressed)
					{
						if (!OrionPawnSelection.Contains(HitPawn))
						{
							OrionPawnSelection.Add(HitPawn);
							EmptyOrionCharaSelection(OrionCharaSelection);
							UE_LOG(LogTemp, Log, TEXT("Added APawn to selection: %s"), *HitPawn->GetName());
						}
						else
						{
							OrionPawnSelection.Remove(HitPawn);
							UE_LOG(LogTemp, Log, TEXT("Removed APawn from selection: %s"), *HitPawn->GetName());
						}
					}
					else
					{
						OrionPawnSelection.Empty();
						OrionPawnSelection.Add(HitPawn);
					}
				}
				break;
			}
		default:
			{
				UE_LOG(LogTemp, Warning, TEXT("Unknown Selectable Type: %s"), *UEnum::GetValueAsString(SelectableType));

				break;
			}
		}
	}
}

void AOrionPlayerController::BoxSelectionUnderCursor(const FVector2D& StartPos, const FVector2D& EndPos)
{
	/*EmptyOrionCharaSelection

	float MinX = FMath::Min(StartPos.X, EndPos.X);
	float MaxX = FMath::Max(StartPos.X, EndPos.X);
	float MinY = FMath::Min(StartPos.Y, EndPos.Y);
	float MaxY = FMath::Max(StartPos.Y, EndPos.Y);

	UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Rect Range: (%.2f, %.2f) ~ (%.2f, %.2f)"),
	       MinX, MinY, MaxX, MaxY);

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionChara::StaticClass(), FoundActors);

	if (FoundActors.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxSelection] No AOrionChara found in the world."));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Found %d AOrionChara in the world."), FoundActors.Num());

	for (AActor* Actor : FoundActors)
	{
		if (!Actor)
		{
			continue;
		}

		FVector2D ScreenPos;
		bool bProjected = ProjectWorldLocationToScreen(Actor->GetActorLocation(), ScreenPos);

		if (!bProjected)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("[BoxSelection] Actor %s could not be projected to screen (behind camera / offscreen?)."),
			       *Actor->GetName());
			continue;
		}
		UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Actor %s => ScreenPos: (%.2f, %.2f)"),
		       *Actor->GetName(), ScreenPos.X, ScreenPos.Y);

		const bool bInsideX = (ScreenPos.X >= MinX && ScreenPos.X <= MaxX);
		const bool bInsideY = (ScreenPos.Y >= MinY && ScreenPos.Y <= MaxY);

		if (bInsideX && bInsideY)
		{
			if (AOrionChara* Chara = Cast<AOrionChara>(Actor))
			{
				OrionCharaSelection.Add(Chara);
				Chara->bIsOrionAIControlled = true;
				UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Actor %s is in selection box -> ADDED."), *Actor->GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("[BoxSelection] Actor %s is out of selection box."), *Actor->GetName());
		}
	}

	if (OrionCharaSelection.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[BoxSelection] No AOrionChara selected after box check."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Selected %d AOrionChara(s)."), (int32)OrionCharaSelection.Num());
		for (auto* Chara : OrionCharaSelection)
		{
			UE_LOG(LogTemp, Log, TEXT(" - %s"), *Chara->GetName());
		}
	}*/

	UE_LOG(LogTemp, Log, TEXT("BoxSelectionUnderCursor is temporarily disabled yet."));
}

void AOrionPlayerController::OnRightMouseUp()
{
	const float PressDuration = GetWorld()->GetTimeSeconds() - RightMouseDownTime;

	if (OrionCharaSelection.IsEmpty() || !CachedRightClickedOrionActor)
	{
		return;
	}

	if (PressDuration < RightClickHoldThreshold) /* Short Press */
	{
		if (AOrionActorStorage* StorageActorInstance = Cast<AOrionActorStorage>(CachedRightClickedOrionActor))
		{
			if (!bIsShiftPressed)
			{
				UE_LOG(LogTemp, Log, TEXT("Immediate Command on Storage Currently Unavailable. "));
			}
			else
			{
				for (const auto& EachChara : OrionCharaSelection)
				{
					if (IOrionInterfaceActionable* IActionable = Cast<IOrionInterfaceActionable>(EachChara))
					{
						FString ActionName = FString::Printf(
							TEXT("CollectingCargo_%s"), *StorageActorInstance->GetName());
						OrionAction AddingAction = IActionable->
							InitActionCollectCargo(ActionName, StorageActorInstance);
						IActionable->InsertOrionActionToQueue(AddingAction, EActionExecution::Procedural, -1);
					}
				}
			}
		}
		else if (AOrionActorProduction* ProductionActorInstance = Cast<AOrionActorProduction>(
			CachedRightClickedOrionActor))
		{
			if (!bIsShiftPressed)
			{
				for (const auto& EachChara : OrionCharaSelection)
				{
					if (IOrionInterfaceActionable* IActionable = Cast<IOrionInterfaceActionable>(EachChara))
					{
						if (IActionable->GetIsCharaProcedural())
						{
							IActionable->SetIsCharaProcedural(false);
						}


						IActionable->RemoveAllActions();

						FString ActionName = FString::Printf(
							TEXT("InteractWithProduction_%s"), *ProductionActorInstance->GetName());
						OrionAction AddingAction = IActionable->InitActionInteractWithProduction(
							ActionName, ProductionActorInstance);
						IActionable->InsertOrionActionToQueue(AddingAction, EActionExecution::RealTime, -1);
					}
				}
			}
			else
			{
				for (const auto& EachChara : OrionCharaSelection)
				{
					if (IOrionInterfaceActionable* IActionable = Cast<IOrionInterfaceActionable>(EachChara))
					{
						FString ActionName = FString::Printf(
							TEXT("InteractWithProduction_%s"), *ProductionActorInstance->GetName());
						OrionAction AddingAction = IActionable->InitActionInteractWithProduction(
							ActionName, ProductionActorInstance);
						IActionable->InsertOrionActionToQueue(AddingAction, EActionExecution::Procedural, -1);
					}
				}
			}
		}
		else if (AOrionActorOre* OreActorInstance = Cast<AOrionActorOre>(CachedRightClickedOrionActor))
		{
			if (!bIsShiftPressed)
			{
				for (const auto& EachChara : OrionCharaSelection)
				{
					if (IOrionInterfaceActionable* IActionable = Cast<IOrionInterfaceActionable>(EachChara))
					{
						if (IActionable->GetIsCharaProcedural())
						{
							IActionable->SetIsCharaProcedural(false);
						}

						IActionable->RemoveAllActions();

						FString ActionName = FString::Printf(
							TEXT("InteractWithActor_%s"), *OreActorInstance->GetName());
						OrionAction AddingAction = IActionable->InitActionInteractWithActor(
							ActionName, OreActorInstance);
						IActionable->InsertOrionActionToQueue(AddingAction, EActionExecution::RealTime, -1);
					}
				}
			}

			else
			{
				for (const auto& EachChara : OrionCharaSelection)
				{
					if (IOrionInterfaceActionable* IActionable = Cast<IOrionInterfaceActionable>(EachChara))
					{
						FString ActionName = FString::Printf(
							TEXT("InteractWithActor_%s"), *OreActorInstance->GetName());
						OrionAction AddingAction = IActionable->InitActionInteractWithActor(
							ActionName, OreActorInstance);
						IActionable->InsertOrionActionToQueue(AddingAction, EActionExecution::Procedural, -1);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Right Clicked on an unknown Actor."));
		}
	}

	else /* Long Press */
	{
		UE_LOG(LogTemp, Warning, TEXT("Long Press on OrionActor => do something else (not InteractWithActor)."));

		// 1. Cache the selected OrionCharas - Subjects
		for (auto& each : OrionCharaSelection)
		{
			CachedActionSubjects.Add(each);
		}

		// 2. Cache the selected OrionActor - Object
		CachedActionObjects = CachedRightClickedOrionActor;


		// 3. Cache the request case name
		FString StringAttackOnOrionActor = "AttackOn" + CachedRightClickedOrionActor->GetName();

		CachedRequestCaseNames.Add(StringAttackOnOrionActor);

		CachedRequestCaseNames.Add("ExchangeCargo");

		// 4. Show the operation menu

		if (OrionHUD)
		{
			TArray<FString> ArrOptionNames;

			ArrOptionNames.Add(StringAttackOnOrionActor);
			ArrOptionNames.Add("ExchangeCargo");
			ArrOptionNames.Add("Operation3");
			OrionHUD->ShowPlayerOperationMenu(MouseX, MouseY, FHitResult(), ArrOptionNames);
		}
	}

	CachedRightClickedOrionActor = nullptr;
}

void AOrionPlayerController::OnRightMouseDown()
{
	RightMouseDownTime = GetWorld()->GetTimeSeconds();

	CachedRightClickedOrionActor = nullptr;

	FHitResult HitResult;
	GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false,
		HitResult
	);

	AActor* HitActor = nullptr;
	bool BHitOnActor = false;
	bool BHitOnLandscape = false;

	if (HitResult.bBlockingHit)
	{
		HitActor = HitResult.GetActor();
		BHitOnActor = true;
	}
	else /*!HitResult.bBlockingHit*/
	{
		// Pass Custom Channel directly if that overload exists: ECC_GameTraceChannel1 is mapped as "Landscape" in Editor. See DefaultEngine.ini.
		// Try to trace landscape using landscape channel

		GetHitResultUnderCursorByChannel(
			UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1),
			false,
			HitResult
		);


		// 0. If clicked on Landscape => Default action is MoveToLocation
		if (HitResult.bBlockingHit)
		{
			BHitOnLandscape = true;
		}
	}

	bool BHitFoundationActor = false;

	if (BHitOnActor && HitActor)
	{
		BHitFoundationActor = HitActor->IsA(AOrionStructureFoundation::StaticClass());
	}

	if (BHitOnLandscape || BHitFoundationActor)
	{
		if (NiagaraHitResultEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				NiagaraHitResultEffect,
				HitResult.Location
			);
		}

		if (!bIsShiftPressed)
		{
			/* Invoke Debug Menu */
			if (OrionCharaSelection.IsEmpty())
			{
				if (OrionHUD)
				{
					TArray<FString> ArrOptionNames;
					ArrOptionNames.Add("SpawnHostileOrionCharacterHere");
					ArrOptionNames.Add("Operation2");
					ArrOptionNames.Add("Operation3");
					OrionHUD->ShowPlayerOperationMenu(MouseX, MouseY, HitResult, ArrOptionNames);
				}
			}
			else /* OrionCharaSelection is not empty. */
			{
				for (const auto& EachChara : OrionCharaSelection)
				{
					if (IOrionInterfaceActionable* IActionable = Cast<IOrionInterfaceActionable>(EachChara))
					{
						if (IActionable->GetIsCharaProcedural())
						{
							IActionable->SetIsCharaProcedural(false);
						}

						IActionable->RemoveAllActions();

						OrionAction AddingAction = IActionable->InitActionMoveToLocation(
							TEXT("MoveToLocation"), HitResult.Location);
						IActionable->InsertOrionActionToQueue(AddingAction, EActionExecution::RealTime, -1);
					}
				}
			}
		}
		else /* bIsShiftPressed == true */
		{
			// If OrionChara or OrionPawn selected => move them to the clicked location
			for (const auto& EachChara : OrionCharaSelection)
			{
				if (IOrionInterfaceActionable* IActionable = Cast<IOrionInterfaceActionable>(EachChara))
				{
					OrionAction AddingAction = IActionable->InitActionMoveToLocation(
						TEXT("MoveToLocation"), HitResult.Location);
					IActionable->InsertOrionActionToQueue(AddingAction, EActionExecution::RealTime, -1);
				}
			}
		}
	}

	else if (BHitOnActor && HitActor)
	{
		// 1. If clicked on OrionChara => Default action is AttackOnChara
		if (AOrionChara* HitChara = Cast<AOrionChara>(HitActor))
		{
			const FVector HitOffset = HitResult.ImpactPoint - HitChara->GetActorLocation();
			const FString ActionName = FString::Printf(
				TEXT("AttackOnCharaLongRange-%s"), *HitActor->GetName());

			if (!bIsShiftPressed)
			{
				if (!OrionCharaSelection.IsEmpty())
				{
					for (const auto& EachChara : OrionCharaSelection)
					{
						if (IOrionInterfaceActionable* IActionable = Cast<IOrionInterfaceActionable>(EachChara))
						{
							if (IActionable->GetIsCharaProcedural())
							{
								IActionable->SetIsCharaProcedural(false);
							}


							if (IActionable->GetUnifiedActionName() == ActionName)
							{
								UE_LOG(LogTemp, Log, TEXT("Action already exists: %s"), *ActionName);
								continue;
							}

							/* else if (IActionable->GetUnifiedActionName().Contains("AttackOnCharaLongRange"))
							{
								UE_LOG(LogTemp, Log, TEXT("Action already exists: %s"), *ActionName);
								continue;
							} */

							IActionable->RemoveAllActions();

							OrionAction AddingAction = IActionable->InitActionAttackOnChara(
								ActionName, HitActor, HitOffset);
							IActionable->InsertOrionActionToQueue(AddingAction, EActionExecution::RealTime, -1);
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("No OrionChara selected."));
				}
			}
			else /* bIsShiftPressed == true */
			{
				if (!OrionCharaSelection.IsEmpty())
				{
					for (const auto& EachChara : OrionCharaSelection)
					{
						if (IOrionInterfaceActionable* IActionable = Cast<IOrionInterfaceActionable>(EachChara))
						{
							OrionAction AddingAction = IActionable->InitActionAttackOnChara(
								ActionName, HitActor, HitOffset);
							IActionable->InsertOrionActionToQueue(AddingAction, EActionExecution::RealTime, -1);
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("No OrionChara selected."));
				}
			}
		}

		// 2. If clicked on OrionActor => Default action is InteractWithActor
		else if (AOrionActor* HitOtherActor = Cast<AOrionActor>(HitActor))
		{
			// ★ Store it here, and distinguish between short press = Interact, and long press = other logic in OnRightMouseUp
			CachedRightClickedOrionActor = HitOtherActor;
		}

		// 3. If clicked on OrionVehicle => Default action is (...)
		else if (HitActor->GetName().Contains("OrionVehicle"))
		{
			UE_LOG(LogTemp, Log, TEXT("Clicked on OrionVehicle: %s"), *HitActor->GetName());
		}

		else
		{
			UE_LOG(LogTemp, Log, TEXT("Clicked other actor: %s"), *HitActor->GetName());
		}
	}
}

void AOrionPlayerController::SelectAll()
{
	EmptyOrionCharaSelection(OrionCharaSelection);

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionChara::StaticClass(), FoundActors);

	int32 NumLeaveUnselected = 1;
	int32 TempCounter = FoundActors.Num() - NumLeaveUnselected;
	for (AActor* Actor : FoundActors)
	{
		if (TempCounter > 0)
		{
			TempCounter--;
			if (AOrionChara* Chara = Cast<AOrionChara>(Actor))
			{
				OrionCharaSelection.Add(Chara);
				Chara->bIsOrionAIControlled = true;
				UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Actor %s is in selection box -> ADDED."), *Actor->GetName());
			}
		}
	}
}

void AOrionPlayerController::OnShiftPressed()
{
	bIsShiftPressed = true;
}

void AOrionPlayerController::OnShiftReleased()
{
	bIsShiftPressed = false;
}

void AOrionPlayerController::RequestAttackOnOrionActor(FVector HitOffset, CommandType inCommandType)
{
	if (!CachedActionSubjects.IsEmpty() && !CachedActionObjects && !OrionCharaSelection.IsEmpty())
	{
		for (const auto& EachChara : OrionCharaSelection)
		{
			if (IOrionInterfaceActionable* IActionable = Cast<IOrionInterfaceActionable>(EachChara))
			{
				if (IActionable->GetIsCharaProcedural())
				{
					IActionable->SetIsCharaProcedural(false);
				}

				FString ActionName = FString::Printf(
					TEXT("AttackOnCharaLongRange-%s"), *CachedActionObjects->GetName());

				if (IActionable->GetUnifiedActionName() == ActionName)
				{
					UE_LOG(LogTemp, Log, TEXT("Action already exists: %s"), *ActionName);
					continue;
				}

				/* else if (IActionable->GetUnifiedActionName().Contains("AttackOnCharaLongRange"))
				{
					UE_LOG(LogTemp, Log, TEXT("Action already exists: %s"), *ActionName);
					continue;
				} */

				IActionable->RemoveAllActions();

				OrionAction AddingAction = IActionable->InitActionAttackOnChara(
					ActionName, CachedActionObjects, HitOffset);
				IActionable->InsertOrionActionToQueue(AddingAction, EActionExecution::RealTime, -1);
			}
		}
	}
}

void AOrionPlayerController::CallBackRequestDistributor(FName CallBackRequest)
{
	UE_LOG(LogTemp, Log, TEXT("CallBackRequestDistributor called with request: %s"), *CallBackRequest.ToString());

	if (CachedRequestCaseNames.IsEmpty())
	{
		return;
	}

	if (CallBackRequest == CachedRequestCaseNames[0])
	{
		RequestAttackOnOrionActor(FVector::ZeroVector, CommandType::Force);
	}

	else if (CallBackRequest == CachedRequestCaseNames[1])
	{
		if (!OrionCharaSelection.IsEmpty() && OrionCharaSelection.Num() == 1)
		{
			FString ActionName = FString::Printf(
				TEXT("InteractWithInventory%s"), *CachedActionObjects->GetName());
			OrionCharaSelection[0]->CharacterActionQueue.Actions.Add(OrionAction(
				ActionName,
				[charPtr = OrionCharaSelection[0], targetActor = CachedActionObjects](
				float DeltaTime) -> bool
				{
					return charPtr->InteractWithInventory(targetActor);
				}));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ExchangeCargo request failed: Not 1 and Only 1 OrionChara selected."));
		}
	}
	else if (CallBackRequest == CachedRequestCaseNames[2])
	{
		UE_LOG(LogTemp, Log, TEXT("Operation 3 executed."));
	}

	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown CallBackRequest: %s"), *CallBackRequest.ToString());
	}

	CachedRequestCaseNames.Empty();
	CachedActionSubjects.Empty();
	CachedActionObjects = nullptr;
}

void AOrionPlayerController::DrawOrionActorStatus()
{
	FHitResult HoveringOver;
	GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, HoveringOver);

	AActor* HoveringActor = HoveringOver.GetActor();

	if (HoveringActor)
	{
		if (AOrionActor* OrionActor = Cast<AOrionActor>(HoveringActor))
		{
			OrionHUD->bShowActorInfo = true;
			TArray<FString> Lines;


			float Progress;
			if (AOrionActorOre* OrionOreActor = Cast<AOrionActorOre>(OrionActor))
			{
				Progress = OrionOreActor->ProductionProgress;
			}

			else if (AOrionActorProduction* OrionProductionActor = Cast<AOrionActorProduction>(OrionActor))
			{
				Progress = OrionProductionActor->ProductionProgress;
			}

			else
			{
				Progress = 0.0f;
			}

			int32 TotalBars = 20; // Segements number
			int32 FilledBars = FMath::RoundToInt((Progress / 100.0f) * TotalBars);

			FString Bar;
			for (int32 i = 0; i < TotalBars; ++i)
			{
				Bar += (i < FilledBars) ? TEXT("#") : TEXT("-");
			}

			Lines.Add(FString::Printf(TEXT("Name: %s"), *OrionActor->GetName()));
			Lines.Add(FString::Printf(TEXT("CurrNumOfWorkers: %d"), OrionActor->CurrWorkers));
			Lines.Add(FString::Printf(TEXT("ProductionProgress: [%s] %.1f%%"), *Bar, Progress));

			if (AOrionActorOre* OrionOreActor = Cast<AOrionActorOre>(OrionActor))
			{
				if (UOrionInventoryComponent* InvComp = OrionOreActor->FindComponentByClass<
					UOrionInventoryComponent>())
				{
					// 2) 拿到所有已持有的 (ItemId, Quantity)
					TArray<FIntPoint> Items = InvComp->GetAllItems();
					for (const FIntPoint& Pair : Items)
					{
						int32 ItemId = Pair.X;
						int32 Quantity = Pair.Y;

						// 3) 拿静态信息（DisplayName/ChineseDisplayName）
						FOrionDataItem Info = InvComp->GetItemInfo(ItemId);

						// 4) 格式化成 "<Name>: <数量>"
						Lines.Add(FString::Printf(
							TEXT("%s: %d"),
							*Info.DisplayName.ToString(),
							Quantity
						));
					}
				}
				else
				{
					Lines.Add(TEXT("No Inventory Component"));
				}
			}

			else if (AOrionActorProduction* OrionProductionActor = Cast<AOrionActorProduction>(OrionActor))
			{
				if (UOrionInventoryComponent* InvComp = OrionProductionActor->FindComponentByClass<
					UOrionInventoryComponent>())
				{
					TArray<FIntPoint> Items = InvComp->GetAllItems();
					for (const FIntPoint& Pair : Items)
					{
						int32 ItemId = Pair.X;
						int32 Quantity = Pair.Y;

						FOrionDataItem Info = InvComp->GetItemInfo(ItemId);

						Lines.Add(FString::Printf(
							TEXT("%s: %d"),
							*Info.DisplayName.ToString(),
							Quantity
						));
					}
				}
				else
				{
					Lines.Add(TEXT("No Inventory Component"));
				}
			}

			else
			{
				Lines.Add(TEXT("No Available Subclass of OrionActor. "));
			}

			Lines.Add(FString::Printf(TEXT("CurrHealth: %d"), OrionActor->CurrHealth));
			OrionHUD->ShowInfoAtMouse(Lines);
		}
	}
	else
	{
		OrionHUD->bShowActorInfo = false;
	}
}
