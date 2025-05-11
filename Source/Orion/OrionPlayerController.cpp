#include "OrionPlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "OrionCameraPawn.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include <Kismet/GameplayStatics.h>
#include "OrionBPFunctionLibrary.h"
#include "OrionAIController.h"
#include "OrionHUD.h"
#include <algorithm>
#include "OrionActor.h"
#include "OrionGameMode.h"
#include "OrionActor/OrionActorOre.h"


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

		InputComponent->BindAction("RightMouseClick", IE_Released, this, &AOrionPlayerController::OnRightMouseUp);
		InputComponent->BindAction("ShiftPress", IE_Pressed, this, &AOrionPlayerController::OnShiftPressed);
		InputComponent->BindAction("ShiftPress", IE_Released, this, &AOrionPlayerController::OnShiftReleased);
	}
}

void AOrionPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void AOrionPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetMousePosition(MouseX, MouseY))
	{
		CurrentMousePos = FVector2D(MouseX, MouseY);
	}

	// Selecting yet not determined as dragging.
	if (bIsSelecting && !bHasDragged)
	{
		constexpr float DragThreshold = 5.f;
		if (FVector2D::Distance(CurrentMousePos, InitialClickPos) > DragThreshold)
		{
			bHasDragged = true;
		}
	}

	FHitResult HoveringOver;
	GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, HoveringOver);

	AActor* HoveringActor = HoveringOver.GetActor();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AOrionHUD* ActorInfoHUD = Cast<AOrionHUD>(PC->GetHUD()))
		{
			if (HoveringActor)
			{
				if (AOrionActor* OrionActor = Cast<AOrionActor>(HoveringActor))
				{
					ActorInfoHUD->bShowActorInfo = true;
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
								FOrionItemInfo Info = InvComp->GetItemInfo(ItemId);

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

								FOrionItemInfo Info = InvComp->GetItemInfo(ItemId);

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
					ActorInfoHUD->ShowInfoAtMouse(Lines);
				}
			}
			else
			{
				ActorInfoHUD->bShowActorInfo = false;
			}
		}
	}


	if (AOrionHUD* CharaHUD = Cast<AOrionHUD>(GetHUD()))
	{
		if (OrionCharaSelection.size() == 1)
		{
			CharaHUD->bShowCharaInfoPanel = true;
			CharaHUD->InfoChara = OrionCharaSelection[0];
		}
		else
		{
			CharaHUD->bShowCharaInfoPanel = false;
			CharaHUD->InfoChara = nullptr;
		}
	}
}

void AOrionPlayerController::OnLeftMouseDown()
{
	bIsSelecting = true;
	bHasDragged = false;

	GetMousePosition(InitialClickPos.X, InitialClickPos.Y);
}

void AOrionPlayerController::OnLeftMouseUp()
{
	if (!bIsSelecting)
	{
		return;
	}

	bIsSelecting = false;

	// No dragged discovered: single selection.
	if (!bHasDragged)
	{
		SingleSelectionUnderCursor();
	}
	else
	{
		BoxSelectionUnderCursor(InitialClickPos, CurrentMousePos);
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

	bool bIsShiftKeyPressed = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);

	AActor* HitActor = HitResult.GetActor();
	if (HitActor)
	{
		UE_LOG(LogTemp, Log, TEXT("Left Clicked Actor Name: %s | Class: %s"),
		       *HitActor->GetName(),
		       *HitActor->GetClass()->GetName());

		if (AOrionActor* ClickedOrionActor = Cast<AOrionActor>(HitActor))
		{
			ClickedOnOrionActor = ClickedOrionActor;
			OnOrionActorSelectionChanged.ExecuteIfBound(ClickedOnOrionActor);
		}


		if (AOrionChara* Chara = Cast<AOrionChara>(HitActor))
		{
			if (!OrionPawnSelection.empty())
			{
				OrionPawnSelection.clear();
			}

			if (bIsShiftKeyPressed)
			{
				// 如果按住Shift键，检查是否已选中，未选中则添加
				if (std::find(OrionCharaSelection.begin(), OrionCharaSelection.end(), Chara) == OrionCharaSelection.
					end())
				{
					OrionCharaSelection.push_back(Chara);
					Chara->bIsOrionAIControlled = true;
					UE_LOG(LogTemp, Log, TEXT("Added OrionChara to selection: %s"), *Chara->GetName());
				}
				else
				{
					// 如果已选中则取消选择
					std::erase(OrionCharaSelection, Chara);
					Chara->bIsOrionAIControlled = false;
					UE_LOG(LogTemp, Log, TEXT("Removed OrionChara from selection: %s"), *Chara->GetName());
				}
			}
			else
			{
				if (!OrionCharaSelection.empty())
				{
					for (AOrionChara* SelectedChara : OrionCharaSelection)
					{
						SelectedChara->bIsOrionAIControlled = false;
					}

					OrionCharaSelection.clear();
				}

				OrionCharaSelection.push_back(Chara);
				Chara->bIsOrionAIControlled = true;
				UE_LOG(LogTemp, Log, TEXT("Selected OrionChara: %s"), *Chara->GetName());
			}
			return;
		}

		if (AWheeledVehiclePawn* HitPawn = Cast<AWheeledVehiclePawn>(HitActor))
		{
			if (!OrionCharaSelection.empty())
			{
				for (AOrionChara* SelectedChara : OrionCharaSelection)
				{
					SelectedChara->bIsOrionAIControlled = false;
				}

				OrionCharaSelection.clear();
			}


			OrionPawnSelection.clear();
			if (bIsShiftKeyPressed)
			{
				if (std::find(OrionPawnSelection.begin(), OrionPawnSelection.end(), HitPawn) == OrionPawnSelection.
					end())
				{
					OrionPawnSelection.push_back(HitPawn);
					if (!OrionCharaSelection.empty())
					{
						for (AOrionChara* SelectedChara : OrionCharaSelection)
						{
							SelectedChara->bIsOrionAIControlled = false;
						}

						OrionCharaSelection.clear();
					}
					UE_LOG(LogTemp, Log, TEXT("Added APawn to selection: %s"), *HitPawn->GetName());
				}
				else
				{
					std::erase(OrionPawnSelection, HitPawn);
					UE_LOG(LogTemp, Log, TEXT("Removed APawn from selection: %s"), *HitPawn->GetName());
				}
			}
			else
			{
				OrionPawnSelection.clear();
				OrionPawnSelection.push_back(HitPawn);
			}
			return;
		}
	}

	// 如果点到空白处或者点到了其他Actor，且未按Shift键，则清空选中
	if (!bIsShiftKeyPressed)
	{
		if (!OrionCharaSelection.empty())
		{
			for (AOrionChara* SelectedChara : OrionCharaSelection)
			{
				SelectedChara->bIsOrionAIControlled = false;
			}

			OrionCharaSelection.clear();
		}

		OrionPawnSelection.clear();
		ClickedOnOrionActor = nullptr;
		OnOrionActorSelectionChanged.ExecuteIfBound(ClickedOnOrionActor);
		UE_LOG(LogTemp, Log, TEXT("No OrionChara or Vehicle selected. Selection cleared."));
	}
}

void AOrionPlayerController::BoxSelectionUnderCursor(const FVector2D& StartPos, const FVector2D& EndPos)
{
	if (!OrionCharaSelection.empty())
	{
		for (AOrionChara* SelectedChara : OrionCharaSelection)
		{
			SelectedChara->bIsOrionAIControlled = false;
		}

		OrionCharaSelection.clear();
	}

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
				OrionCharaSelection.push_back(Chara);
				Chara->bIsOrionAIControlled = true;
				UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Actor %s is in selection box -> ADDED."), *Actor->GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("[BoxSelection] Actor %s is out of selection box."), *Actor->GetName());
		}
	}

	if (OrionCharaSelection.size() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[BoxSelection] No AOrionChara selected after box check."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Selected %d AOrionChara(s)."), (int32)OrionCharaSelection.size());
		for (auto* Chara : OrionCharaSelection)
		{
			UE_LOG(LogTemp, Log, TEXT(" - %s"), *Chara->GetName());
		}
	}
}

void AOrionPlayerController::OnRightMouseUp()
{
	const float PressDuration = GetWorld()->GetTimeSeconds() - RightMouseDownTime;

	if (OrionCharaSelection.empty() || !CachedRightClickedOrionActor)
	{
		return;
	}

	if (PressDuration < RightClickHoldThreshold)
	{
		// Short press
		UE_LOG(LogTemp, Log, TEXT("Short Press on OrionActor => InteractWithActor."));
		AOrionGameMode* OrionGameMode = GetWorld()->GetAuthGameMode<AOrionGameMode>();
		if (OrionGameMode && !OrionCharaSelection.empty())
		{
			if (AOrionActorStorage* TempActorStorage = Cast<AOrionActorStorage>(CachedRightClickedOrionActor))
			{
				if (!bIsShiftPressed)
				{
					UE_LOG(LogTemp, Log, TEXT("Immediate Command on Storage Currently Unavailable. "));
				}
				else
				{
					OrionGameMode->ApproveCollectingCargo(OrionCharaSelection, TempActorStorage,
					                                      CommandType::Append);
				}
			}
			else if (AOrionActorProduction* TempActorProduction = Cast<AOrionActorProduction>(
				CachedRightClickedOrionActor))
			{
				if (!bIsShiftPressed)
				{
					OrionGameMode->ApproveInteractWithProduction(OrionCharaSelection, TempActorProduction,
					                                             CommandType::Force);
				}
				else
				{
					OrionGameMode->ApproveInteractWithProduction(OrionCharaSelection, TempActorProduction,
					                                             CommandType::Append);
				}
			}
			else if (AOrionActorOre* TempActorOre = Cast<AOrionActorOre>(CachedRightClickedOrionActor))
			{
				if (!bIsShiftPressed)
				{
					OrionGameMode->ApproveInteractWithActor(OrionCharaSelection, CachedRightClickedOrionActor,
					                                        CommandType::Force);
				}

				else
				{
					OrionGameMode->ApproveInteractWithActor(OrionCharaSelection, CachedRightClickedOrionActor,
					                                        CommandType::Append);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Right Clicked on an unknown Actor."));
			}
		}
	}
	else
	{
		// Long press
		UE_LOG(LogTemp, Warning, TEXT("Long Press on OrionActor => do something else (not InteractWithActor)."));

		// 1. Cache the selected OrionCharas - Subjects
		for (auto& each : OrionCharaSelection)
		{
			CachedActionSubjects.push_back(each);
		}

		// 2. Cache the selected OrionActor - Object
		CachedActionObjects.push_back(CachedRightClickedOrionActor);


		// 3. Cache the request case name
		std::string StringAttackOnOrionActor = "AttackOn" + std::string(
			TCHAR_TO_UTF8(*CachedRightClickedOrionActor->GetName()));
		FString FStringAttackOnOrionActor = FString(UTF8_TO_TCHAR(StringAttackOnOrionActor.c_str()));
		CachedRequestCaseNames.push_back(FStringAttackOnOrionActor);

		CachedRequestCaseNames.push_back("ExchangeCargo");

		// 4. Show the operation menu
		AOrionHUD* OrionHUD = Cast<AOrionHUD>(GetHUD());
		if (OrionHUD)
		{
			std::vector<std::string> ArrOptionNames;

			ArrOptionNames.push_back(StringAttackOnOrionActor);
			ArrOptionNames.push_back("ExchangeCargo");
			ArrOptionNames.push_back("Operation3");
			OrionHUD->ShowPlayerOperationMenu(MouseX, MouseY, FHitResult(), ArrOptionNames);
		}
	}

	// 清空缓存，避免下一次按下时仍旧拿到旧的Actor
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

	if (!HitResult.bBlockingHit)
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
			if (NiagaraHitResultEffect)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					GetWorld(),
					NiagaraHitResultEffect,
					HitResult.Location
				);
			}

			AOrionGameMode* OrionGameMode = GetWorld()->GetAuthGameMode<AOrionGameMode>();
			if (OrionGameMode)
			{
				if (!bIsShiftPressed)
				{
					// If no OrionChara or OrionPawn selected => show menu
					if (OrionCharaSelection.empty() && OrionPawnSelection.empty())
					{
						//AOrionHUD* OrionHUD = Cast<AOrionHUD>(GetHUD());
						//if (OrionHUD)
						//{
						//	std::vector<std::string> ArrOptionNames;
						//	ArrOptionNames.push_back("SpawnHostileOrionCharacterHere");
						//	ArrOptionNames.push_back("Operation2");
						//	ArrOptionNames.push_back("Operation3");
						//	OrionHUD->ShowPlayerOperationMenu(MouseX, MouseY, HitResult, ArrOptionNames);
						//}
					}

					// If OrionChara or OrionPawn selected => move them to the clicked location
					else
					{
						if (!OrionCharaSelection.empty())
						{
							OrionGameMode->ApproveCharaMoveToLocation(OrionCharaSelection, HitResult.Location,
							                                          CommandType::Force);
						}
						if (!OrionPawnSelection.empty())
						{
							OrionGameMode->ApprovePawnMoveToLocation(OrionPawnSelection, HitResult.Location,
							                                         CommandType::Force);
						}
					}
				}
				else
				{
					OrionGameMode->ApproveCharaMoveToLocation(OrionCharaSelection, HitResult.Location,
					                                          CommandType::Append);
					OrionGameMode->ApprovePawnMoveToLocation(OrionPawnSelection, HitResult.Location,
					                                         CommandType::Append);
				}
			}

			return;
		}
	}

	if (HitResult.bBlockingHit)
	{
		AActor* HitWorldActor = HitResult.GetActor();

		// 1. If clicked on OrionChara => Default action is AttackOnChara
		if (AOrionChara* HitChara = Cast<AOrionChara>(HitWorldActor))
		{
			FVector HitOffset = HitResult.ImpactPoint - HitChara->GetActorLocation();

			if (!bIsShiftPressed)
			{
				AOrionGameMode* OrionGameMode = GetWorld()->GetAuthGameMode<AOrionGameMode>();
				if (OrionGameMode && !OrionCharaSelection.empty())
				{
					OrionGameMode->ApproveCharaAttackOnActor(OrionCharaSelection, HitChara, HitOffset,
					                                         CommandType::Force);
				}
			}
			else
			{
				AOrionGameMode* OrionGameMode = GetWorld()->GetAuthGameMode<AOrionGameMode>();
				if (OrionGameMode && !OrionCharaSelection.empty())
				{
					OrionGameMode->ApproveCharaAttackOnActor(OrionCharaSelection, HitChara, HitOffset,
					                                         CommandType::Append);
				}
			}
		}

		// 2. If clicked on OrionActor => Default action is InteractWithActor
		else if (AOrionActor* HitActor = Cast<AOrionActor>(HitWorldActor))
		{
			// ★ 把它存起来，等 OnRightMouseUp 时再做区分：短按=Interact，长按=其他逻辑
			CachedRightClickedOrionActor = HitActor;
		}

		// 3. If clicked on OrionVehicle => Default action is (...)
		else if (HitWorldActor->GetName().Contains("OrionVehicle"))
		{
			UE_LOG(LogTemp, Log, TEXT("Clicked on OrionVehicle: %s"), *HitWorldActor->GetName());
		}

		else
		{
			UE_LOG(LogTemp, Log, TEXT("Clicked other actor: %s"), *HitWorldActor->GetName());
		}
	}
}

void AOrionPlayerController::SelectAll()
{
	if (!OrionCharaSelection.empty())
	{
		for (AOrionChara* SelectedChara : OrionCharaSelection)
		{
			SelectedChara->bIsOrionAIControlled = false;
		}

		OrionCharaSelection.clear();
	}

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
				OrionCharaSelection.push_back(Chara);
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
	if (!CachedActionSubjects.empty() && !CachedActionObjects.empty())
	{
		AOrionGameMode* OrionGameMode = GetWorld()->GetAuthGameMode<AOrionGameMode>();
		if (OrionGameMode)
		{
			OrionGameMode->ApproveCharaAttackOnActor(CachedActionSubjects, CachedActionObjects.front(), HitOffset,
			                                         CommandType::Force);
		}
	}
}

void AOrionPlayerController::CallBackRequestDistributor(FName CallBackRequest)
{
	UE_LOG(LogTemp, Log, TEXT("CallBackRequestDistributor called with request: %s"), *CallBackRequest.ToString());

	if (CachedRequestCaseNames.empty())
	{
		return;
	}

	if (!CachedRequestCaseNames.empty())
	{
		FString TempString = CachedRequestCaseNames.front();
		UE_LOG(LogTemp, Log, TEXT("CachedRequestCaseNames: %s"), *TempString);
	}

	if (CallBackRequest == CachedRequestCaseNames.front())
	{
		RequestAttackOnOrionActor(FVector::ZeroVector, CommandType::Force);
	}

	else if (CallBackRequest == CachedRequestCaseNames[1])
	{
		if (!OrionCharaSelection.empty() && OrionCharaSelection.size() == 1)
		{
			FString ActionName = FString::Printf(TEXT("MoveToLocation%s"), *CachedActionObjects.front()->GetName());
			OrionCharaSelection.front()->CharacterActionQueue.Actions.push_back(Action(
				ActionName,
				[charPtr = OrionCharaSelection.front(), targetActor = CachedActionObjects.front()](
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

	CachedRequestCaseNames.clear();
	CachedActionSubjects.clear();
	CachedActionObjects.clear();
}
