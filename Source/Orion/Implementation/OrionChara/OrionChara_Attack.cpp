#include "../OrionChara.h"
#include "../OrionProjectile.h"

#include "AIController.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"


bool AOrionChara::AttackOnChara(float DeltaTime, AActor* InTarget, FVector HitOffset)
{

    if (!InTarget) return true;

    if (AOrionChara* TargetChara = Cast<AOrionChara>(InTarget))
    {
        if (TargetChara->CharaState == ECharaState::Incapacitated || TargetChara->CharaState == ECharaState::Dead)
        {
            IsAttackOnCharaLongRange = false;
            return true;
        }
    
        if (TargetChara == this)
        {
            IsAttackOnCharaLongRange = false;
            return true;
        }
    }

    return AttackOnCharaLongRange(DeltaTime, InTarget, HitOffset);
}

bool AOrionChara::AttackOnCharaLongRange(float DeltaTime, AActor* InTarget, FVector HitOffset)
{   
	// Check fire range and blockage
    float DistToTarget = FVector::Dist(GetActorLocation(), InTarget->GetActorLocation() + HitOffset);
    bool bInRange = (DistToTarget <= FireRange);

    bool bLineOfSight = false;
    // DrawDebugLine(GetWorld(), GetActorLocation(), InTarget->GetActorLocation() + HitOffset, FColor::Green, false, 5.0f, 2.0f);

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
			bLineOfSight = false; // blocked
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
			bLineOfSight = true; // not blocked
        }
    }

	// Check if shooting is possible

    if (bInRange && bLineOfSight)
    {
        
        IsAttackOnCharaLongRange = true;

        AIController->StopMovement();

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
		// Not in range or line of sight blocked => move to a better position
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
    /*
    // 1) 如果装备武器的蒙太奇还在播，立即停止它
    if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
    {
        if (AM_EquipWeapon && AnimInst->Montage_IsPlaying(AM_EquipWeapon))
        {
            // 0.1 秒内 Blend Out 停止
            AnimInst->Montage_Stop(0.1f, AM_EquipWeapon);

            // 同时解绑结束回调，防止之后触发
            AnimInst->OnMontageEnded.RemoveDynamic(this, &AOrionChara::OnEquipWeaponMontageEnded);
        }
    }

    // 2) 清理两个定时器，避免在蒙太奇停止后还触发 Spawn 或 Attack 逻辑
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(EquipMontageSpawnHandle);
        GetWorld()->GetTimerManager().ClearTimer(EquipMontageTriggerHandle);
    }

    // 3) 原有逻辑
    */

    IsAttackOnCharaLongRange = false;
    SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
    RemoveWeaponActor();
}

void AOrionChara::OnEquipWeaponMontageInterrupted()
{
    // 1) 如果装备武器的蒙太奇还在播，立即停止它
    if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
    {
        if (AM_EquipWeapon && AnimInst->Montage_IsPlaying(AM_EquipWeapon))
        {
            // 0.1 秒内 Blend Out 停止
            AnimInst->Montage_Stop(0.1f, AM_EquipWeapon);

            // 同时解绑结束回调，防止之后触发
            AnimInst->OnMontageEnded.RemoveDynamic(this, &AOrionChara::OnEquipWeaponMontageEnded);
        }
    }

    // 2) 清理两个定时器，避免在蒙太奇停止后还触发 Spawn 或 Attack 逻辑
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(EquipMontageSpawnHandle);
        GetWorld()->GetTimerManager().ClearTimer(EquipMontageTriggerHandle);
    }

    AttackOnCharaLongRangeStop();
}

void AOrionChara::PlayEquipWeaponMontage()
{
    if (!AM_EquipWeapon) return;
    UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
    if (!AnimInst) return;

    // 只留结束回调来清定时器
    AnimInst->OnMontageEnded.RemoveDynamic(this, &AOrionChara::OnEquipWeaponMontageEnded);
    AnimInst->OnMontageEnded.AddDynamic(this, &AOrionChara::OnEquipWeaponMontageEnded);

    // 播放蒙太奇

	bIsPlayingMontage = true;

    float Rate = AnimInst->Montage_Play(AM_EquipWeapon, 1.f);
    UE_LOG(LogTemp, Log, TEXT("PlayEquipMontage Rate=%.2f"), Rate);

    if (Rate > 0.f)
    {
        float Duration = AM_EquipWeapon->GetPlayLength() / Rate;

        // 1) 在开始后 0.5s 生成武器
        GetWorld()->GetTimerManager().SetTimer(
            EquipMontageSpawnHandle,
            [this]()
            {
                MontageCallbackChara->SpawnWeaponActor();
            },
            0.44 * Duration,
            false
        );

        // 2) 在结束前 0.5s 安排攻击逻辑

        float TriggerTime = FMath::Max(0.f, Duration - 0.2f);
        GetWorld()->GetTimerManager().SetTimer(
            EquipMontageTriggerHandle,
            [this]()
            {
                MontageCallbackChara->IsAttackOnCharaLongRange = true;
                MontageCallbackChara->CharacterActionQueue.Actions.push_back(
                    Action(MontageCallbackActionName,
                        [charPtr = MontageCallbackChara,
                        targetChara = MontageCallbackTargetActor,
                        inHitOffset = MontageCallbackHitOffset](float DeltaTime) -> bool
                        {
                            return charPtr->AttackOnChara(DeltaTime, targetChara, inHitOffset);
                        }
                    )
                );
            },
            TriggerTime,
            false
        );
    }
}

void AOrionChara::OnEquipWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // 清除两个定时器，防止蒙太奇意外中断后仍触发
    GetWorld()->GetTimerManager().ClearTimer(EquipMontageSpawnHandle);
    GetWorld()->GetTimerManager().ClearTimer(EquipMontageTriggerHandle);

    if (bInterrupted)
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipWeaponMontage interrupted."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("EquipWeaponMontage ended."));
    }

    UE_LOG(LogTemp, Log, TEXT("MontageCallbackChara: %s"), *MontageCallbackChara->GetName());
}