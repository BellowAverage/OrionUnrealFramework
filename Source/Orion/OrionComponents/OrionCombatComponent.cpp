#include "OrionCombatComponent.h"
#include "Orion/OrionComponents/OrionMovementComponent.h"

#include <string>

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"
#include "Orion/OrionWeaponProjectile/OrionProjectile.h"
#include "TimerManager.h"

#include <unordered_set>

UOrionCombatComponent::UOrionCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOrionCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
}

void UOrionCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	for (UStaticMeshComponent* Comp : AttachedArrowComponents)
	{
		if (Comp && Comp->IsValidLowLevel())
		{
			Comp->DestroyComponent();
		}
	}
	AttachedArrowComponents.Empty();
}

AOrionChara* UOrionCombatComponent::GetOrionOwner() const
{
	return Cast<AOrionChara>(GetOwner());
}

bool UOrionCombatComponent::AttackOnChara(float DeltaTime, AActor* InTarget, FVector HitOffset)
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

		if (TargetChara == GetOrionOwner())
		{
			IsAttackOnCharaLongRange = false;
			return true;
		}
	}

	return AttackOnCharaLongRange(DeltaTime, InTarget, HitOffset);
}

bool UOrionCombatComponent::AttackOnCharaLongRange(float DeltaTime, AActor* InTarget, FVector HitOffset)
{
	AOrionChara* Owner = GetOrionOwner();
	if (!Owner || !Owner->AIController || !InTarget)
	{
		return true;
	}

	HitOffset = FVector::ZeroVector;

	const float DistToTarget = FVector::Dist(Owner->GetActorLocation(), InTarget->GetActorLocation() + HitOffset);
	const bool bInRange = (DistToTarget <= FireRange);
	const bool bLineOfSight = bInRange ? HasClearLineOfSight(InTarget) : false;

	if (bInRange && bLineOfSight)
	{
		IsAttackOnCharaLongRange = true;

		if (!Owner->GetVelocity().IsNearlyZero())
		{
			if (Owner->MovementComp) Owner->MovementComp->MoveToLocationStop();
		}

		Owner->AIController->StopMovement();

		const FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
			Owner->GetActorLocation(),
			InTarget->GetActorLocation() + HitOffset
		);
		Owner->SetActorRotation(LookAtRot);

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

		SpawnBulletActorAccumulatedTime += DeltaTime;
		if (SpawnBulletActorAccumulatedTime < AttackFrequencyLongRange)
		{
			return false;
		}
		SpawnBulletActorAccumulatedTime = 0.f;

		FVector BoxOrigin, BoxExtent;
		InTarget->GetActorBounds(true, BoxOrigin, BoxExtent);

		const FVector RandomHitPoint = UKismetMathLibrary::RandomPointInBoundingBox(
			BoxOrigin + HitOffset,
			BoxExtent
		);

		const FVector MuzzleLoc = Owner->GetMesh()->GetSocketLocation(TEXT("MuzzleSocket"));
		const FVector IdealDir = (RandomHitPoint - MuzzleLoc).GetSafeNormal();

		const float Accuracy = 1.0f;
		const float MaxSpreadDeg = 1.f;
		const float SpreadDeg = (1.f - Accuracy) * MaxSpreadDeg;
		FVector FinalDir = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(
			IdealDir,
			SpreadDeg
		);
		FinalDir.Z = 0.f;

		if (Owner->InventoryComp)
		{
			if (Owner->InventoryComp->GetItemQuantity(3) > 0)
			{
				Owner->InventoryComp->ModifyItemQuantity(3, -1);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AttackOnCharaLongRange: No ammo left."));
				AttackOnCharaLongRangeStop();
				return true;
			}
		}

		SpawnBulletActor(MuzzleLoc, FinalDir);

		return false;
	}

	if (!bInRange)
	{
		UE_LOG(LogTemp, Log, TEXT("AttackOnCharaLongRange: Out of range. Moving to a better position."));
	}
	else if (!bLineOfSight)
	{
		UE_LOG(LogTemp, Log, TEXT("AttackOnCharaLongRange: line of sight blocked. Moving to a better position."));
	}

	if (Owner->MovementComp) Owner->MovementComp->MoveToLocation(InTarget->GetActorLocation());

	IsAttackOnCharaLongRange = false;
	return false;
}

void UOrionCombatComponent::AttackOnCharaLongRangeStop()
{
	AOrionChara* Owner = GetOrionOwner();

	if (Owner)
	{
		if (UAnimInstance* AnimInst = Owner->GetMesh()->GetAnimInstance())
		{
			if (AM_EquipWeapon && AnimInst->Montage_IsPlaying(AM_EquipWeapon))
			{
				AnimInst->Montage_Stop(0.1f, AM_EquipWeapon);
				AnimInst->OnMontageEnded.RemoveDynamic(this, &UOrionCombatComponent::OnEquipWeaponMontageEnded);
			}
		}

		if (bIsEquipedWithWeapon)
		{
			Owner->RemoveWeaponActor();
			bIsEquipedWithWeapon = false;
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EquipMontageSpawnHandle);
		World->GetTimerManager().ClearTimer(EquipMontageTriggerHandle);
	}

	bMontageReady2Attack = false;
	IsAttackOnCharaLongRange = false;
	SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
}

void UOrionCombatComponent::RefreshAttackFrequency()
{
	if (IsAttackOnCharaLongRange != PreviousTickIsAttackOnCharaLongRange)
	{
		SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
	}

	PreviousTickIsAttackOnCharaLongRange = IsAttackOnCharaLongRange;
}

void UOrionCombatComponent::SpawnBulletActor(const FVector& SpawnLoc, const FVector& DirToFire)
{
	if (AOrionChara* Owner = GetOrionOwner())
	{
		Owner->SpawnOrionBulletActor(SpawnLoc, DirToFire);
	}
}

bool UOrionCombatComponent::HasClearLineOfSight(AActor* InTargetActor) const
{
	AOrionChara* Owner = GetOrionOwner();
	if (!Owner || !InTargetActor)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(AttackTrace), true);
	TraceParams.AddIgnoredActor(Owner);
	TraceParams.AddIgnoredActor(InTargetActor);
	for (TActorIterator<AOrionProjectile> It(Owner->GetWorld()); It; ++It)
	{
		TraceParams.AddIgnoredActor(*It);
	}

	const bool bHit = Owner->GetWorld()->LineTraceSingleByChannel(
		Hit,
		Owner->GetActorLocation(),
		InTargetActor->GetActorLocation(),
		ECC_Visibility,
		TraceParams
	);

	if (bHit)
	{
		if (Hit.GetActor())
		{
			UE_LOG(LogTemp, Warning, TEXT("AttackOnCharaLongRange: Line of sight blocked by %s."),
			       *Hit.GetActor()->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AttackOnCharaLongRange: Line of sight blocked by an unknown object."));
		}
		return false;
	}

	return true;
}

void UOrionCombatComponent::PlayEquipWeaponMontage()
{
	AOrionChara* Owner = GetOrionOwner();
	if (!Owner || !AM_EquipWeapon)
	{
		return;
	}

	UAnimInstance* AnimInst = Owner->GetMesh()->GetAnimInstance();
	if (!AnimInst)
	{
		return;
	}

	AnimInst->OnMontageEnded.RemoveDynamic(this, &UOrionCombatComponent::OnEquipWeaponMontageEnded);
	AnimInst->OnMontageEnded.AddDynamic(this, &UOrionCombatComponent::OnEquipWeaponMontageEnded);

	bIsPlayingMontage = true;
	const float Rate = AnimInst->Montage_Play(AM_EquipWeapon, 1.f);
	UE_LOG(LogTemp, Log, TEXT("PlayEquipMontage Rate=%.2f"), Rate);

	if (Rate > 0.f)
	{
		const float Duration = AM_EquipWeapon->GetPlayLength() / Rate;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				EquipMontageSpawnHandle,
				[this, Owner]()
				{
					Owner->SpawnWeaponActor();
					bIsEquipedWithWeapon = true;
				},
				0.44f * Duration,
				false
			);

			const float TriggerTime = FMath::Max(0.f, Duration - 0.15f);
			World->GetTimerManager().SetTimer(
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
}

void UOrionCombatComponent::OnEquipWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EquipMontageSpawnHandle);
		World->GetTimerManager().ClearTimer(EquipMontageTriggerHandle);
	}

	bIsPlayingMontage = false;

	if (bInterrupted)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipWeaponMontage interrupted."));
		OnEquipWeaponMontageInterrupted();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("EquipWeaponMontage ended."));
	}
}

void UOrionCombatComponent::OnEquipWeaponMontageInterrupted()
{
	AttackOnCharaLongRangeStop();
}

void UOrionCombatComponent::SpawnArrowPenetrationEffect(const FVector& HitLocation, const FVector& HitNormal,
                                                        UStaticMesh* ArrowMesh)
{
	AOrionChara* Owner = GetOrionOwner();
	if (!ArrowMesh || !Owner)
	{
		return;
	}

	std::unordered_set<std::string> BoneWhitelist = {
		"pelvis",
		"spine_03",
		"clavicle_l",
		"clavicle_r",
		"upperarm_l",
		"upperarm_r",
		"thigh_l",
		"thigh_r",
		"head"
	};

	TArray<USkeletalMeshComponent*> AllSkeletalMeshes;
	Owner->GetComponents(AllSkeletalMeshes);
	if (AllSkeletalMeshes.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("SpawnArrowPenetrationEffect: No SkeletalMeshComponent found on this character."));
		return;
	}

	UStaticMeshComponent* PenetratingArrowComponent = NewObject<UStaticMeshComponent>(
		Owner, UStaticMeshComponent::StaticClass());
	if (!PenetratingArrowComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnArrowPenetrationEffect: Failed to create ArrowComponent."));
		return;
	}

	FName ClosestBoneName = NAME_None;
	USkeletalMeshComponent* ClosestMeshComp = nullptr;
	float MinDistance = FLT_MAX;

	for (USkeletalMeshComponent* SkelMesh : AllSkeletalMeshes)
	{
		if (!SkelMesh || !SkelMesh->GetSkeletalMeshAsset())
		{
			continue;
		}

		const FReferenceSkeleton& RefSkeleton = SkelMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
		const int32 BoneCount = RefSkeleton.GetNum();

		for (int32 i = 0; i < BoneCount; ++i)
		{
			const FName BoneName = RefSkeleton.GetBoneName(i);
			const std::string BoneNameStr = TCHAR_TO_UTF8(*BoneName.ToString());
			if (!BoneWhitelist.contains(BoneNameStr))
			{
				continue;
			}

			const FVector BoneWorldLocation = SkelMesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);
			const float Dist = FVector::Dist(BoneWorldLocation, HitLocation);

			if (Dist < MinDistance)
			{
				MinDistance = Dist;
				ClosestBoneName = BoneName;
				ClosestMeshComp = SkelMesh;
			}
		}
	}

	if (ClosestBoneName == NAME_None || !ClosestMeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnArrowPenetrationEffect: No bone found in any SkeletalMeshComponent."));
		PenetratingArrowComponent->DestroyComponent();
		return;
	}

	PenetratingArrowComponent->SetStaticMesh(ArrowMesh);
	PenetratingArrowComponent->RegisterComponent();
	PenetratingArrowComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PenetratingArrowComponent->SetVisibility(true);
	PenetratingArrowComponent->SetMobility(EComponentMobility::Movable);
	PenetratingArrowComponent->SetSimulatePhysics(false);

	FRotator ArrowRotation = HitNormal.Rotation();
	ArrowRotation += FRotator(0.f, 180.f, 0.f);

	PenetratingArrowComponent->AttachToComponent(ClosestMeshComp, FAttachmentTransformRules::KeepWorldTransform,
	                                             ClosestBoneName);
	PenetratingArrowComponent->SetWorldLocation(HitLocation);
	PenetratingArrowComponent->SetWorldRotation(ArrowRotation);

	AttachedArrowComponents.Add(PenetratingArrowComponent);

	UE_LOG(LogTemp, Log, TEXT("SpawnArrowPenetrationEffect: Attached arrow to bone [%s] of mesh [%s]."),
	       *ClosestBoneName.ToString(),
	       *ClosestMeshComp->GetName());

	if (UWorld* World = GetWorld())
	{
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(
			TimerHandle,
			[PenetratingArrowComponent]()
			{
				if (PenetratingArrowComponent && PenetratingArrowComponent->IsValidLowLevel())
				{
					PenetratingArrowComponent->DestroyComponent();
				}
			},
			30.0f,
			false
		);
	}
}

