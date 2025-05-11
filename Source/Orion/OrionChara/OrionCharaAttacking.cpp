#include "../OrionChara.h"
#include "../OrionProjectile.h"

#include "AIController.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include <unordered_set>


bool AOrionChara::AttackOnChara(float DeltaTime, AActor* InTarget, FVector HitOffset)
{
	if (!InTarget)
	{
		return true;
	}

	if (AOrionChara* TargetChara = Cast<AOrionChara>(InTarget))
	{
		if (TargetChara->CharaState == ECharaState::Incapacitated || TargetChara->CharaState == ECharaState::Dead)
		{
			AttackOnCharaLongRangeStop();
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
	HitOffset = FVector(0.f, 0.f, 0.0f);

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

		if (!GetVelocity().IsNearlyZero())
		{
			MoveToLocationStop();
		}

		AIController->StopMovement();

		FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
			GetActorLocation(),
			InTarget->GetActorLocation() + HitOffset
		);
		SetActorRotation(LookAtRot);

		if (!bIsEquipedWithWeapon)
		{
			if (bIsPlayingMontage)
			{
				return false;
			}
			PlayEquipWeaponMontage();
		}

		if (!bMontageReady2Attack)
		{
			return false;
		}


		// 1) 先累积时间，限频率
		SpawnBulletActorAccumulatedTime += DeltaTime;
		if (SpawnBulletActorAccumulatedTime < AttackFrequencyLongRange)
		{
			return false;
		}
		SpawnBulletActorAccumulatedTime = 0.f;

		// 2) 计算目标 Actor 的包围盒，并在盒体里取随机点
		FVector BoxOrigin, BoxExtent;
		InTarget->GetActorBounds(true, BoxOrigin, BoxExtent);

		// 如果想加上 Z 方向的偏移，比如击中头顶，可以把 HitOffset 也加进去
		FVector RandomHitPoint = UKismetMathLibrary::RandomPointInBoundingBox(
			BoxOrigin + HitOffset,
			BoxExtent
		);

		// 3) 计算发射点（Muzzle）和方向
		FVector MuzzleLoc = GetMesh()->GetSocketLocation(TEXT("MuzzleSocket"));

		FVector IdealDir = (RandomHitPoint - MuzzleLoc).GetSafeNormal();

		// 4) 角色层面的散布（Accuracy）
		float Accuracy = 1.0f;
		float MaxSpreadDeg = 1.f;
		float SpreadDeg = (1.f - Accuracy) * MaxSpreadDeg;
		FVector FinalDir = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(
			IdealDir,
			SpreadDeg
		);

		FinalDir.Z = 0.f; // 只在 XY 平面上发射

		// 5) 真正去 Spawn 子弹

		if (InventoryComp->GetItemQuantity(3) > 0)
		{
			InventoryComp->ModifyItemQuantity(3, -1);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AttackOnCharaLongRange: No ammo left."));

			if (IsAttackOnCharaLongRange)
			{
				AttackOnCharaLongRangeStop();
			}

			return true;
		}

		SpawnBulletActor(MuzzleLoc, FinalDir);

		return false;
	}
	// Not in range or line of sight blocked => move to a better position
	if (!bInRange)
	{
		UE_LOG(LogTemp, Log, TEXT("AttackOnCharaLongRange: Out of range. Moving to a better position."));
	}
	else if (!bLineOfSight)
	{
		UE_LOG(LogTemp, Log, TEXT("AttackOnCharaLongRange: line of sight blocked. Moving to a better position."));
	}

	//if (AIController)
	//{
	//	EPathFollowingRequestResult::Type RequestResult = AIController->MoveToLocation(
	//		InTarget->GetActorLocation(), 5.0f, true, true, true, false, nullptr, true);
	//}

	MoveToLocation(InTarget->GetActorLocation());

	//if (IsAttackOnCharaLongRange && !GetVelocity().IsNearlyZero())
	//{
	//	MoveToLocationStop();
	//}

	IsAttackOnCharaLongRange = false;
	return false;
}

void AOrionChara::SpawnBulletActor(const FVector& SpawnLoc, const FVector& DirToFire)
{
	SpawnOrionBulletActor(SpawnLoc, DirToFire);
}

void AOrionChara::SpawnArrowPenetrationEffect(const FVector& HitLocation, const FVector& HitNormal,
                                              UStaticMesh* ArrowMesh)
{
	if (!ArrowMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnArrowPenetrationEffect: ArrowMesh is null."));
		return;
	}

	std::unordered_set<std::string> BoneWhitelist = {
		"pelvis", // Core lower body
		"spine_03", // Upper torso
		"clavicle_l", // Left shoulder
		"clavicle_r", // Right shoulder
		"upperarm_l", // Left upper arm
		"upperarm_r", // Right upper arm
		"thigh_l", // Left upper leg
		"thigh_r", // Right upper leg
		"head" // Self-explanatory
	};

	// 收集当前角色的所有 SkeletalMeshComponent
	TArray<USkeletalMeshComponent*> AllSkeletalMeshes;
	GetComponents<USkeletalMeshComponent>(AllSkeletalMeshes);

	if (AllSkeletalMeshes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("SpawnArrowPenetrationEffect: No SkeletalMeshComponent found on this character."));
		return;
	}

	// 创建箭矢的 StaticMeshComponent
	UStaticMeshComponent* PenetratingArrowComponent = NewObject<UStaticMeshComponent>(
		this, UStaticMeshComponent::StaticClass());
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
			if (!BoneWhitelist.contains(BoneNameStr))
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
	PenetratingArrowComponent->AttachToComponent(ClosestMeshComp, FAttachmentTransformRules::KeepWorldTransform,
	                                             ClosestBoneName);
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

void AOrionChara::AttackOnCharaLongRangeStop()
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


	if (bIsEquipedWithWeapon)
	{
		RemoveWeaponActor();
		bIsEquipedWithWeapon = false;
	}

	bMontageReady2Attack = false;
	IsAttackOnCharaLongRange = false;
	SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
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
	if (!AM_EquipWeapon)
	{
		return;
	}
	UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
	if (!AnimInst)
	{
		return;
	}

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
				SpawnWeaponActor();
				bIsEquipedWithWeapon = true;
			},
			0.44 * Duration,
			false
		);

		// 2) 在结束前 0.5s 安排攻击逻辑

		float TriggerTime = FMath::Max(0.f, Duration - 0.15f);
		GetWorld()->GetTimerManager().SetTimer(
			EquipMontageTriggerHandle,
			[this]()
			{
				IsAttackOnCharaLongRange = true;
				bMontageReady2Attack = true;
			},
			TriggerTime,
			false
		);
	}
}

void AOrionChara::OnEquipWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 1) 清除所有相关定时器
	GetWorld()->GetTimerManager().ClearTimer(EquipMontageSpawnHandle);
	GetWorld()->GetTimerManager().ClearTimer(EquipMontageTriggerHandle);

	// 2) 重置状态，允许下一次蒙太奇和定时器
	bIsPlayingMontage = false;

	if (bInterrupted)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipWeaponMontage interrupted."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("EquipWeaponMontage ended."));
	}
}

float AOrionChara::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
                              AActor* DamageCauser)
{
	const float DamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	CurrHealth -= DamageApplied;

	//UE_LOG(LogTemp, Log, TEXT("AOrionChara took %f damage. Current Health: %f"), DamageApplied, CurrHealth);

	/*
	// 检查 DamageEvent 是否为径向伤害事件
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

void AOrionChara::RefreshAttackFrequency()
{
	if (IsAttackOnCharaLongRange != PreviousTickIsAttackOnCharaLongRange)
	{
		SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
	}

	PreviousTickIsAttackOnCharaLongRange = IsAttackOnCharaLongRange;
}
