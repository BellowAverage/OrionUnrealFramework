// OrionChara.cpp

#include "OrionChara.h"
#include "OrionAIController.h"
#include "OrionWeapon.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"
#include "Navigation/PathFollowingComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h" // 引入用于SimpleMoveToLocation的头文件
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "OrionBPFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "OrionProjectile.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "TimerManager.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include <Components/SphereComponent.h>
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

AOrionChara::AOrionChara()
{
    PrimaryActorTick.bCanEverTick = true;

    // 1) 当此角色被Spawn时，默认使用哪个控制器类来控制它
    AIControllerClass = AOrionAIController::StaticClass();

    // 让引擎在场景中放置或动态生成该角色时，自动让 AI Controller 来控制它
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;


    // 2) 创建StimuliSource
    StimuliSourceComp = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComp"));
    
    // 注册成可被“视觉”感知
    StimuliSourceComp->RegisterForSense(TSubclassOf<UAISense_Sight>());
    // StimuliSourceComp->RegisterForSense(UAISense_Hearing::StaticClass());

    StimuliSourceComp->bAutoRegister = true; // 自动注册

    // 3) Weapon equipment logics
    SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;

    
}

// Called when the game starts or when spawned
void AOrionChara::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("%s has been constructed."), *GetName());

    // 1) 尝试获取当前的 AIController
    AIController = Cast<AOrionAIController>(GetController());
    if (AIController)
    {
        UE_LOG(LogTemp, Log, TEXT("OrionChara is now controlled by OrionAIController"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("OrionChara is not controlled by OrionAIController"));
        
        // 检查是否设置了 AIControllerClass
        if (AIControllerClass)
        {
            UE_LOG(LogTemp, Log, TEXT("AIControllerClass is set to: %s"), *AIControllerClass->GetName());

            // 获取世界上下文
            UWorld* World = GetWorld();
            if (World)
            {
                // 设置 Spawn 参数
                FActorSpawnParameters SpawnParams;
                SpawnParams.Instigator = GetInstigator();
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                UE_LOG(LogTemp, Log, TEXT("Attempting to spawn AIController of class: %s"), *AIControllerClass->GetName());

                // 使用 SpawnActor 生成 AIController，不再使用 Cast<>()，因为 SpawnActor<AOrionAIController> 已返回正确类型
                AIController = World->SpawnActor<AOrionAIController>(AIControllerClass, SpawnParams);
                if (AIController)
                {
                    // 让新的 AIController 控制这个角色
                    AIController->Possess(this);
                    UE_LOG(LogTemp, Log, TEXT("OrionAIController has been spawned and possessed the OrionChara."));
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to spawn OrionAIController. Skipping related logic."));
                    // 转换失败，跳过相关逻辑
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("World context is invalid. Skipping related logic."));
                // 世界上下文无效，跳过相关逻辑
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("AIControllerClass is not set. Please assign it in the constructor or editor."));
            // AIControllerClass 未设置，跳过相关逻辑
        }
    }

    // 2) 获取 CharacterMovementComponent
    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    if (MovementComponent)
    {
        // Don't rotate character to camera direction
        bUseControllerRotationPitch = false;
        bUseControllerRotationYaw = false;
        bUseControllerRotationRoll = false;

        // Configure character movement
        MovementComponent->bOrientRotationToMovement = true; // Rotate character to moving direction
        MovementComponent->RotationRate = FRotator(0.f, 270.f, 0.f);
        MovementComponent->bConstrainToPlane = true;
        MovementComponent->bSnapToPlaneAtStart = true;

        MovementComponent->MaxWalkSpeed = 300.0f; // 设置移动速度为 300 单位/秒

    }

    // Initialize default health values
    MaxHealth = 100.0f;
    CurrHealth = MaxHealth;

    // Enable the character to receive damage
    this->SetCanBeDamaged(true);

}

void AOrionChara::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // UE_LOG(LogTemp, Log, TEXT("DoOnceInteractWithActor: %s"), DoOnceInteractWithActor ? TEXT("True") : TEXT("False"));

    //FVector Velocity = GetCharacterMovement()->Velocity;
    //if (Velocity.Size() > 0 && Velocity.Size() < 400.0f)
    //{
    //    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    //    if (MovementComponent)
    //    {
    //        MovementComponent->MaxWalkSpeed += 5;
    //    }
    //}

    //if (Velocity.Size() == 0)
    //{
    //    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    //    if (MovementComponent)
    //    {
    //        MovementComponent->MaxWalkSpeed = 10;
    //    }
    //}

	if (CharaState == ECharaState::Dead || CharaState == ECharaState::Incapacitated)
	{
		return;
	}

    if (CurrentAction == nullptr && !CharacterActionQueue.IsEmpty())
    {
        CurrentAction = CharacterActionQueue.GetNextAction();
    }

    if (CurrentAction)
    {
        bool ActionComplete = CurrentAction->ExecuteFunction(DeltaTime);

        if (ActionComplete)
        {
            CharacterActionQueue.RemoveCurrentAction();
            CurrentAction = nullptr;
        }
    }

    if (IsAttackOnCharaLongRange != PreviousTickIsAttackOnCharaLongRange)
    {
        SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
        if (!PreviousTickIsAttackOnCharaLongRange)
        {
            // Spawn BP_OrionWeapon on the Character's hand
            SpawnWeaponActor();
        }
        else
        {
			// Destroy BP_OrionWeapon
            RemoveWeaponActor();
        }

    }

    PreviousTickIsAttackOnCharaLongRange = IsAttackOnCharaLongRange;
}

void AOrionChara::AddActionToQueue(const FString& ActionName, EActionType ActionType)
{
    // 根据 ActionType 映射到具体的执行函数
    std::function<bool(float)> Func;

    switch (ActionType)
    {
    case EActionType::MoveToLocation:
        Func = [this, ActionName](float DeltaTime) -> bool {
            UE_LOG(LogTemp, Log, TEXT("Executing Action: %s"), *ActionName);
            return MoveToLocation(TargetLocation1);
        };
        break;

    case EActionType::AttackOnCharaLongRange:
        Func = [this, ActionName](float DeltaTime) -> bool {
            UE_LOG(LogTemp, Log, TEXT("Executing Action: %s"), *ActionName);
			return AttackOnChara(DeltaTime, GetClosestOrionChara(), FVector(0, 0, 0));
        };
        break;

    case EActionType::WorkingTest:
        Func = [this, ActionName](float DeltaTime) -> bool {
            UE_LOG(LogTemp, Log, TEXT("Executing Action: %s"), *ActionName);
            return WorkingTest(DeltaTime);
            };
        break;

    default:
        UE_LOG(LogTemp, Warning, TEXT("Unknown Action Type: %d"), static_cast<uint8>(ActionType));
        return;
    }

    Action NewAction(ActionName, Func);
    CharacterActionQueue.Actions.push_back(NewAction);
}

// Called to bind functionality to input
void AOrionChara::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
}

bool AOrionChara::MoveToLocation(FVector InTargetLocation)
{
    //static auto DoOnce = []() -> bool
    //    {
    //        static bool bHasExecuted = false;
    //        if (!bHasExecuted)
    //        {
    //            bHasExecuted = true;
    //            return true;
    //        }
    //        return false;
    //    };

    if (AIController)
    {
        FNavAgentProperties AgentProperties = AIController->GetNavAgentPropertiesRef();

        UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        FNavLocation ProjectedLocation;
        if (NavSys && NavSys->ProjectPointToNavigation(InTargetLocation, ProjectedLocation))
        {
            // ProjectedLocation.Location 就是投影到可行走区域后的坐标，Z 值将是地面或 NavMesh 的真实高度
            InTargetLocation = ProjectedLocation.Location;
        }

        EPathFollowingRequestResult::Type RequestResult = AIController->MoveToLocation(InTargetLocation, 5.0f, true, true, true, false, 0, true);
        
        //if (DoOnce())
        //{
        //    // 根据请求结果处理逻辑
        //    if (RequestResult == EPathFollowingRequestResult::RequestSuccessful)
        //    {
        //        UE_LOG(LogTemp, Log, TEXT("MoveToLocation called successfully with TargetLocation: %s"), *InTargetLocation.ToString());
        //    }
        //    else if (RequestResult == EPathFollowingRequestResult::Failed)
        //    {
        //        UE_LOG(LogTemp, Warning, TEXT("MoveToLocation failed for TargetLocation: %s"), *InTargetLocation.ToString());
        //    }
        //    else if (RequestResult == EPathFollowingRequestResult::AlreadyAtGoal)
        //    {
        //        UE_LOG(LogTemp, Log, TEXT("Already at TargetLocation: %s"), *InTargetLocation.ToString());
        //    }
        //}

    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AIController is not assigned!"));
    }

    //FString TempDebugMessage = FString::SanitizeFloat(FVector::Dist(GetActorLocation(), InTargetLocation));
    //UOrionBPFunctionLibrary::OrionPrint(TempDebugMessage);

    FVector AjustedActorLocation = GetActorLocation();
    AjustedActorLocation.Z -= 85.f - 1.548;
    //FVector GapV = AjustedActorLocation - InTargetLocation;
    //FString TempDebugMessage = AjustedActorLocation.ToString() + InTargetLocation.ToString() + FString::SanitizeFloat(FVector::Dist(AjustedActorLocation, InTargetLocation));
    //FString TempDebugMessage = GapV.ToString() + " | " + FString::SanitizeFloat(FVector::Dist(AjustedActorLocation, InTargetLocation));
    //UOrionBPFunctionLibrary::OrionPrint(TempDebugMessage);
    return FVector::Dist(AjustedActorLocation, InTargetLocation) < 60.0f;
}

void AOrionChara::MoveToLocationStop()
{
    AOrionAIController* OrionAC = Cast<AOrionAIController>(GetController());
    if (OrionAC)
    {
        OrionAC->StopMovement();
        //UE_LOG(LogTemp, Log, TEXT("Movement stopped successfully."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("MoveToLocationStop: OrionAC is nullptr or not of type AOrionAIController."));
    }
}

bool AOrionChara::WorkingTest(float DeltaTime)
{
    WorkingTestAccumulatedTime += DeltaTime;

    IsWorkingTest = true;

    if (WorkingTestAccumulatedTime >= 5.0f)
    {
        IsWorkingTest = false;
        WorkingTestAccumulatedTime = 0.0f;
        return true;
    }

    return false;
}

bool AOrionChara::PickUpItem(float DeltaTime, AOrionActor* InTarget)
{
    // 如果动画蒙太奇未播放，且 InTarget 有效
    if (!bMontagePlaying && PickupMontage && InTarget)
    {
        // 获取动画实例
        AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            // 播放动画蒙太奇
            AnimInstance->Montage_Play(PickupMontage);
            bMontagePlaying = true;
            bMontageFinished = false;

        }
    }

    // 如果动画蒙太奇正在播放
    if (bMontagePlaying && AnimInstance)
    {
        // 检查蒙太奇是否仍在播放
        if (!AnimInstance->Montage_IsPlaying(PickupMontage))
        {
            // 如果动画播放完成，返回 true
            bMontagePlaying = false;
            bMontageFinished = true;

            OrionPickup(InTarget);

            // 设置角色状态为 Carrying
            CharaState = ECharaState::Carrying;
			InTarget->Destroy();
            return true;
        }
    }

    // 如果动画尚未完成，返回 false
    return false;
}

bool AOrionChara::InteractWithActor(float DeltaTime, AOrionActor* InTarget)
{
	if (this->CharaState == ECharaState::Carrying)
	{
		UOrionBPFunctionLibrary::OrionPrint("InteractWithActor: Cannot interact while carrying an item.");
		return true;
	}

    if (!InTarget)
    {
        IsInteractWithActor = false;
        return true;
    }

    USphereComponent* CollisionSphere = InTarget->FindComponentByClass<USphereComponent>();

    if (CollisionSphere && CollisionSphere->IsOverlappingActor(this))
    {
		if (InTarget->GetName().Contains("BP_OrionDynamicActor"))
		{
			UE_LOG(LogTemp, Log, TEXT("%s: Interacting with BP_OrionDynamicActor. "), *this->GetName());
			return PickUpItem(DeltaTime, InTarget);
		}

        IsInteractWithActor = true;

		// Determine the type of interaction

		FString TargetInteractType = InTarget->GetInteractType();
        if (TargetInteractType == "Mining")
        {
			InteractType = EInteractType::Mining;
        }
        else
        {
			InteractType = EInteractType::Unavailable;
			UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: Unavailable interact type."));
			IsInteractWithActor = false;
			return true;
        }

		if (!DoOnceInteractWithActor)
		{
			DoOnceInteractWithActor = true;
            CurrentInteractActor = InTarget;
            InTarget->CurrWorkers += 1;
		}

        if (AIController)
        {
            AIController->StopMovement();
            //UE_LOG(LogTemp, Log, TEXT("StopMovement Called. "));
        }

        // 计算朝向目标的旋转角度
        FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
            GetActorLocation(),
            InTarget->GetActorLocation()
        );

        // 忽略高度轴上的旋转
        LookAtRot.Pitch = 0.0f;
        LookAtRot.Roll = 0.0f;

        // 设置角色的旋转
        SetActorRotation(LookAtRot);

        return false;
    }
    else
    {
        if (AIController)
        {
            UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
            if (NavSys)
            {
                FNavLocation ProjectedLocation; // 修改为 FNavLocation

                // 定义搜索范围
                FVector SearchExtent(500.0f, 500.0f, 500.0f);

                // 尝试将目标点投影到导航网格上
                if (NavSys->ProjectPointToNavigation(
                    InTarget->GetActorLocation(),  // 原始目标位置
                    ProjectedLocation,            // 输出投影后的可导航点
                    SearchExtent                   // 搜索范围
                ))
                {
                    // 使用投影点作为目标位置
                    EPathFollowingRequestResult::Type RequestResult = AIController->MoveToLocation(
                        ProjectedLocation.Location,  // 使用 ProjectedLocation.Location
                        20.0f,       // 容忍距离（单位：厘米）
                        true,       // 停止于重叠
                        true,       // 使用路径寻路
                        true,       // 投影到可导航区域
                        false,      // 不允许平移移动
                        0,          // 无自定义路径过滤器
                        true        // 允许部分路径
                    );

                    if (RequestResult == EPathFollowingRequestResult::Failed)
                    {
                        //UE_LOG(LogTemp, Warning, TEXT("MoveToLocation failed even after projection!"));
                    }
                    else
                    {
                        //UE_LOG(LogTemp, Log, TEXT("MoveToLocation request successful."));
                    }
                }
                else
                {
                    //UE_LOG(LogTemp, Warning, TEXT("Failed to project point to navigation!"));
                }
            }
            else
            {
                //UE_LOG(LogTemp, Error, TEXT("Navigation system is not available!"));
            }
        }

        IsInteractWithActor = false;
        return false;
    }

}

void AOrionChara::InteractWithActorStop()
{
    if (!CurrentInteractActor) // 已经清理过了
    {
        return;
    }

    IsInteractWithActor = false;
	DoOnceInteractWithActor = false;
	CurrentInteractActor->CurrWorkers -= 1;
	CurrentInteractActor = nullptr;
	InteractType = EInteractType::Unavailable;
}

bool AOrionChara::AttackOnChara(float DeltaTime, AOrionChara* InTarget, FVector HitOffset)
{

	if (this->CharaState == ECharaState::Carrying)
	{
		UOrionBPFunctionLibrary::OrionPrint("AttackOnChara: Cannot attack while carrying an item.");
		return true;
	}

    // 检查目标是否还存在
    if (!InTarget)
    {
        UOrionBPFunctionLibrary::OrionPrint("AttackOnChara: Target is invalid. Stop attacking.");
        IsAttackOnCharaLongRange = false;
        return true;
    }

    if (InTarget->CharaState == ECharaState::Incapacitated || InTarget->CharaState == ECharaState::Dead)
    {
        UOrionBPFunctionLibrary::OrionPrint("AttackOnChara: Target is Incapacitated. Stop attacking.");
        IsAttackOnCharaLongRange = false;
        return true;
    }
    
    // 不允许攻击自己
    if (InTarget == this)
    {
        UOrionBPFunctionLibrary::OrionPrint("AttackOnChara: Attacking on oneself is not supported.");
        IsAttackOnCharaLongRange = false;
        return true;
    }

    return AttackOnCharaLongRange(DeltaTime, InTarget, HitOffset);

    
}

void AOrionChara::SpawnBulletActor(const FVector& TargetLocation, float DeltaTime)
{

    SpawnBulletActorAccumulatedTime += DeltaTime;
    if (SpawnBulletActorAccumulatedTime > AttackFrequencyLongRange)
    {

		SpawnOrionBulletActor(TargetLocation, FVector((0, 0, 0)));
        SpawnBulletActorAccumulatedTime = 0;
    }
}

bool AOrionChara::AttackOnCharaLongRange(float DeltaTime, AOrionChara* InTarget, FVector HitOffset)
{   
    // 3. 检测射程&遮挡
    float DistToTarget = FVector::Dist(GetActorLocation(), InTarget->GetActorLocation() + HitOffset);

    bool bInRange = (DistToTarget <= FireRange);

    // 遮挡判断：线 Trace
    bool bLineOfSight = false;
    //DrawDebugLine(GetWorld(), GetActorLocation(), InTarget->GetActorLocation() + HitOffset, FColor::Green, false, 5.0f, 2.0f);

    if (bInRange)
    {
        FHitResult Hit;
        FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(AttackTrace), true);
        TraceParams.AddIgnoredActor(this);
        TraceParams.AddIgnoredActor(InTarget);

        // 忽略所有 AOrionProjectile 的子类
        for (TActorIterator<AOrionProjectile> It(GetWorld()); It; ++It)
        {
            TraceParams.AddIgnoredActor(*It);
        }


        bool bHit = GetWorld()->LineTraceSingleByChannel(
            Hit,
            GetActorLocation(),
            InTarget->GetActorLocation() + HitOffset,
            ECC_Visibility,
            TraceParams
        );

        if (bHit)
        {
            bLineOfSight = false; // 有阻挡
            if (Hit.GetActor())
            {
                UE_LOG(LogTemp, Warning, TEXT("AttackOnCharaLongRange: Line of sight blocked by %s."),
                    *Hit.GetActor()->GetName());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("AttackOnCharaLongRange: Line of sight blocked by an unknown object."));
            }
        }
        else
        {
            bLineOfSight = true; // 未撞到任何阻挡 =>无遮挡
        }

        //bLineOfSight = !bHit; // 未撞到任何阻挡 =>无遮挡
    }

    // 4. 判断能否直接射击
    if (bInRange && bLineOfSight)
    {
        // 4.1 连续射击
        
        IsAttackOnCharaLongRange = true;

        AIController->StopMovement();

        // 2. 先让角色面向目标 (每帧都朝向，以便目标移动时持续追踪)
        FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
            GetActorLocation(),
            InTarget->GetActorLocation() + HitOffset
        );
        SetActorRotation(LookAtRot);

        SpawnBulletActor(InTarget->GetActorLocation() + HitOffset, DeltaTime);

        return false;
    }
    else
    {
        // 5. 不在射程 or 有遮挡 => 先找一个可射击位置，然后插入“移动”动作
        if (!bInRange)
        {
			UE_LOG(LogTemp, Log, TEXT("AttackOnCharaLongRange: Out of range. Moving to a better position."));
        }
        else if (!bLineOfSight)
        {
            UE_LOG(LogTemp, Log, TEXT("AttackOnCharaLongRange: line of sight blocked. Moving to a better position."));
        }

        if (AIController)
        {
            EPathFollowingRequestResult::Type RequestResult = AIController->MoveToLocation(InTarget->GetActorLocation(), 5.0f, true, true, true, false, 0, true);
        }

        IsAttackOnCharaLongRange = false;
        return false;
    }
}

void AOrionChara::AttackOnCharaLongRangeStop()
{
	IsAttackOnCharaLongRange = false;
    SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
}

void AOrionChara::RemoveAllActions(const FString& Except)
{
    //FString OngoingActionNameBeforeCleared;
    //if (CurrentAction)
    //{
    //    OngoingActionNameBeforeCleared = CurrentAction->Name;
    //}


    //if (OngoingActionNameBeforeCleared.Contains("ForceMoveToLocation") || OngoingActionNameBeforeCleared.Contains("MoveToLocation"))
    //{
    //    if (!Except.Contains("TempDoNotStopMovement"))
    //    {
    //        MoveToLocationStop();
    //    }
    //    else
    //    {

    //    }

    //}

    //if (OngoingActionNameBeforeCleared.Contains("ForceAttackOnCharaLongRange") || OngoingActionNameBeforeCleared.Contains("AttackOnCharaLongRange"))
    //{
    //    AttackOnCharaLongRangeStop();
    //}

    //if (OngoingActionNameBeforeCleared.Contains("ForceInteractWithActor") || OngoingActionNameBeforeCleared.Contains("InteractWithActor"))
    //{
    //    InteractWithActorStop();
    //}

    //CharacterActionQueue.Actions.clear();

	// ==========================

    // 如果有正在执行的动作，先把动作名称记录下来
    if (CurrentAction)
    {
        FString OngoingActionName = CurrentAction->Name;

        // 根据动作名称，先调用相应的 Stop 函数
        if (OngoingActionName.Contains("ForceMoveToLocation") || OngoingActionName.Contains("MoveToLocation"))
        {
            if (!Except.Contains("TempDoNotStopMovement"))
            {
                MoveToLocationStop();
            }
        }

        if (OngoingActionName.Contains("ForceAttackOnCharaLongRange") || OngoingActionName.Contains("AttackOnCharaLongRange"))
        {
            AttackOnCharaLongRangeStop();
        }

        if (OngoingActionName.Contains("ForceInteractWithActor") || OngoingActionName.Contains("InteractWithActor"))
        {
            InteractWithActorStop();
        }

        // 停止完之后，把 CurrentAction 置空，防止后续误用
        CurrentAction = nullptr;
    }

    // 现在再清空动作队列
    CharacterActionQueue.Actions.clear();
}

/* discarded: used blueprintImplementableEvent instead
//void AOrionChara::SpawnWeaponActor()
//{
//    if (WeaponClass)
//    {
//        FVector SocketLocation = GetMesh()->GetSocketLocation("LeftHandWeaponSocket");
//        FRotator SocketRotation = GetMesh()->GetSocketRotation("LeftHandWeaponSocket");
//
//        FActorSpawnParameters SpawnParams;
//        SpawnParams.Owner = this;
//        SpawnParams.Instigator = GetInstigator();
//
//        // Spawn 一个 AOrionWeapon 实例，使用在编辑器中指定的具体子类（例如BP_OrionWeapon）
//        AOrionWeapon* SpawnedWeapon = GetWorld()->SpawnActor<AOrionWeapon>(
//            WeaponClass,
//            FTransform(SocketRotation, SocketLocation),
//            SpawnParams
//        );
//
//        CurrentWeapon = SpawnedWeapon;
//
//        if (SpawnedWeapon)
//        {
//            // 将武器附加到角色的骨骼上
//            SpawnedWeapon->AttachToComponent(
//                GetMesh(),
//                FAttachmentTransformRules::SnapToTargetNotIncludingScale,
//                "LeftHandWeaponSocket"
//            );
//
//            UE_LOG(LogTemp, Log, TEXT("Weapon spawned and attached to LeftHandWeaponSocket"));
//        }
//        else
//        {
//            UE_LOG(LogTemp, Warning, TEXT("Failed to spawn weapon."));
//        }
//    }
//    else
//    {
//        UE_LOG(LogTemp, Warning, TEXT("WeaponClass is not set! Please assign a valid BP_OrionWeapon class in editor/blueprint."));
//    }
//
//
//}

//void AOrionChara::RemoveWeaponActor()
//{
//    if (CurrentWeapon)
//    {
//        // 从世界中销毁武器
//        CurrentWeapon->Destroy();
//        CurrentWeapon = nullptr;
//    }
//}

*/

void AOrionChara::SpawnArrowPenetrationEffect(const FVector& HitLocation, const FVector& HitNormal, UStaticMesh* ArrowMesh)
{
    if (!ArrowMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnArrowPenetrationEffect: ArrowMesh is null."));
        return;
    }

    std::unordered_set<std::string> BoneWhitelist = {
        "root",
        "pelvis",
        "spine_01",
        "spine_02",
        "spine_03",
        "spine_04",
        "spine_05",
        "neck_01",
        "neck_02",
        "head",
        "clavicle_l",
        "upperarm_l",
        "lowerarm_l",
        "lowerarm_twist_02_l",
        "lowerarm_twist_01_l",
        "hand_l",
        "middle_metacarpal_l",
        "middle_01_l",
        "middle_02_l",
        "middle_03_l",
        "pinky_metacarpal_l",
        "pinky_01_l",
        "pinky_02_l",
        "pinky_03_l",
        "ring_metacarpal_l",
        "ring_01_l",
        "ring_02_l",
        "ring_03_l",
        "thumb_01_l",
        "thumb_02_l",
        "thumb_03_l",
        "index_metacarpal_l",
        "index_01_l",
        "index_02_l",
        "index_03_l",
        "upperarm_twist_01_l",
        "upperarm_twist_02_l",
        "clavicle_r",
        "upperarm_r",
        "lowerarm_r",
        "lowerarm_twist_02_r",
        "lowerarm_twist_01_r",
        "hand_r",
        "middle_metacarpal_r",
        "middle_01_r",
        "middle_02_r",
        "middle_03_r",
        "pinky_metacarpal_r",
        "pinky_01_r",
        "pinky_02_r",
        "pinky_03_r",
        "ring_metacarpal_r",
        "ring_01_r",
        "ring_02_r",
        "ring_03_r",
        "thumb_01_r",
        "thumb_02_r",
        "thumb_03_r",
        "index_metacarpal_r",
        "index_01_r",
        "index_02_r",
        "index_03_r",
        "upperarm_twist_01_r",
        "upperarm_twist_02_r",
        "thigh_r",
        "calf_r",
        "foot_r",
        "ball_r",
        "calf_twist_02_r",
        "calf_twist_01_r",
        "thigh_twist_01_r",
        "thigh_twist_02_r",
        "thigh_l",
        "calf_l",
        "foot_l",
        "ball_l",
        "calf_twist_02_l",
        "calf_twist_01_l",
        "thigh_twist_01_l",
        "thigh_twist_02_l",
        "ik_foot_root",
        "ik_foot_l",
        "ik_foot_r",
        "ik_hand_root",
        "ik_hand_gun",
        "ik_hand_l",
        "ik_hand_r",
        "interaction",
        "center_of_mass"
    };

    // 收集当前角色的所有 SkeletalMeshComponent
    TArray<USkeletalMeshComponent*> AllSkeletalMeshes;
    GetComponents<USkeletalMeshComponent>(AllSkeletalMeshes);

    if (AllSkeletalMeshes.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnArrowPenetrationEffect: No SkeletalMeshComponent found on this character."));
        return;
    }

    // 创建箭矢的 StaticMeshComponent
    UStaticMeshComponent* PenetratingArrowComponent = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass());
    if (!PenetratingArrowComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnArrowPenetrationEffect: Failed to create ArrowComponent."));
        return;
    }

    // 用于记录最优骨骼（距离最近）
    FName ClosestBoneName = NAME_None;
    USkeletalMeshComponent* ClosestMeshComp = nullptr;
    float MinDistance = FLT_MAX;

    // 遍历所有 SkeletalMeshComponent
    for (USkeletalMeshComponent* SkelMesh : AllSkeletalMeshes)
    {
        if (!SkelMesh || !SkelMesh->GetSkeletalMeshAsset())
        {
            continue;
        }

        // 获取骨骼参考
        const FReferenceSkeleton& RefSkeleton = SkelMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
        const int32 BoneCount = RefSkeleton.GetNum();

        // 遍历该组件上的所有骨骼，寻找白名单 & 距离最近的骨骼
        for (int32 i = 0; i < BoneCount; ++i)
        {
            FName BoneName = RefSkeleton.GetBoneName(i);

            // 转成 std::string 检查是否在白名单
            std::string BoneNameStr = TCHAR_TO_UTF8(*BoneName.ToString());
            if (BoneWhitelist.count(BoneNameStr) == 0)
            {
                // 不在白名单就跳过
                continue;
            }

            // 获取该骨骼的世界空间位置
            FVector BoneWorldLocation = SkelMesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);
            float Dist = FVector::Dist(BoneWorldLocation, HitLocation);

            // 比较是否更近
            if (Dist < MinDistance)
            {
                MinDistance = Dist;
                ClosestBoneName = BoneName;
                ClosestMeshComp = SkelMesh;
            }
        }
    }

    // 如果没有找到合适的骨骼，退出
    if (ClosestBoneName == NAME_None || !ClosestMeshComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnArrowPenetrationEffect: No bone found in any SkeletalMeshComponent."));
        PenetratingArrowComponent->DestroyComponent();
        return;
    }

    // 设置箭矢的网格
    PenetratingArrowComponent->SetStaticMesh(ArrowMesh);

    // 使组件在渲染和物理上可用
    PenetratingArrowComponent->RegisterComponent();
    PenetratingArrowComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PenetratingArrowComponent->SetVisibility(true);
    PenetratingArrowComponent->SetMobility(EComponentMobility::Movable);
    PenetratingArrowComponent->SetSimulatePhysics(false);

    // 把箭矢附着到找到的 SkeletalMeshComponent 上
    // 这里的规则 KeepWorldTransform 表示保持我们已经设置好的世界位置
    // 你也可以根据需求改用 SnapToTarget 等规则
    if (this->GetName().Contains("BP_XChara"))
    {

        // 计算箭矢的旋转方向
        FRotator ArrowRotation = HitNormal.Rotation();
        ArrowRotation += FRotator(0.f, 180.f, 0.f);
        //this->CurrHealth += 30;
        PenetratingArrowComponent->AttachToComponent(ClosestMeshComp, FAttachmentTransformRules::KeepWorldTransform, ClosestBoneName);
        PenetratingArrowComponent->SetWorldLocation(HitLocation);
        PenetratingArrowComponent->SetWorldRotation(ArrowRotation);
    }
    else
    {
        FRotator ArrowRotation = HitNormal.Rotation();
        ArrowRotation += FRotator(0.f, 180.f, 0.f);
        //this->CurrHealth += 30;
        PenetratingArrowComponent->AttachToComponent(ClosestMeshComp, FAttachmentTransformRules::KeepWorldTransform, ClosestBoneName);
        PenetratingArrowComponent->SetWorldLocation(HitLocation);
        PenetratingArrowComponent->SetWorldRotation(ArrowRotation);
    }


    // 把这个组件存起来，以便以后销毁或管理
    AttachedArrowComponents.Add(PenetratingArrowComponent);

    // 打印一下我们最终附着的结果
    UE_LOG(LogTemp, Log, TEXT("SpawnArrowPenetrationEffect: Attached arrow to bone [%s] of mesh [%s]."),
        *ClosestBoneName.ToString(),
        *ClosestMeshComp->GetName());

    // 启动一个定时器，在 10 秒后自动销毁箭矢
    if (GetWorld())
    {
        FTimerHandle TimerHandle;
        // 这里用 [WeakObjectPtr or Raw pointer capture] 即可
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle,
            [PenetratingArrowComponent]()
            {
                if (PenetratingArrowComponent && PenetratingArrowComponent->IsValidLowLevel())
                {
                    PenetratingArrowComponent->DestroyComponent();
                }
            },
            30.0f,
            false // 不循环
        );
    }
}


float AOrionChara::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // Call the base class - may adjust damage
    float DamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // Subtract damage from current health
    CurrHealth -= DamageApplied;

    UE_LOG(LogTemp, Log, TEXT("AOrionChara took %f damage. Current Health: %f"), DamageApplied, CurrHealth);

    // Check if health is zero or below
    if (CurrHealth <= 0.0f)
    {
        Incapacitate();
    }

    return DamageApplied;
}

void AOrionChara::Die()
{
	CharaState = ECharaState::Dead;

    UE_LOG(LogTemp, Log, TEXT("AOrionChara::Die() called."));

    // 1. 停止所有动作
    RemoveAllActions();

    // 2. 停止AI感知
    if (StimuliSourceComp)
    {
        // Ensure that the sense class is valid
        if (UAISense_Sight::StaticClass())
        {
            StimuliSourceComp->UnregisterFromSense(UAISense_Sight::StaticClass());
            UE_LOG(LogTemp, Log, TEXT("Unregistered from UAISense_Sight."));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("UAISense_Sight::StaticClass() returned nullptr."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("StimuliSourceComp is null in Die()."));
    }

    // 3. 停止AI控制
    if (AIController)
    {
        AIController->StopMovement();
        AIController->UnPossess();
        UE_LOG(LogTemp, Log, TEXT("AIController stopped and unpossessed."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AIController is null in Die()."));
    }

    if (AttachedArrowComponents.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("No attached arrow components to destroy."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Cleaning up %d attached arrow components."), AttachedArrowComponents.Num());

    for (auto& ArrowComp : AttachedArrowComponents)
    {
        if (ArrowComp && ArrowComp->IsValidLowLevel())
        {
            ArrowComp->DestroyComponent();
            UE_LOG(LogTemp, Log, TEXT("Destroyed an attached arrow component."));
        }
    }

    // Clear the array after destruction
    AttachedArrowComponents.Empty();

    Destroy();
    UE_LOG(LogTemp, Log, TEXT("AOrionChara destroyed."));
}

void AOrionChara::Incapacitate()
{
	CharaState = ECharaState::Incapacitated;

	// 1. 停止所有动作
	RemoveAllActions();
	// 2. 停止AI感知
	if (StimuliSourceComp)
	{
		// Ensure that the sense class is valid
		if (UAISense_Sight::StaticClass())
		{
			StimuliSourceComp->UnregisterFromSense(UAISense_Sight::StaticClass());
			UE_LOG(LogTemp, Log, TEXT("Unregistered from UAISense_Sight."));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UAISense_Sight::StaticClass() returned nullptr."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("StimuliSourceComp is null in Die()."));
	}
	// 3. 停止AI控制
	if (AIController)
	{
		AIController->StopMovement();
		AIController->UnPossess();
		UE_LOG(LogTemp, Log, TEXT("AIController stopped and unpossessed."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AIController is null in Die()."));
	}

    // Inside AOrionChara::BeginPlay() or wherever appropriate
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponentByClass(UPrimitiveComponent::StaticClass()));
    if (PrimitiveComponent)
    {
        PrimitiveComponent->SetSimulatePhysics(true);
    }

    // Disable capsule collision
    UCapsuleComponent* CharaCapsuleComponent = GetCapsuleComponent();
    if (CharaCapsuleComponent)
    {
        CharaCapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

AOrionChara* AOrionChara::GetClosestOrionChara()
{
    TArray<AActor*> AllOrionCharas;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionChara::StaticClass(), AllOrionCharas);
    AOrionChara* ClosestOrionChara = nullptr;
    float MinDistance = std::numeric_limits<float>::max();
    for (AActor* OrionChara : AllOrionCharas)
    {
        if (OrionChara != this)
        {
            FVector TargetLocation = OrionChara->GetActorLocation();
            float Distance = FVector::Dist(GetActorLocation(), TargetLocation);
            if (Distance < MinDistance)
            {
                MinDistance = Distance;
                ClosestOrionChara = Cast<AOrionChara>(OrionChara);
            }
        }
    }
    if (ClosestOrionChara)
    {
        UE_LOG(LogTemp, Log, TEXT("%s: ClosestOrionChara is %s"), *GetName(), *ClosestOrionChara->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: No other OrionChara found"), *GetName());
    }
    return ClosestOrionChara;
}

void AOrionChara::AddItemToInventory(int ItemID, int Quantity)
{
	if (CharaInventoryMap.contains(ItemID))
	{
		CharaInventoryMap[ItemID] += Quantity;
	}
	else
	{
		CharaInventoryMap[ItemID] = Quantity;
	}
}