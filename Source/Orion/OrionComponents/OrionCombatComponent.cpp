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
#include "Orion/OrionComponents/OrionAttributeComponent.h"
#include "Orion/OrionWeaponProjectile/OrionProjectile.h"
#include "TimerManager.h"

#include <unordered_set>

// ============================================================================
// Static Global Weapon Data Storage
// ============================================================================

TMap<EOrionWeaponType, FOrionWeaponParams> UOrionCombatComponent::GlobalWeaponParamsMap;

void UOrionCombatComponent::InitializeGlobalWeaponParams()
{
	if (GlobalWeaponParamsMap.Num() > 0)
	{
		return; // Already initialized
	}

	// Initialize Rifle params
	FOrionWeaponParams RifleParams;
	RifleParams.IsWeaponLongRange = true;
	RifleParams.FireRange = 2000.0f;
	RifleParams.AttackFrequencyLongRange = 0.15f;
	RifleParams.AttackTriggerTimeLongRange = 0.5f;
	RifleParams.PelletsCount = 1;
	RifleParams.SpreadAngle = 0.f;
	GlobalWeaponParamsMap.Add(EOrionWeaponType::Rifle, RifleParams);

	// Initialize Shotgun params
	FOrionWeaponParams ShotgunParams;
	ShotgunParams.IsWeaponLongRange = true;
	ShotgunParams.FireRange = 200.f;
	ShotgunParams.AttackFrequencyLongRange = 1.0f;
	ShotgunParams.AttackTriggerTimeLongRange = 0.5f;
	ShotgunParams.PelletsCount = 15;
	ShotgunParams.SpreadAngle = 15.0f;
	GlobalWeaponParamsMap.Add(EOrionWeaponType::Shotgun, ShotgunParams);
}

// ============================================================================
// UOrionGameplayAbility
// ============================================================================

UOrionGameplayAbility::UOrionGameplayAbility()
{
	bIsActive = false;
}

void UOrionGameplayAbility::Initialize(UOrionCombatComponent* InComponent)
{
	Component = InComponent;
}

bool UOrionGameplayAbility::CanActivate() const
{
	return Component != nullptr;
}

void UOrionGameplayAbility::Activate()
{
	bIsActive = true;
}

void UOrionGameplayAbility::Tick(float DeltaTime)
{
	// Base implementation does nothing
}

void UOrionGameplayAbility::EndAbility()
{
	bIsActive = false;
}

void UOrionGameplayAbility::SetTarget(AActor* InTarget)
{
	TargetActor = InTarget;
}

UWorld* UOrionGameplayAbility::GetWorld() const
{
	return Component ? Component->GetWorld() : nullptr;
}

// ============================================================================
// UOrionGameplayAbility_AutoAttack
// ============================================================================

void UOrionGameplayAbility_AutoAttack::Initialize(UOrionCombatComponent* InComponent)
{
	Super::Initialize(InComponent);
	// Initialize accumulated time to allow immediate fire if ready, similar to Component BeginPlay
	if (Component)
	{
		const FOrionWeaponParams& Params = Component->GetCurrentWeaponParams();
		SpawnBulletActorAccumulatedTime = Params.AttackFrequencyLongRange - Params.AttackTriggerTimeLongRange;
	}
}

void UOrionGameplayAbility_AutoAttack::Activate()
{
	Super::Activate();
	// When activated, we might want to reset some transient states if needed,
	// but for AutoAttack which is ticked continuously, we mostly rely on Tick.
}

void UOrionGameplayAbility_AutoAttack::EndAbility()
{
	Super::EndAbility();

	if (!Component) return;

	AOrionChara* Owner = Cast<AOrionChara>(Component->GetOwner());
	if (Owner)
	{
		if (UAnimInstance* AnimInst = Owner->GetMesh()->GetAnimInstance())
		{
			if (Component->AM_EquipWeapon && AnimInst->Montage_IsPlaying(Component->AM_EquipWeapon))
			{
				AnimInst->Montage_Stop(0.1f, Component->AM_EquipWeapon);
				AnimInst->OnMontageEnded.RemoveDynamic(this, &UOrionGameplayAbility_AutoAttack::OnEquipWeaponMontageEnded);
			}
		}

		// [Fix] Instead of removing weapon immediately, start a timer to remove it later.
		// This allows seamless target switching without re-equipping.
		// 只有在真正停止攻击时才设置卸载定时器（5秒后卸载武器）
		// 注意：如果在这5秒内重新开始攻击，定时器会在 Tick() 中被清除
		if (Component->IsEquippedWithWeapon)
		{
			if (UWorld* World = GetWorld())
			{
				// Clear any existing unequip timer first
				World->GetTimerManager().ClearTimer(Component->UnequipWeaponTimerHandle);
				
				TWeakObjectPtr<UOrionCombatComponent> WeakComp(Component);
				TWeakObjectPtr<AOrionChara> WeakOwner(Owner);

				World->GetTimerManager().SetTimer(
					Component->UnequipWeaponTimerHandle,
					[WeakComp, WeakOwner]()
					{
						if (WeakComp.IsValid() && WeakOwner.IsValid())
						{
							// [Fix] 在定时器触发时，再次检查是否还在攻击
							// 如果还在攻击，说明在5秒内重新开始了攻击，不卸载武器
							if (WeakComp->IsAttackOnCharaLongRange)
							{
								// 如果还在攻击，不卸载武器（定时器应该已经被清除，但这里作为双重保险）
								return;
							}
							
							// 只有在没有攻击时才卸载武器
							if (WeakComp->IsEquippedWithWeapon)
							{
								WeakOwner->RemoveWeaponActor();
								WeakComp->IsEquippedWithWeapon = false;
							}
						}
					},
					5.0f, // 5 seconds delay before holstering weapon - 只在没有攻击时才开始计时
					false
				);
			}
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EquipMontageSpawnHandle);
		World->GetTimerManager().ClearTimer(EquipMontageTriggerHandle);
	}

	Component->IsMontageReady2Attack = false;
	Component->IsPlayingMontage = false; 
	Component->IsAttackOnCharaLongRange = false;
	
	// Reset cooldown
	const FOrionWeaponParams& Params = Component->GetCurrentWeaponParams();
	SpawnBulletActorAccumulatedTime = Params.AttackFrequencyLongRange - Params.AttackTriggerTimeLongRange;
}

void UOrionGameplayAbility_AutoAttack::Tick(float DeltaTime)
{
	// Logic copied and adapted from OrionCombatComponent::AttackOnCharaLongRange
	
	if (!Component) return;
	AOrionChara* Owner = Cast<AOrionChara>(Component->GetOwner());
	if (!Owner || !Owner->OrionAIControllerInstance || !TargetActor)
	{
		return;
	}

	// Basic Validation
	// [Refactor] Use AttributeComponent to check validity and life
	const UOrionAttributeComponent* AttrComp = TargetActor->FindComponentByClass<UOrionAttributeComponent>();
	if (!AttrComp)
	{
		// Target not valid for attack
		EndAbility();
		return;
	}

	if (!AttrComp->IsAlive())
	{
		EndAbility();
		return;
	}

	// Specific check for self-target
	if (TargetActor == Owner)
	{
		Component->IsAttackOnCharaLongRange = false;
		return;
	}

	// Specific check for Chara state if it IS a chara (AttributeComp handles health, but CharaState handles animation/logic state)
	if (AOrionChara* TargetChara = Cast<AOrionChara>(TargetActor))
	{
		if (TargetChara->CharaState == ECharaState::Incapacitated || TargetChara->CharaState == ECharaState::Dead)
		{
			EndAbility();
			return;
		}
	}

	FVector HitOffset = FVector::ZeroVector; // Simplified for now, could be passed via SetTarget params if needed
	
	const FOrionWeaponParams& Params = Component->GetCurrentWeaponParams();

	const float DistToTarget = FVector::Dist(Owner->GetActorLocation(), TargetActor->GetActorLocation() + HitOffset);
	const bool bInRange = (DistToTarget <= Params.FireRange);
	const bool bLineOfSight = bInRange ? HasClearLineOfSight(TargetActor) : false;

	if (bInRange && bLineOfSight)
	{
		Component->IsAttackOnCharaLongRange = true;

		// [Fix] 在攻击过程中，清除卸载武器的定时器，防止在攻击时武器被卸载
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(Component->UnequipWeaponTimerHandle);
		}

		if (!Owner->GetVelocity().IsNearlyZero())
		{
			if (Owner->MovementComp) Owner->MovementComp->MoveToLocationStop();
		}

		Owner->OrionAIControllerInstance->StopMovement();

		const FVector OwnerLoc = Owner->GetActorLocation();
		FVector TargetLoc = TargetActor->GetActorLocation() + HitOffset;
		// 将目标高度强制拉平到自身高度，避免角色在攻击时产生俯仰
		TargetLoc.Z = OwnerLoc.Z;

		const FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(OwnerLoc, TargetLoc);
		FRotator FlatRot = LookAtRot;
		FlatRot.Pitch = 0.f;
		FlatRot.Roll = 0.f;
		Owner->SetActorRotation(FlatRot);

		if (!Component->IsEquippedWithWeapon)
		{
			if (Component->IsPlayingMontage)
			{
				return;
			}
			PlayEquipWeaponMontage();
		}
		else
		{
			// [Fix] If weapon is equipped (e.g. from previous target), ensure we are ready to attack immediately
			// unless we are in the middle of equipping (handled by IsPlayingMontage)
			if (!Component->IsMontageReady2Attack && !Component->IsPlayingMontage)
			{
				Component->IsMontageReady2Attack = true;
			}
		}

		if (!Component->IsMontageReady2Attack)
		{
			return;
		}

		SpawnBulletActorAccumulatedTime += DeltaTime;
		if (SpawnBulletActorAccumulatedTime < Params.AttackFrequencyLongRange)
		{
			return;
		}
		SpawnBulletActorAccumulatedTime = 0.f;

		// Use target center for precise aiming, spread will be handled by Weapon::Fire
		const FVector TargetCenter = TargetActor->GetActorLocation() + HitOffset;

		if (Owner->InventoryComp)
		{
			if (Owner->InventoryComp->GetItemQuantity(3) > 0)
			{
				Owner->InventoryComp->ModifyItemQuantity(3, -1);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Ability_AutoAttack: No ammo left."));
				EndAbility();
				return;
			}
		}

		// Spread and Pellet count handled by Weapon::Fire via CombatComponent::SpawnBulletActor
		Component->CalculateAndSpawnBulletActor(TargetCenter);

		return;
	}

	if (!bInRange)
	{
		// UE_LOG(LogTemp, Log, TEXT("Ability_AutoAttack: Out of range. Moving to a better position."));
	}
	else if (!bLineOfSight)
	{
		// UE_LOG(LogTemp, Log, TEXT("Ability_AutoAttack: line of sight blocked. Moving to a better position."));
	}

	if (Owner->MovementComp) Owner->MovementComp->MoveToLocation(TargetActor->GetActorLocation());

	Component->IsAttackOnCharaLongRange = false;
}

bool UOrionGameplayAbility_AutoAttack::HasClearLineOfSight(AActor* InTargetActor) const
{
	if (!Component) return false;
	const AOrionChara* Owner = Cast<AOrionChara>(Component->GetOwner());
	if (!Owner || !InTargetActor) return false;

	FHitResult Hit;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(AttackTrace), true);
	TraceParams.AddIgnoredActor(Owner);
	TraceParams.AddIgnoredActor(InTargetActor);
	
	// Assuming Component has access to world, Iterate projectiles
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
		return false;
	}

	return true;
}

void UOrionGameplayAbility_AutoAttack::PlayEquipWeaponMontage()
{
	if (!Component) return;
	AOrionChara* Owner = Cast<AOrionChara>(Component->GetOwner());
	if (!Owner || !Component->AM_EquipWeapon) return;

	// [Fix] Clear unequip timer if we start equipping/attacking
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Component->UnequipWeaponTimerHandle);
	}

	// [Fix] If already equipped (e.g. from delayed holster), skip montage
	if (Component->IsEquippedWithWeapon)
	{
		Component->IsPlayingMontage = false;
		Component->IsMontageReady2Attack = true;
		Component->IsAttackOnCharaLongRange = true;
		return;
	}

	UAnimInstance* AnimInst = Owner->GetMesh()->GetAnimInstance();
	if (!AnimInst) return;

	AnimInst->OnMontageEnded.RemoveDynamic(this, &UOrionGameplayAbility_AutoAttack::OnEquipWeaponMontageEnded);
	AnimInst->OnMontageEnded.AddDynamic(this, &UOrionGameplayAbility_AutoAttack::OnEquipWeaponMontageEnded);

	Component->IsPlayingMontage = true;

	if (const float Rate = AnimInst->Montage_Play(Component->AM_EquipWeapon, 1.f); Rate > 0.f)
	{
		const float Duration = Component->AM_EquipWeapon->GetPlayLength() / Rate;

		if (const UWorld* World = GetWorld())
		{
			TWeakObjectPtr<AOrionChara> WeakOwner(Owner);
			TWeakObjectPtr<UOrionCombatComponent> WeakComp(Component);
			
			World->GetTimerManager().SetTimer(
				EquipMontageSpawnHandle,
				[WeakOwner, WeakComp]()
				{
					if (AOrionChara* Ptr = WeakOwner.Get())
					{
						Ptr->SpawnWeaponActor();
						if (WeakComp.IsValid())
						{
							WeakComp->IsEquippedWithWeapon = true;
						}
					}
				},
				0.44f * Duration,
				false
			);

			const float TriggerTime = FMath::Max(0.f, Duration - 0.15f);
			World->GetTimerManager().SetTimer(
				EquipMontageTriggerHandle,
				[WeakComp]()
				{
					if (WeakComp.IsValid())
					{
						WeakComp->IsAttackOnCharaLongRange = true;
						WeakComp->IsMontageReady2Attack = true;
					}
				},
				TriggerTime,
				false
			);
		}
	}
	else
	{
		Component->IsPlayingMontage = false;
		Component->IsEquippedWithWeapon = true;
		Component->IsMontageReady2Attack = true;
		Component->IsAttackOnCharaLongRange = true;
		Owner->SpawnWeaponActor();
	}
}

void UOrionGameplayAbility_AutoAttack::OnEquipWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EquipMontageSpawnHandle);
		World->GetTimerManager().ClearTimer(EquipMontageTriggerHandle);
	}

	if (Component)
	{
		Component->IsPlayingMontage = false;
	}

	if (bInterrupted)
	{
		OnEquipWeaponMontageInterrupted();
	}
}

void UOrionGameplayAbility_AutoAttack::OnEquipWeaponMontageInterrupted()
{
	EndAbility();
}

UOrionCombatComponent::UOrionCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	// Ensure global weapon params are initialized
	InitializeGlobalWeaponParams();
}

const FOrionWeaponParams& UOrionCombatComponent::GetCurrentWeaponParams() const
{
	const FOrionWeaponParams* Params = GlobalWeaponParamsMap.Find(CurrentWeaponType);
	if (Params)
	{
		return *Params;
	}
	
	// Fallback: return Rifle params if not found
	const FOrionWeaponParams* RifleParams = GlobalWeaponParamsMap.Find(EOrionWeaponType::Rifle);
	if (RifleParams)
	{
		return *RifleParams;
	}
	
	// Last resort: return a static default (should never happen if InitializeGlobalWeaponParams was called)
	static FOrionWeaponParams DefaultParams;
	return DefaultParams;
}

void UOrionCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	
	const FOrionWeaponParams& Params = GetCurrentWeaponParams();
	SpawnBulletActorAccumulatedTime = Params.AttackFrequencyLongRange - Params.AttackTriggerTimeLongRange;

	// Initialize Ability
	AutoAttackAbility = NewObject<UOrionGameplayAbility_AutoAttack>(this, UOrionGameplayAbility_AutoAttack::StaticClass());
	if (AutoAttackAbility)
	{
		AutoAttackAbility->Initialize(this);
	}
}

void UOrionCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (ActiveAbility)
	{
		ActiveAbility->EndAbility();
	}
	ActiveAbility = nullptr;
	AutoAttackAbility = nullptr;

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
		if (TargetChara == GetOrionOwner())
		{
			IsAttackOnCharaLongRange = false;
			return true;
		}
	}

	// [Refactor] Use AttributeComponent to check if target is alive
	if (const UOrionAttributeComponent* AttrComp = InTarget->FindComponentByClass<UOrionAttributeComponent>())
	{
		if (!AttrComp->IsAlive())
		{
			AttackOnCharaLongRangeStop();
			return true;
		}
	}
	else
	{
		// Target has no AttributeComponent, treat as invalid or undamageable?
		// For now, if it's an actor without health, we might just return true to stop attacking.
		// Or assume it's destructible in other ways.
		// Given the requirement "attack all actors with AttributeComponent", others are invalid.
		AttackOnCharaLongRangeStop();
		return true;
	}

	return AttackOnCharaLongRange(DeltaTime, InTarget, HitOffset);
}

bool UOrionCombatComponent::AttackOnCharaLongRange(float DeltaTime, AActor* InTarget, FVector HitOffset)
{
	// [New GAS Architecture Proxy]
	
	if (!AutoAttackAbility)
	{
		// Fallback if ability init failed, though it shouldn't
		return true; 
	}

	// 1. Activate if not active
	if (ActiveAbility != AutoAttackAbility)
	{
		if (ActiveAbility)
		{
			ActiveAbility->EndAbility();
		}
		
		ActiveAbility = AutoAttackAbility;
		ActiveAbility->SetTarget(InTarget);
		ActiveAbility->Activate();
	}

	// 2. Update Target (in case it changes during execution, though Action usually holds one target)
	ActiveAbility->SetTarget(InTarget);

	// 3. Tick
	ActiveAbility->Tick(DeltaTime);

	// 4. Determine return value based on ability state or execution result
	// The original function returns 'false' while running, and 'true' when it wants to stop/finish.
	// AutoAttack is usually continuous until stopped externally or target dies.
	// We can check if Ability is still active.
	// However, original logic returned 'true' mostly on invalid target.
	
	if (!InTarget) return true;

	return false; // Continuous action
}

void UOrionCombatComponent::AttackOnCharaLongRangeStop()
{
	// [New GAS Architecture Proxy]
	if (ActiveAbility)
	{
		ActiveAbility->EndAbility();
		ActiveAbility = nullptr;
	}

	// Keep cleaning up component state just in case, to match original guarantees
	
	AOrionChara* Owner = GetOrionOwner();
	if (Owner)
	{
		// ... logic handled by Ability::EndAbility but we double check here if we want absolute safety
		// Actually Ability::EndAbility should handle it.
		// But let's keep the original cleanup logic here as a "Finalize" to be safe,
		// or if Ability failed to clean up. 
		// Since we want "exact same function", trusting Ability::EndAbility is the "GAS way".
		// But let's look at what the original function did.
		
		if (UAnimInstance* AnimInst = Owner->GetMesh()->GetAnimInstance())
		{
			if (AM_EquipWeapon && AnimInst->Montage_IsPlaying(AM_EquipWeapon))
			{
				AnimInst->Montage_Stop(0.1f, AM_EquipWeapon);
				AnimInst->OnMontageEnded.RemoveDynamic(this, &UOrionCombatComponent::OnEquipWeaponMontageEnded);
			}
		}

		// [Fix] Do NOT remove weapon here if ability logic handles delay.
		// However, Component's Stop usually implies "Force Stop".
		// But Ability::EndAbility handles the delayed timer now.
		// So we shouldn't force remove here unless we want to reset hard.
		// Let's rely on Ability state.
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EquipMontageSpawnHandle);
		World->GetTimerManager().ClearTimer(EquipMontageTriggerHandle);
	}

	IsMontageReady2Attack = false;
	IsAttackOnCharaLongRange = false;
	const FOrionWeaponParams& Params = GetCurrentWeaponParams();
	SpawnBulletActorAccumulatedTime = Params.AttackFrequencyLongRange - Params.AttackTriggerTimeLongRange;
}

void UOrionCombatComponent::RefreshAttackFrequency()
{
	if (IsAttackOnCharaLongRange != PreviousTickIsAttackOnCharaLongRange)
	{
		const FOrionWeaponParams& Params = GetCurrentWeaponParams();
		SpawnBulletActorAccumulatedTime = Params.AttackFrequencyLongRange - Params.AttackTriggerTimeLongRange;
	}

	PreviousTickIsAttackOnCharaLongRange = IsAttackOnCharaLongRange;
}

void UOrionCombatComponent::CalculateAndSpawnBulletActor(const FVector& TargetLocation) const
{
	AOrionWeapon* SpawnedWeaponInstance = nullptr;
	FVector BulletSpawnLocation;
	if (const auto OwnerChara = Cast<AOrionChara>(GetOwner()))
	{
		OwnerChara->GetBulletSpawnParams(BulletSpawnLocation, SpawnedWeaponInstance);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnBulletActor: Owner is not AOrionChara."));
		return;
	}

	if (!SpawnedWeaponInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnBulletActor: No PrimaryWeaponRef found."));
		return;
	}

	const FVector DirToFire = (TargetLocation - BulletSpawnLocation).GetSafeNormal();

	/*UE_LOG(LogTemp, Log, TEXT("SpawnBulletActor: SpawnLoc=(%f, %f, %f), DirToFire=(%f, %f, %f), OwnerLoc=(%f, %f, %f), WeaponType=%d"),
		SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z,
		DirToFire.X, DirToFire.Y, DirToFire.Z,
		GetOwner()->GetActorLocation().X, GetOwner()->GetActorLocation().Y, GetOwner()->GetActorLocation().Z,
		static_cast<int32>(CurrentWeaponType));*/

	const FOrionWeaponParams& Params = GetCurrentWeaponParams();

	SpawnedWeaponInstance->Fire(
		BulletSpawnLocation,
		DirToFire,
		CurrentWeaponType,
		Params.PelletsCount,
		Params.SpreadAngle
	);

}

bool UOrionCombatComponent::HasClearLineOfSight(AActor* InTargetActor) const
{
	// Kept for backward compatibility or other usages, 
	// though AutoAttackAbility has its own copy for encapsulation.
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
	// Deprecated / Delegated to Ability.
	// Kept empty or legacy just in case something else calls it (Private, so unlikely).
	// But Ability has its own implementation.
}

void UOrionCombatComponent::OnEquipWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// Delegated to Ability.
	// If the delegate was bound to THIS component, we need to redirect or ensure Ability binds to it.
	// My Ability implementation binds to Ability's method.
	// So this can be empty or removed if no one else binds it.
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
