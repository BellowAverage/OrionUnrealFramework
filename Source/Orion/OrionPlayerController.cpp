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


AOrionPlayerController::AOrionPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    bShowMouseCursor = true;               // 显示鼠标光标
    bEnableClickEvents = true;             // 启用点击事件
    bEnableMouseOverEvents = true;         // 启用鼠标悬停事件
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
        const float DragThreshold = 5.f;
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

                    float Progress = OrionActor->ProductionProgress;  // e.g. 72.5
                    int32 TotalBars = 20;  // Segements number
                    int32 FilledBars = FMath::RoundToInt((Progress / 100.0f) * TotalBars);

                    FString Bar;
                    for (int32 i = 0; i < TotalBars; ++i)
                    {
                        Bar += (i < FilledBars) ? TEXT("#") : TEXT("-");
                    }

                    Lines.Add(FString::Printf(TEXT("Name: %s"), *OrionActor->GetName()));
                    Lines.Add(FString::Printf(TEXT("CurrNumOfWorkers: %d"), OrionActor->CurrWorkers));
                    Lines.Add(FString::Printf(TEXT("ProductionProgress: [%s] %.1f%%"), *Bar, Progress));
                    Lines.Add(FString::Printf(TEXT("CurrInventory: %d"), OrionActor->CurrInventory));
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
            CharaHUD->bShowCharaInfoPanel = true;          //★ 改这里
            CharaHUD->InfoChara = OrionCharaSelection[0];
        }
        else
        {
            CharaHUD->bShowCharaInfoPanel = false;         //★ 改这里
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
    if (!bIsSelecting) return;

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

		// 1. 检查是否是 AOrionChara
        if (AOrionChara* Chara = Cast<AOrionChara>(HitActor))
        {
            if (bIsShiftKeyPressed)
            {

                // 如果按住Shift键，检查是否已选中，未选中则添加
                if (std::find(OrionCharaSelection.begin(), OrionCharaSelection.end(), Chara) == OrionCharaSelection.end())
                {
                    OrionCharaSelection.push_back(Chara);
                    UE_LOG(LogTemp, Log, TEXT("Added OrionChara to selection: %s"), *Chara->GetName());
                }
                else
                {
                    // 如果已选中则取消选择
                    OrionCharaSelection.erase(std::remove(OrionCharaSelection.begin(), OrionCharaSelection.end(), Chara), OrionCharaSelection.end());
                    UE_LOG(LogTemp, Log, TEXT("Removed OrionChara from selection: %s"), *Chara->GetName());
                }
            }
            else
            {
                // 如果未按住Shift键，则清空之前的选择并选择新的角色
                OrionCharaSelection.clear();
                OrionCharaSelection.push_back(Chara);
                UE_LOG(LogTemp, Log, TEXT("Selected OrionChara: %s"), *Chara->GetName());
            }
            return;
        }

		// 2. 检查是否是 APawn
		else if (AWheeledVehiclePawn* HitPawn = Cast<AWheeledVehiclePawn>(HitActor))
		{        OrionPawnSelection.clear();
			if (bIsShiftKeyPressed)
			{
				if (std::find(OrionPawnSelection.begin(), OrionPawnSelection.end(), HitPawn) == OrionPawnSelection.end())
				{
					OrionPawnSelection.push_back(HitPawn);
					UE_LOG(LogTemp, Log, TEXT("Added APawn to selection: %s"), *HitPawn->GetName());
				}
				else
				{
					OrionPawnSelection.erase(std::remove(OrionPawnSelection.begin(), OrionPawnSelection.end(), HitPawn), OrionPawnSelection.end());
					UE_LOG(LogTemp, Log, TEXT("Removed APawn from selection: %s"), *HitPawn->GetName());
				}
			}
			else
			{
				// 如果未按住Shift键，则清空之前的选择并选择新的角色
				OrionPawnSelection.clear();
				OrionPawnSelection.push_back(HitPawn);
			}
			return;
		}
    }

    // 如果点到空白处或者点到了其他Actor，且未按Shift键，则清空选中
    if (!bIsShiftKeyPressed)
    {
        OrionCharaSelection.clear();
        OrionPawnSelection.clear();
        UE_LOG(LogTemp, Log, TEXT("No OrionChara or Vehicle selected. Selection cleared."));
    }
}

void AOrionPlayerController::BoxSelectionUnderCursor(const FVector2D& StartPos, const FVector2D& EndPos)
{
    // 清空已有选中
    OrionCharaSelection.clear();

    // 计算矩形范围
    float MinX = FMath::Min(StartPos.X, EndPos.X);
    float MaxX = FMath::Max(StartPos.X, EndPos.X);
    float MinY = FMath::Min(StartPos.Y, EndPos.Y);
    float MaxY = FMath::Max(StartPos.Y, EndPos.Y);

    UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Rect Range: (%.2f, %.2f) ~ (%.2f, %.2f)"),
        MinX, MinY, MaxX, MaxY);

    // 获取场景中所有 AOrionChara
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionChara::StaticClass(), FoundActors);

    // 如果没有找到任何此类对象，直接返回
    if (FoundActors.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BoxSelection] No AOrionChara found in the world."));
        return;
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Found %d AOrionChara in the world."), FoundActors.Num());
    }

    for (AActor* Actor : FoundActors)
    {
        if (!Actor) continue;

        FVector2D ScreenPos;
        bool bProjected = ProjectWorldLocationToScreen(Actor->GetActorLocation(), ScreenPos);

        // 打印调试信息：投影是否成功 + 投影后的屏幕坐标
        if (!bProjected)
        {
            UE_LOG(LogTemp, Warning, TEXT("[BoxSelection] Actor %s could not be projected to screen (behind camera / offscreen?)."),
                *Actor->GetName());
            continue;
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Actor %s => ScreenPos: (%.2f, %.2f)"),
                *Actor->GetName(), ScreenPos.X, ScreenPos.Y);
        }

        // 判断是否在屏幕矩形内
        const bool bInsideX = (ScreenPos.X >= MinX && ScreenPos.X <= MaxX);
        const bool bInsideY = (ScreenPos.Y >= MinY && ScreenPos.Y <= MaxY);

        if (bInsideX && bInsideY)
        {
            // 再次检查是否为 AOrionChara（一般来说一定是）
            if (AOrionChara* Chara = Cast<AOrionChara>(Actor))
            {
                OrionCharaSelection.push_back(Chara);
                UE_LOG(LogTemp, Log, TEXT("[BoxSelection] Actor %s is in selection box -> ADDED."), *Actor->GetName());
            }
        }
        else
        {
            UE_LOG(LogTemp, Verbose, TEXT("[BoxSelection] Actor %s is out of selection box."), *Actor->GetName());
        }
    }

    // 最后统计一下选中了多少
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
            if (!bIsShiftPressed)
            {
                OrionGameMode->ApproveInteractWithActor(OrionCharaSelection, CachedRightClickedOrionActor, CommandType::Force);
            }
            else
            {
                OrionGameMode->ApproveInteractWithActor(OrionCharaSelection, CachedRightClickedOrionActor, CommandType::Append);

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
        std::string StringAttackOnOrionActor = "AttackOn" + std::string(TCHAR_TO_UTF8(*CachedRightClickedOrionActor->GetName()));
        FString FStringAttackOnOrionActor = FString(UTF8_TO_TCHAR(StringAttackOnOrionActor.c_str()));
        CachedRequestCaseNames.push_back(FStringAttackOnOrionActor);

		// 4. Show the operation menu
        AOrionHUD* OrionHUD = Cast<AOrionHUD>(GetHUD());
        if (OrionHUD)
        {
            std::vector<std::string> ArrOptionNames;

            ArrOptionNames.push_back(StringAttackOnOrionActor);
            ArrOptionNames.push_back("Operation2");
            ArrOptionNames.push_back("Operation3");
            OrionHUD->ShowPlayerOperationMenu(MouseX, MouseY, FHitResult(), ArrOptionNames);
        }

    }

    // 清空缓存，避免下一次按下时仍旧拿到旧的Actor
    CachedRightClickedOrionActor = nullptr;
}

void AOrionPlayerController::OnRightMouseDown()
{
	UE_LOG(LogTemp, Warning, TEXT("Right mouse button pressed - 0418."));
	RightMouseDownTime = GetWorld()->GetTimeSeconds();

	CachedRightClickedOrionActor = nullptr;

    FHitResult HitResult;
    GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        HitResult
    );

    UE_LOG(LogTemp, Log, TEXT("bBlockingHit is %s"), HitResult.bBlockingHit ? TEXT("true") : TEXT("false"));

    if (!HitResult.bBlockingHit)
    {
		// Pass Custom Channel directly if that overload exists: ECC_GameTraceChannel1 is mapped as "Landscape" in Editor. See DefaultEngine.ini.
		// Try to trace landscape using landscape channel

		UE_LOG(LogTemp, Warning, TEXT("Called Landscape Channel. "));
        GetHitResultUnderCursorByChannel(
            UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1),
            false,
            HitResult
        );


		// 0. If clicked on Landscape => Default action is MoveToLocation
        if (HitResult.bBlockingHit)
        {
            UE_LOG(LogTemp, Log, TEXT("Hit ON GROUND component: %s"), *HitResult.Component->GetName());

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
						AOrionHUD* OrionHUD = Cast<AOrionHUD>(GetHUD());
						if (OrionHUD)
						{
							std::vector<std::string> ArrOptionNames;
							ArrOptionNames.push_back("SpawnOrionCharacterHere");
							ArrOptionNames.push_back("Operation2");
							ArrOptionNames.push_back("Operation3");
							OrionHUD->ShowPlayerOperationMenu(MouseX, MouseY, HitResult, ArrOptionNames);
						}
					}

					// If OrionChara or OrionPawn selected => move them to the clicked location
					else
					{
                        if (!OrionCharaSelection.empty())
                        {
                            OrionGameMode->ApproveCharaMoveToLocation(OrionCharaSelection, HitResult.Location, CommandType::Force);
                        }
						if (!OrionPawnSelection.empty())
						{
							OrionGameMode->ApprovePawnMoveToLocation(OrionPawnSelection, HitResult.Location, CommandType::Force);
						}
					}
                }
                else
                {
                    OrionGameMode->ApproveCharaMoveToLocation(OrionCharaSelection, HitResult.Location, CommandType::Append);
                    OrionGameMode->ApprovePawnMoveToLocation(OrionPawnSelection, HitResult.Location, CommandType::Append);
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
                    OrionGameMode->ApproveCharaAttackOnActor(OrionCharaSelection, HitChara, HitOffset, CommandType::Force);
                }
			}
			else
			{
                AOrionGameMode* OrionGameMode = GetWorld()->GetAuthGameMode<AOrionGameMode>();
                if (OrionGameMode && !OrionCharaSelection.empty())
                {
                    OrionGameMode->ApproveCharaAttackOnActor(OrionCharaSelection, HitChara, HitOffset, CommandType::Append);
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
    OrionCharaSelection.clear();

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionChara::StaticClass(), FoundActors);

    int32 TempCounter = FoundActors.Num() - 1;
    for (AActor* Actor : FoundActors)
    {
        if (TempCounter > 0)
        {
            TempCounter --;
            if (AOrionChara* Chara = Cast<AOrionChara>(Actor))
            {
                OrionCharaSelection.push_back(Chara);
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
            OrionGameMode->ApproveCharaAttackOnActor(CachedActionSubjects, CachedActionObjects.front(), HitOffset, CommandType::Force);
        }

    }
}

void AOrionPlayerController::CallBackRequestDistributor(FName CallBackRequest)
{
	UE_LOG(LogTemp, Log, TEXT("CallBackRequestDistributor called with request: %s"), *CallBackRequest.ToString());
	
	FString TempString = CachedRequestCaseNames.front();
	UE_LOG(LogTemp, Log, TEXT("CachedRequestCaseNames: %s"), *TempString);

    if (CallBackRequest == CachedRequestCaseNames.front())
	{
		RequestAttackOnOrionActor(FVector::ZeroVector, CommandType::Force);
	}

	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown CallBackRequest: %s"), *CallBackRequest.ToString());
	}

	CachedRequestCaseNames.clear();
	CachedActionSubjects.clear();
	CachedActionObjects.clear();

}
