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

                    float Progress = OrionActor->GetProductionProgress();  // e.g. 72.5
                    int32 TotalBars = 20;  // 总共显示多少格
                    int32 FilledBars = FMath::RoundToInt((Progress / 100.0f) * TotalBars);

                    FString Bar;
                    for (int32 i = 0; i < TotalBars; ++i)
                    {
                        Bar += (i < FilledBars) ? TEXT("#") : TEXT("-");
                    }

                    Lines.Add(FString::Printf(TEXT("Name: %s"), *OrionActor->GetName()));
                    Lines.Add(FString::Printf(TEXT("CurrNumOfWorkers: %d"), OrionActor->GetCurrWorkers()));
                    Lines.Add(FString::Printf(TEXT("ProductionProgress: [%s] %.1f%%"), *Bar, Progress));
                    Lines.Add(FString::Printf(TEXT("CurrInventory: %d"), OrionActor->GetCurrInventory()));
                    ActorInfoHUD->ShowInfoAtMouse(Lines);
                }
                //else if (AOrionChara* OrionChara = Cast<AOrionChara>(HoveringActor))
                //{
                //    TArray<FString> Lines;
                //    Lines.Add(FString::Printf(TEXT("Name: %s"), *OrionChara->GetName()));
                //    Lines.Add(FString::Printf(TEXT("Location: %s"), *OrionChara->GetActorLocation().ToString()));
                //    Lines.Add(FString::Printf(TEXT("Rotation: %s"), *OrionChara->GetActorRotation().ToString()));
                //    ActorInfoHUD->ShowInfoAtMouse(Lines);
                //}
            }
            else
            {
				ActorInfoHUD->bShowActorInfo = false;
            }
        }
	}
}

// discarded
void AOrionPlayerController::OnLeftMouseClick()
{
    FHitResult HitResult;
    GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, HitResult);

    AActor* HitActor = HitResult.GetActor();

    if (HitActor)
    {
        UE_LOG(LogTemp, Log, TEXT("Clicked on: %s"), *HitActor->GetName());
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

// ==============================
//   单点选中：检测鼠标下对象
// ==============================
void AOrionPlayerController::SingleSelectionUnderCursor()
{
    FHitResult HitResult;
    GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        HitResult
    );

    // 获取是否按下Ctrl键
    bool bIsCtrlPressed = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);

    // 在点击位置播放特效
    //if (NiagaraHitResultEffect && HitResult.bBlockingHit)
    //{
    //    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
    //        GetWorld(),
    //        NiagaraHitResultEffect,
    //        HitResult.Location
    //    );
    //}

    // 如果点中了某个 Actor，检查是否是 AOrionChara
    AActor* HitActor = HitResult.GetActor();
    if (HitActor)
    {
        UE_LOG(LogTemp, Log, TEXT("Clicked Actor Name: %s | Class: %s"),
            *HitActor->GetName(),
            *HitActor->GetClass()->GetName());

        UE_LOG(LogTemp, Log, TEXT("Clicked on: %s"), *HitActor->GetName());

        if (AOrionChara* Chara = Cast<AOrionChara>(HitActor))
        {
            if (bIsCtrlPressed)
            {

                // 如果按住Ctrl键，检查是否已选中，未选中则添加
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
                // 如果未按住Ctrl键，则清空之前的选择并选择新的角色
                OrionCharaSelection.clear();
                OrionCharaSelection.push_back(Chara);
                UE_LOG(LogTemp, Log, TEXT("Selected OrionChara: %s"), *Chara->GetName());
            }
            return;
        }
    }

    // 如果点到空白处或者点到了其他Actor，且未按Ctrl键，则清空选中
    if (!bIsCtrlPressed)
    {
        OrionCharaSelection.clear();
        UE_LOG(LogTemp, Log, TEXT("No OrionChara selected. Selection cleared."));
    }
}

// ==============================
//   框选：根据屏幕矩形选中
// ==============================
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
        // 短按：执行 InteractWithActor
        UE_LOG(LogTemp, Log, TEXT("Short Press on OrionActor => InteractWithActor."));

        for (auto& each : OrionCharaSelection)
        {
            if (!each) continue;

            FString ActionName = FString::Printf(TEXT("ForceInteractWithActor|%s"), *CachedRightClickedOrionActor->GetName());
            each->RemoveAllActions();
            each->CharacterActionQueue.Actions.push_back(
                Action(ActionName,
                    [charPtr = each, targetActor = CachedRightClickedOrionActor](float DeltaTime) -> bool
                    {
                        return charPtr->InteractWithActor(DeltaTime, targetActor);
                    }
                )
            );
        }
    }
    else
    {
        // -- 长按 --
        // 这里写你要的“长按不执行 InteractWithActor，改为执行另一个逻辑”
        UE_LOG(LogTemp, Warning, TEXT("Long Press on OrionActor => do something else (not InteractWithActor)."));

        AOrionHUD* OrionHUD = Cast<AOrionHUD>(GetHUD());
        if (OrionHUD)
        {
            std::vector<std::string> ArrOptionNames;
            std::string OptionName1 = "AttackOn" + std::string(TCHAR_TO_UTF8(*CachedRightClickedOrionActor->GetName()));
            ArrOptionNames.push_back(OptionName1);
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
	RightMouseDownTime = GetWorld()->GetTimeSeconds();

	CachedRightClickedOrionActor = nullptr;

    // 1. 获取 HitResult
    FHitResult HitResult;
    GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        HitResult
    );

    // 2. 如果确实点到了什么（bBlockingHit == true）
    if (HitResult.bBlockingHit)
    {
        // 2.1 尝试获取被点击的Actor
        AActor* HitWorldActor = HitResult.GetActor();

        // 2.2 如果点到的是 OrionChara
        if (AOrionChara* HitChara = Cast<AOrionChara>(HitWorldActor))
        {
            // （按需）决定要不要播放 Niagara 特效
            // if (NiagaraHitResultEffect)
            // {
            //     UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            //         GetWorld(),
            //         NiagaraHitResultEffect,
            //         HitChara->GetActorLocation()
            //     );
            // }
            FVector HitOffset = HitResult.ImpactPoint - HitChara->GetActorLocation();
            // 向控制选中的角色下达“攻击”指令
            if (!OrionCharaSelection.empty())
            {
                for (auto& each : OrionCharaSelection)
                {
                    FString ActionName = FString::Printf(TEXT("ForceAttackOnCharaLongRange|%s"), *HitChara->GetName());

                    if (each->CurrentAction)
                    {
                        if (each->CurrentAction->Name == ActionName)
                        {
						    UE_LOG(LogTemp, Warning, TEXT("Already attacking chara selected as target. "), *ActionName);
                            continue;
                        }
                        
                        // switch target
                        else if (each->CurrentAction->Name.Contains("AttackOnCharaLongRange"))
                        {
                            each->CharacterActionQueue.Actions.clear();

                            each->CharacterActionQueue.Actions.push_back(
                                Action(ActionName,
                                    // 捕获: 攻击者(each) + 目标(HitChara)
                                    [charPtr = each, targetChara = HitChara, inHitOffset = HitOffset](float DeltaTime) -> bool
                                    {
                                        return charPtr->AttackOnChara(DeltaTime, targetChara, inHitOffset);
                                    }
                                )
                            );
                            continue;
                        }
                    }

                    each->RemoveAllActions();
                    // 添加“ForceAttackOnChara”的 Action
                    each->CharacterActionQueue.Actions.push_back(
                        Action(ActionName,
                            // 捕获: 攻击者(each) + 目标(HitChara)
                            [charPtr = each, targetChara = HitChara, inHitOffset = HitOffset](float DeltaTime) -> bool
                            {
                                // AOrionChara::AttackOnChara(AOrionChara* InTarget) 返回 bool
                                return charPtr->AttackOnChara(DeltaTime, targetChara, inHitOffset);
                            }
                        )
                    );
                }
            }
        }
        // 2.3 如果点到的是 OrionActor
        else if (AOrionActor* HitActor = Cast<AOrionActor>(HitWorldActor))
        {

            // ★ 把它存起来，等 OnRightMouseUp 时再做区分：短按=Interact，长按=其他逻辑
            CachedRightClickedOrionActor = HitActor;

            // 不要立即执行 InteractWithActor，也不要清除角色 action
            // 因为可能要判断玩家究竟是不是长按。

            /*
            if (!OrionCharaSelection.empty())
            {
                for (auto& each : OrionCharaSelection)
                {
                    FString ActionName = FString::Printf(TEXT("ForceInteractWithActor|%s"), *HitActor->GetName());
                    each->RemoveAllActions();
                    each->CharacterActionQueue.Actions.push_back(
                        Action(ActionName,
                            [charPtr = each, targetActor = HitActor](float DeltaTime) -> bool
                            {
                                return charPtr->InteractWithActor(DeltaTime, targetActor);
                            }
                        )
                    );
                }
            }
            */
        }
        else
        {
            // 在点击位置播放特效
            if (NiagaraHitResultEffect)
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                    GetWorld(),
                    NiagaraHitResultEffect,
                    HitResult.Location
                );
            }

            // 让选中的角色移动过去
            if (!OrionCharaSelection.empty())
            {
                for (auto& each : OrionCharaSelection)
                {

                    if (each->CurrentAction)
                    {
                        if (each->CurrentAction->Name.Contains("MoveToLocation"))
                        {
                            each->RemoveAllActions("TempDoNotStopMovement");
                        }
                        else
                        {
                            each->RemoveAllActions();
                        }
                    }
                    else
                    {
                        each->RemoveAllActions();
                    }

                    each->CharacterActionQueue.Actions.push_back(
                        Action(TEXT("ForceMoveToLocation"),
                            [charPtr = each, location = HitResult.Location](float DeltaTime) -> bool
                            {
                                return charPtr->MoveToLocation(location);
                            }
                        )
                    );
                }
            }
            else
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