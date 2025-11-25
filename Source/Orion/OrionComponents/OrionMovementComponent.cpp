#include "OrionMovementComponent.h"
#include "Orion/OrionChara/OrionChara.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "DrawDebugHelpers.h"
#include "AIController.h" // Still needed for checking if controller exists

UOrionMovementComponent::UOrionMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOrionMovementComponent::BeginPlay()
{
	Super::BeginPlay();

    // Initialize speed (logic from InitOrionCharaMovement can be partially moved here)
    if (AOrionChara* Owner = GetOrionOwner())
    {
        if (UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement())
        {
            MoveComp->MaxWalkSpeed = OrionCharaSpeed;
        }
    }
}

AOrionChara* UOrionMovementComponent::GetOrionOwner() const
{
    return Cast<AOrionChara>(GetOwner());
}

bool UOrionMovementComponent::MoveToLocation(const FVector& InTargetLocation)
{
    AOrionChara* Owner = GetOrionOwner();
    if (!Owner) return true;

	// Record target for "replan" use
	LastMoveDestination = InTargetLocation;
	bHasMoveDestination = true;

	constexpr float AcceptanceRadius = 50.f;
	const FVector SearchExtent(500.f, 500.f, 500.f);

    // AIController check here is to ensure the character is controlled, not to use MoveTo
	if (!Owner->GetController())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] No Controller -> treat as arrived"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] No NavigationSystem -> treat as arrived"));
		return true;
	}

	// If path hasn't been generated yet, sync calculate once
	if (NavPathPoints.Num() == 0)
	{
		FNavLocation ProjectedNav;
		FVector DestLocation = InTargetLocation;
		if (NavSys->ProjectPointToNavigation(InTargetLocation, ProjectedNav, SearchExtent))
		{
			DestLocation = ProjectedNav.Location;
		}
		// Calculate path
		FPathFindingQuery Query;
		UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(GetWorld(), Owner->GetActorLocation(), DestLocation, Owner);
		
        if (!Path || Path->PathPoints.Num() <= 1)
		{
			// UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] Failed to build path or already at goal"));
			return true;
		}
		NavPathPoints = Path->PathPoints;
		CurrentNavPointIndex = 1; // Skip start point

        // Debug Draw (original code preserved)
		for (int32 i = 0; i < NavPathPoints.Num(); ++i)
		{
			DrawDebugSphere(GetWorld(), NavPathPoints[i], 20.f, 12, FColor::Green, false, 5.f);
			if (i > 0)
			{
				DrawDebugLine(GetWorld(), NavPathPoints[i - 1], NavPathPoints[i], FColor::Green, false, 5.f, 0, 2.f);
			}
		}
	}

	// Current target point
	FVector TargetPoint = NavPathPoints[CurrentNavPointIndex];
	float Dist2D = FVector::Dist2D(Owner->GetActorLocation(), TargetPoint);
	
    // Verbose Log
	// UE_LOG(LogTemp, Verbose, TEXT("[MoveToLocation] NextPt=%s, Dist2D=%.1f"), *TargetPoint.ToString(), Dist2D);

	// After reaching current path point, advance to next
	if (Dist2D <= AcceptanceRadius)
	{
		CurrentNavPointIndex++;
		if (CurrentNavPointIndex >= NavPathPoints.Num())
		{
			// All points reached, stop moving
			NavPathPoints.Empty();
			CurrentNavPointIndex = 0;
			UE_LOG(LogTemp, Log, TEXT("[MoveToLocation] Arrived at final destination"));
			return true;
		}
	}

	// Move by adding input: toward next path point
	FVector Dir = (TargetPoint - Owner->GetActorLocation()).GetSafeNormal2D();
    
    // [Key modification] Call Owner's AddMovementInput
	Owner->AddMovementInput(Dir, 1.0f, true);

	return false; // Still moving
}

void UOrionMovementComponent::MoveToLocationStop()
{
    // If using AddMovementInput, it's best to zero Velocity or StopMovement when stopping
    if (AOrionChara* Owner = GetOrionOwner())
    {
        if (AAIController* AIC = Cast<AAIController>(Owner->GetController()))
        {
            AIC->StopMovement();
        }
    }
    
	bHasMoveDestination = false;
	NavPathPoints.Empty();
	CurrentNavPointIndex = 0;
}

void UOrionMovementComponent::ReRouteMoveToLocation()
{
    // Original logic: clear path, MoveToLocation will automatically recalculate next frame
	if (!bHasMoveDestination)
	{
		return;
	}

	// 1) Clear old navigation points
	NavPathPoints.Empty();
	CurrentNavPointIndex = 0;
    
    // Original code's ReRoute used AIController->MoveTo, but here to maintain consistency (since you don't want to use AIC),
    // we only need to clear path points, because MoveToLocation function will automatically pathfind if NavPathPoints is empty.
    // This is safer than the original ReRoute mixed logic.
    
	UE_LOG(LogTemp, Log, TEXT("[ReRoute] Path cleared, will be recalculated next tick."));
}

void UOrionMovementComponent::ChangeMaxWalkSpeed(float InValue)
{
    OrionCharaSpeed = InValue;
    if (AOrionChara* Owner = GetOrionOwner())
    {
        if (UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement())
        {
            MoveComp->MaxWalkSpeed = InValue;
        }
    }
}
