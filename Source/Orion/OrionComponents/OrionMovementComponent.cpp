#include "OrionMovementComponent.h"
#include "Orion/OrionChara/OrionChara.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "DrawDebugHelpers.h"
#include "AIController.h" // Still needed for checking if controller exists
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

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

	// --- [Fix] Detect if target has changed significantly ---
	// If currently has a destination, and new target is more than 10cm away from old target, treat as new command
	if (bHasMoveDestination && FVector::DistSquared(InTargetLocation, LastMoveDestination) > 100.0f)
	{
		// UE_LOG(LogTemp, Log, TEXT("[Movement] Target changed, resetting path."));
		NavPathPoints.Empty(); // Clear path, force next code to recalculate path
		CurrentNavPointIndex = 0;
	}
	// --- [Fix End] ---

	// Record target for "replan" use
	LastMoveDestination = InTargetLocation;
	bHasMoveDestination = true;

	constexpr float AcceptanceRadius = 50.f;
	const FVector SearchExtent(500.f, 500.f, 500.f);

    // OrionAIControllerInstance check here is to ensure the character is controlled, not to use MoveTo
	if (!Owner->GetController())
	{
		// UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] No Controller -> treat as arrived"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		// UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] No NavigationSystem -> treat as arrived"));
		return true;
	}
	
	// Check if stuck and need to reroute
	if (bEnableAvoidance && CheckAndHandleStuck(GetWorld()->GetDeltaSeconds()))
	{
		// Force path recalculation when stuck
		NavPathPoints.Empty();
		CurrentNavPointIndex = 0;
		bIsCurrentlyStuck = false;
		StuckTimer = 0.0f;
		// UE_LOG(LogTemp, Log, TEXT("[MoveToLocation] Character stuck, recalculating path..."));
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
		
		// Debug Draw removed
	}

	// Current target point
	FVector TargetPoint = NavPathPoints[CurrentNavPointIndex];
	float Dist2D = FVector::Dist2D(Owner->GetActorLocation(), TargetPoint);
	
    // Verbose Log removed

	// After reaching current path point, advance to next
	if (Dist2D <= AcceptanceRadius)
	{
		CurrentNavPointIndex++;
		if (CurrentNavPointIndex >= NavPathPoints.Num())
		{
			// All points reached, stop moving
			NavPathPoints.Empty();
			CurrentNavPointIndex = 0;
			// UE_LOG(LogTemp, Log, TEXT("[MoveToLocation] Arrived at final destination"));
			return true;
		}
	}

	// Move by adding input: toward next path point
	FVector Dir = (TargetPoint - Owner->GetActorLocation()).GetSafeNormal2D();
    
	// Apply avoidance if enabled
	if (bEnableAvoidance)
	{
		FVector AvoidanceDir = CalculateAvoidanceVector(Dir);
		
		// Blend desired direction with avoidance
		if (!AvoidanceDir.IsNearlyZero())
		{
			Dir = (Dir * (1.0f - AvoidanceWeight) + AvoidanceDir * AvoidanceWeight).GetSafeNormal2D();
		}
	}
	
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
    
    // Original code's ReRoute used OrionAIControllerInstance->MoveTo, but here to maintain consistency (since you don't want to use AIC),
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

TArray<AOrionChara*> UOrionMovementComponent::GetNearbyCharacters() const
{
	TArray<AOrionChara*> NearbyCharacters;
	
	AOrionChara* Owner = GetOrionOwner();
	if (!Owner)
	{
		return NearbyCharacters;
	}
	
	UWorld* World = GetWorld();
	if (!World)
	{
		return NearbyCharacters;
	}
	
	const FVector MyLocation = Owner->GetActorLocation();
	
	// Get all OrionChara actors
	TArray<AActor*> AllCharas;
	UGameplayStatics::GetAllActorsOfClass(World, AOrionChara::StaticClass(), AllCharas);
	
	for (AActor* Actor : AllCharas)
	{
		if (Actor == Owner)
		{
			continue; // Skip self
		}
		
		AOrionChara* OtherChara = Cast<AOrionChara>(Actor);
		if (!OtherChara)
		{
			continue;
		}
		
		// Check if character is alive
		if (OtherChara->CharaState != ECharaState::Alive)
		{
			continue;
		}
		
		// Check distance
		float Distance = FVector::Dist(MyLocation, OtherChara->GetActorLocation());
		if (Distance < AvoidanceRadius)
		{
			NearbyCharacters.Add(OtherChara);
		}
	}
	
	return NearbyCharacters;
}

FVector UOrionMovementComponent::CalculateAvoidanceVector(const FVector& DesiredDirection) const
{
	AOrionChara* Owner = GetOrionOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}
	
	const FVector MyLocation = Owner->GetActorLocation();
	const FVector MyVelocity = Owner->GetVelocity();
	
	TArray<AOrionChara*> NearbyCharacters = GetNearbyCharacters();
	
	if (NearbyCharacters.Num() == 0)
	{
		return DesiredDirection;
	}
	
	FVector TotalAvoidance = FVector::ZeroVector;
	int32 AvoidanceCount = 0;
	
	for (AOrionChara* OtherChara : NearbyCharacters)
	{
		const FVector OtherLocation = OtherChara->GetActorLocation();
		const FVector ToOther = OtherLocation - MyLocation;
		const float Distance = ToOther.Size2D();
		
		// Skip if too far (shouldn't happen but safety check)
		if (Distance > AvoidanceRadius || Distance < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		
		// Calculate avoidance direction (away from the other character)
		FVector AwayFromOther = -ToOther.GetSafeNormal2D();
		
		// Stronger avoidance when closer
		float AvoidanceStrength = 1.0f - (Distance / AvoidanceRadius);
		AvoidanceStrength = FMath::Square(AvoidanceStrength); // Exponential falloff
		
		// Extra strong avoidance when within minimum separation distance
		if (Distance < MinSeparationDistance)
		{
			AvoidanceStrength = FMath::Max(AvoidanceStrength, 1.0f);
			
			// Add a perpendicular component to help characters slide past each other
			FVector Perpendicular = FVector::CrossProduct(AwayFromOther, FVector::UpVector).GetSafeNormal2D();
			
			// Choose perpendicular direction based on desired direction
			float DotProduct = FVector::DotProduct(DesiredDirection, Perpendicular);
			if (DotProduct < 0)
			{
				Perpendicular = -Perpendicular;
			}
			
			// Blend perpendicular direction with away direction
			AwayFromOther = (AwayFromOther + Perpendicular * 0.5f).GetSafeNormal2D();
		}
		
		// Consider the other character's velocity for predictive avoidance
		const FVector OtherVelocity = OtherChara->GetVelocity();
		if (!OtherVelocity.IsNearlyZero())
		{
			// Predict future position
			const float PredictionTime = 0.5f;
			const FVector PredictedOtherPos = OtherLocation + OtherVelocity * PredictionTime;
			const FVector ToPredicted = PredictedOtherPos - MyLocation;
			const float PredictedDist = ToPredicted.Size2D();
			
			if (PredictedDist < Distance && PredictedDist < AvoidanceRadius)
			{
				// Other is getting closer, add extra avoidance
				FVector AwayFromPredicted = -ToPredicted.GetSafeNormal2D();
				AwayFromOther = (AwayFromOther + AwayFromPredicted * 0.5f).GetSafeNormal2D();
				AvoidanceStrength *= 1.2f;
			}
		}
		
		TotalAvoidance += AwayFromOther * AvoidanceStrength;
		AvoidanceCount++;
	}
	
	if (AvoidanceCount == 0)
	{
		return DesiredDirection;
	}
	
	// Average the avoidance vectors
	TotalAvoidance /= AvoidanceCount;
	
	// Combine with desired direction
	FVector FinalDirection = DesiredDirection + TotalAvoidance;
	
	// Ensure we still have forward progress (don't completely override desired direction)
	float ForwardComponent = FVector::DotProduct(FinalDirection, DesiredDirection);
	if (ForwardComponent < 0.2f)
	{
		// If avoidance is pushing us backward too much, add more forward bias
		FinalDirection = DesiredDirection * 0.3f + FinalDirection;
	}
	
	return FinalDirection.GetSafeNormal2D();
}

bool UOrionMovementComponent::CheckAndHandleStuck(float DeltaTime)
{
	AOrionChara* Owner = GetOrionOwner();
	if (!Owner)
	{
		return false;
	}
	
	const FVector CurrentPosition = Owner->GetActorLocation();
	const float DistanceMoved = FVector::Dist2D(CurrentPosition, LastPositionForStuckCheck);
	
	if (DistanceMoved < StuckDistanceThreshold)
	{
		StuckTimer += DeltaTime;
		
		if (StuckTimer >= StuckTimeThreshold)
		{
			bIsCurrentlyStuck = true;
			return true;
		}
	}
	else
	{
		StuckTimer = 0.0f;
		bIsCurrentlyStuck = false;
	}
	
	LastPositionForStuckCheck = CurrentPosition;
	return false;
}
