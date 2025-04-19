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
#include "Blueprint/AIBlueprintHelperLibrary.h"
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
#include "Engine/DamageEvents.h"

AOrionChara::AOrionChara()
{
    PrimaryActorTick.bCanEverTick = true;

    // 1) 当此角色被Spawn时，默认使用哪个控制器类来控制它
    AIControllerClass = AOrionAIController::StaticClass();

    // 让引擎在场景中放置或动态生成该角色时，自动让 AI Controller 来控制它
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;


    // 2) 创建StimuliSource
    StimuliSourceComp = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComp"));
    
    StimuliSourceComp->RegisterForSense(TSubclassOf<UAISense_Sight>());
    // StimuliSourceComp->RegisterForSense(UAISense_Hearing::StaticClass());

    StimuliSourceComp->bAutoRegister = true; // 自动注册

    // 3) Weapon equipment logics
    SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;

    CharacterActionQueue.Actions.reserve(100);

    
}

void AOrionChara::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("%s has been constructed."), *GetName());

    if (GetMesh())
    {
        DefaultMeshRelativeLocation = GetMesh()->GetRelativeLocation();
        DefaultMeshRelativeRotation = GetMesh()->GetRelativeRotation();
        DefaultCapsuleMeshOffset = GetCapsuleComponent()->GetComponentLocation() - GetMesh()->GetComponentLocation();
    }

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
    CurrHealth = MaxHealth;

    // Enable the character to receive damage
    this->SetCanBeDamaged(true);

}

void AOrionChara::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

	if (CharaState == ECharaState::Dead || CharaState == ECharaState::Incapacitated)
	{
		return;
	}

    
	if (CharaState == ECharaState::Ragdoll)
	{
        FName RootBone = GetMesh()->GetBoneName(0);
        FVector MeshRootLocation = GetMesh()->GetBoneLocation(RootBone);

        UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
        if (CapsuleComp)
        {
            FVector NewCapsuleLocation = MeshRootLocation + DefaultCapsuleMeshOffset;
            CapsuleComp->SetWorldLocation(NewCapsuleLocation);
        }


        if (RagdollWakeupAccumulatedTime >= RagdollWakeupThreshold)
        {
			RagdollWakeupAccumulatedTime = 0;
			RagdollWakeup();
		}
        else
        {
            RagdollWakeupAccumulatedTime += DeltaTime;
        }
	}

	ForceDetectionOnVelocityChange();

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

    if (CharaAIState == EAIState::AttackingEnemies)
    {
        if (CurrentAction == nullptr)
        {
			AOrionChara* TargetActor = GetClosestOrionChara();

            if (TargetActor)
            {
                FString ActionName = FString::Printf(TEXT("AttackOnCharaLongRange|%s"), *TargetActor->GetName());
                CharacterActionQueue.Actions.push_back(
                    Action(ActionName,
                        [charPtr = this, targetChara = TargetActor, inHitOffset = FVector()](float DeltaTime) -> bool
                        {
                            return charPtr->AttackOnChara(DeltaTime, targetChara, inHitOffset);
                        }
                    )
                );
            }
        }
    }

    PreviousTickIsAttackOnCharaLongRange = IsAttackOnCharaLongRange;
}

void AOrionChara::ForceDetectionOnVelocityChange()
{
    FVector CurrentVelocity = GetVelocity();
    float VelocityChange = (CurrentVelocity - PreviousVelocity).Size();

    if (VelocityChange > VelocityChangeThreshold)
    {
        // 认为受到了外力影响
        OnForceExceeded(CurrentVelocity - PreviousVelocity);
    }

    PreviousVelocity = CurrentVelocity;
}

bool AOrionChara::MoveToLocation(FVector InTargetLocation)
{
    /*
    static auto DoOnce = []() -> bool
        {
            static bool bHasExecuted = false;
            if (!bHasExecuted)
            {
                bHasExecuted = true;
                return true;
            }
            return false;
        };
    */

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
        
    /*
        if (DoOnce())
        {
            // 根据请求结果处理逻辑
            if (RequestResult == EPathFollowingRequestResult::RequestSuccessful)
            {
                UE_LOG(LogTemp, Log, TEXT("MoveToLocation called successfully with TargetLocation: %s"), *InTargetLocation.ToString());
            }
            else if (RequestResult == EPathFollowingRequestResult::Failed)
            {
                UE_LOG(LogTemp, Warning, TEXT("MoveToLocation failed for TargetLocation: %s"), *InTargetLocation.ToString());
            }
            else if (RequestResult == EPathFollowingRequestResult::AlreadyAtGoal)
            {
                UE_LOG(LogTemp, Log, TEXT("Already at TargetLocation: %s"), *InTargetLocation.ToString());
            }
        }
    */

    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AIController is not assigned!"));
    }

    FVector AjustedActorLocation = GetActorLocation();
    AjustedActorLocation.Z -= 85.f - 1.548;

    return FVector::Dist(AjustedActorLocation, InTargetLocation) < 60.0f;
}

void AOrionChara::MoveToLocationStop()
{
    AOrionAIController* OrionAC = Cast<AOrionAIController>(GetController());
    if (OrionAC)
    {
        OrionAC->StopMovement();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("MoveToLocationStop: OrionAC is nullptr or not of type AOrionAIController."));
    }
}

bool AOrionChara::InteractWithActor(float DeltaTime, AOrionActor* InTarget)
{

    if (!IsValid(InTarget))
    {
		UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: InTarget is not valid."));
        IsInteractWithActor = false;
        return true;
    }

    if (InTarget->ActorStatus == EActorStatus::NotInteractable)
    {
        UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: InTarget is not Interactable."));
        IsInteractWithActor = false;
        return true;
    }

    USphereComponent* CollisionSphere = InTarget->FindComponentByClass<USphereComponent>();

    if (CollisionSphere && CollisionSphere->IsOverlappingActor(this))
    {
		if (InTarget->GetName().Contains("BP_OrionDynamicActor"))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: Interacting with BP_OrionDynamicActor is now Discarded."), *this->GetName());
			//return PickUpItem(DeltaTime, InTarget);
            return true;
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

bool AOrionChara::AttackOnChara(float DeltaTime, AActor* InTarget, FVector HitOffset)
{

    if (!InTarget) return true;

    if (AOrionChara* TargetChara = Cast<AOrionChara>(InTarget))
    {
        if (TargetChara->CharaState == ECharaState::Incapacitated || TargetChara->CharaState == ECharaState::Dead)
        {
            //UOrionBPFunctionLibrary::OrionPrint("AttackOnChara: Target is Incapacitated. Stop attacking."); 
            IsAttackOnCharaLongRange = false;
            return true;
        }
    
        if (TargetChara == this)
        {
            //UOrionBPFunctionLibrary::OrionPrint("AttackOnChara: Attacking on oneself is not supported."); 
            IsAttackOnCharaLongRange = false;
            return true;
        }
    }

    return AttackOnCharaLongRange(DeltaTime, InTarget, HitOffset);
}

bool AOrionChara::AttackOnCharaLongRange(float DeltaTime, AActor* InTarget, FVector HitOffset)
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
			InTarget->GetActorLocation() + HitOffset + FVector(0, 0, 20.0f) // ADD AN ADDITIONAL OFFSET
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

void AOrionChara::SpawnBulletActor(const FVector& TargetLocation, float DeltaTime)
{

    SpawnBulletActorAccumulatedTime += DeltaTime;
    if (SpawnBulletActorAccumulatedTime > AttackFrequencyLongRange)
    {

        SpawnOrionBulletActor(TargetLocation, FVector((0, 0, 0)));
        SpawnBulletActorAccumulatedTime = 0;
    }
}

void AOrionChara::RemoveAllActions(const FString& Except)
{

    if (CurrentAction)
    {
        FString OngoingActionName = CurrentAction->Name;

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

        CurrentAction = nullptr;
    }

    CharacterActionQueue.Actions.clear();
}

void AOrionChara::SpawnArrowPenetrationEffect(const FVector& HitLocation, const FVector& HitNormal, UStaticMesh* ArrowMesh)
{
    if (!ArrowMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnArrowPenetrationEffect: ArrowMesh is null."));
        return;
    }

    std::unordered_set<std::string> BoneWhitelist = {
        "pelvis",       // Core lower body
        "spine_03",     // Upper torso
        "clavicle_l",   // Left shoulder
        "clavicle_r",   // Right shoulder
        "upperarm_l",   // Left upper arm
        "upperarm_r",   // Right upper arm
        "thigh_l",      // Left upper leg
        "thigh_r",      // Right upper leg
        "head"          // Self-explanatory
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


    // 计算箭矢的旋转方向
    FRotator ArrowRotation = HitNormal.Rotation();
    ArrowRotation += FRotator(0.f, 180.f, 0.f);
    //this->CurrHealth += 30;
    PenetratingArrowComponent->AttachToComponent(ClosestMeshComp, FAttachmentTransformRules::KeepWorldTransform, ClosestBoneName);
    PenetratingArrowComponent->SetWorldLocation(HitLocation);
    PenetratingArrowComponent->SetWorldRotation(ArrowRotation);


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
    float DamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    CurrHealth -= DamageApplied;

    UE_LOG(LogTemp, Log, TEXT("AOrionChara took %f damage. Current Health: %f"), DamageApplied, CurrHealth);

    // 检查 DamageEvent 是否为径向伤害事件
    /*
    if (DamageEvent.GetTypeID() == FRadialDamageEvent::ClassID)
    {
		UE_LOG(LogTemp, Log, TEXT("DamageEvent is a radial damage event."));
        // 将 DamageEvent 转换为 FRadialDamageEvent
        const FRadialDamageEvent& RadialDamageEvent = static_cast<const FRadialDamageEvent&>(DamageEvent);

        // 计算角色与爆炸点的距离
        float Distance = FVector::Dist(RadialDamageEvent.Origin, GetActorLocation());

        // 计算力的大小
        float MaxForce = DamageAmount; // 这里假设 DamageAmount 与最大力成正比
        float ForceMagnitude = FMath::Max(0.f, MaxForce * (1 - Distance / RadialDamageEvent.Params.OuterRadius));

        UE_LOG(LogTemp, Warning, TEXT("Calculated ForceMagnitude: %f at Distance: %f"), ForceMagnitude, Distance);

        // 如果力超过阈值，调用 OnForceExceeded
        if (ForceMagnitude > ForceThreshold)
        {
            // 计算力的方向
            FVector ForceDirection = GetActorLocation() - RadialDamageEvent.Origin;
            ForceDirection = ForceDirection.GetSafeNormal();

            // 调用 OnForceExceeded，传递力向量
            OnForceExceeded(ForceDirection * ForceMagnitude);
        }
    }
    */


    if (CurrHealth <= 0.0f)
    {
        Incapacitate();
    }

    return DamageApplied;
}

void AOrionChara::OnForceExceeded(const FVector& DeltaVelocity)
{
    float DeltaVSize = DeltaVelocity.Size();
    UE_LOG(LogTemp, Warning, TEXT("Force exceeded! Delta Velocity: %f"), DeltaVSize);

    float Mass = GetMesh()->GetMass();

    const float DeltaT = 0.0166f;

    // 计算近似的平均受力 F = m * Δv / Δt
    float ApproximatedForce = Mass * DeltaVSize / DeltaT / 20.0f;
    UE_LOG(LogTemp, Warning, TEXT("Approximated Impulse Force: %f"), ApproximatedForce);

    FVector CurrVel = GetVelocity();

    Ragdoll();

    // 计算当前速度

    GetMesh()->SetPhysicsLinearVelocity(CurrVel);

    FVector ImpulseToAdd = DeltaVelocity.GetSafeNormal() * ApproximatedForce * DeltaT;
    GetMesh()->AddImpulse(ImpulseToAdd, NAME_None, true);
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

void AOrionChara::Ragdoll()
{
    RemoveAllActions();

    //UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
    //if (CapsuleComp)
    //{
    //    CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    //    CapsuleComp->SetVisibility(false);
    //}

    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponentByClass(UPrimitiveComponent::StaticClass()));
    if (PrimitiveComponent)
    {
        PrimitiveComponent->SetSimulatePhysics(true);
    }

	CharaState = ECharaState::Ragdoll;

}

void AOrionChara::RagdollWakeup()
{
    CharaState = ECharaState::Alive;

	UE_LOG(LogTemp, Warning, TEXT("AOrionChara::RagdollWakeup() called."));


    //UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
    //if (CapsuleComp)
    //{
    //    CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    //    CapsuleComp->SetVisibility(true);
    //}

    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponentByClass(UPrimitiveComponent::StaticClass()));
    if (PrimitiveComponent)
    {
        PrimitiveComponent->SetSimulatePhysics(false);
    }

    // 将角色旋转调整为正立（保留当前 Yaw，仅将 Pitch 与 Roll 设为 0）
    FRotator CurrentRot = GetActorRotation();
    FRotator UprightRot(0.0f, CurrentRot.Yaw, 0.0f);
    SetActorRotation(UprightRot);

    // 重置 Skeletal Mesh 的相对位置和旋转至初始状态
    if (GetMesh())
    {
        // 如果当前 Mesh 还处于物理模拟状态下，其骨骼可能被物理计算改变，停止物理模拟后重新设置初始状态
        GetMesh()->SetRelativeLocation(DefaultMeshRelativeLocation);
        GetMesh()->SetRelativeRotation(DefaultMeshRelativeRotation);

        // 刷新骨骼状态，使其回到参考姿势
        GetMesh()->RefreshBoneTransforms();

        // 如果需要，还可以重置动画实例（强制刷新到动画蓝图中的默认 Idle 或站立状态）
        if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
        {
            AnimInst->InitializeAnimation();
        }
    }
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
			if (AOrionChara* Chara = Cast<AOrionChara>(OrionChara))
			{
				if (Chara->CharaSide == CharaSide)
				{
					continue;
				}

                FVector TargetLocation = Chara->GetActorLocation();
                float Distance = FVector::Dist(GetActorLocation(), TargetLocation);
                if (Distance < MinDistance)
                {
                    MinDistance = Distance;
                    ClosestOrionChara = Chara;
                }
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("%s is not a valid AOrionChara"), *OrionChara->GetName());
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

