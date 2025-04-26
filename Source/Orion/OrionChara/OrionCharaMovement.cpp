#include "../OrionChara.h"
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
		OnForceExceeded(CurrentVelocity - PreviousVelocity);
	}

	PreviousVelocity = CurrentVelocity;
}

void AOrionChara::ChangeMaxWalkSpeed(float InValue)
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = InValue;
		OrionCharaSpeed = InValue;
	}
}
