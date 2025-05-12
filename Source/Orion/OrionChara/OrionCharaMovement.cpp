#include "OrionChara.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"

void AOrionChara::InitOrionCharaMovement()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		// Don't rotate character to camera direction
		bUseControllerRotationPitch = false;
		bUseControllerRotationYaw = false;
		bUseControllerRotationRoll = false;

		// Configure character movement
		MovementComponent->bOrientRotationToMovement = true;
		MovementComponent->RotationRate = FRotator(0.f, 270.f, 0.f);
		MovementComponent->bConstrainToPlane = true;
		MovementComponent->bSnapToPlaneAtStart = true;

		MovementComponent->MaxWalkSpeed = OrionCharaSpeed;
	}

	if (GetMesh())
	{
		DefaultMeshRelativeLocation = GetMesh()->GetRelativeLocation();
		DefaultMeshRelativeRotation = GetMesh()->GetRelativeRotation();
		DefaultCapsuleMeshOffset = GetCapsuleComponent()->GetComponentLocation() - GetMesh()->GetComponentLocation();
	}
}

void AOrionChara::RegisterCharaRagdoll(float DeltaTime)
{
	if (CharaState == ECharaState::Ragdoll)
	{
		SynchronizeCapsuleCompLocation();

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
}


void AOrionChara::SynchronizeCapsuleCompLocation() const
{
	const FName RootBone = GetMesh()->GetBoneName(0);
	const FVector MeshRootLocation = GetMesh()->GetBoneLocation(RootBone);

	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		const FVector NewCapsuleLocation = MeshRootLocation + DefaultCapsuleMeshOffset;
		CapsuleComp->SetWorldLocation(NewCapsuleLocation);
	}
}

void AOrionChara::ForceDetectionOnVelocityChange()
{
	const FVector CurrentVelocity = GetVelocity();

	if (const float VelocityChange = (CurrentVelocity - PreviousVelocity).Size() > VelocityChangeThreshold)
	{
		//OnForceExceeded(CurrentVelocity - PreviousVelocity);
		BlueprintNativeVelocityExceeded();
	}

	PreviousVelocity = CurrentVelocity;
}

void AOrionChara::OnForceExceeded(const FVector& DeltaVelocity)
{
	const float DeltaVSize = DeltaVelocity.Size();
	UE_LOG(LogTemp, Warning, TEXT("Force exceeded! Delta Velocity: %f"), DeltaVSize);

	const float Mass = GetMesh()->GetMass();

	constexpr float DeltaT = 0.0166f;

	// F = m * Δv / Δt  
	const float ApproximatedForce = Mass * DeltaVSize / DeltaT / 20.0f;
	UE_LOG(LogTemp, Warning, TEXT("Approximated Impulse Force: %f"), ApproximatedForce);

	const FVector CurrVel = GetVelocity();

	Ragdoll();

	GetMesh()->SetPhysicsLinearVelocity(CurrVel);

	const FVector ImpulseToAdd = DeltaVelocity.GetSafeNormal() * ApproximatedForce * DeltaT;

	// Log the physics simulation status of GetMesh()  
	if (GetMesh())
	{
		const bool bIsSimulatingPhysics = GetMesh()->IsSimulatingPhysics();
		UE_LOG(LogTemp, Warning, TEXT("Physics Simulation Status of GetMesh(): %s"),
		       bIsSimulatingPhysics ? TEXT("Enabled") : TEXT("Disabled"));
	}

	GetMesh()->AddImpulse(ImpulseToAdd, NAME_None, true);
}

void AOrionChara::Ragdoll()
{
	RemoveAllActions();

	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(
		GetComponentByClass(UPrimitiveComponent::StaticClass()));
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

	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(
		GetComponentByClass(UPrimitiveComponent::StaticClass()));
	if (PrimitiveComponent)
	{
		PrimitiveComponent->SetSimulatePhysics(false);
	}

	FRotator CurrentRot = GetActorRotation();
	FRotator UprightRot(0.0f, CurrentRot.Yaw, 0.0f);
	SetActorRotation(UprightRot);

	if (GetMesh())
	{
		GetMesh()->SetRelativeLocation(DefaultMeshRelativeLocation);
		GetMesh()->SetRelativeRotation(DefaultMeshRelativeRotation);

		GetMesh()->RefreshBoneTransforms();

		if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
		{
			AnimInst->InitializeAnimation();
		}
	}
}

void AOrionChara::ChangeMaxWalkSpeed(float InValue)
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = InValue;
		OrionCharaSpeed = InValue;
	}
}
