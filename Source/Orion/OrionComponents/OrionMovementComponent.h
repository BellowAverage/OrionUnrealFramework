#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrionMovementComponent.generated.h"

class AOrionChara;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ORION_API UOrionMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UOrionMovementComponent();

protected:
	virtual void BeginPlay() override;

public:	
	// --- Core Movement Logic (Preserving Original Manual Implementation) ---
    
    // Execute movement (returns true when arrived/completed)
	bool MoveToLocation(const FVector& InTargetLocation);
    
    // Stop movement
	void MoveToLocationStop();
    
    // Recalculate path
	void ReRouteMoveToLocation();
    
    // Change max speed
    void ChangeMaxWalkSpeed(float InValue);

    // --- State Variables (Moved from Chara) ---

    // Recorded target point
	FVector LastMoveDestination;
    // Whether there is a valid target
	bool bHasMoveDestination = false;
    
    // Current path point list
	TArray<FVector> NavPathPoints;
    // Current target path point index
	int32 CurrentNavPointIndex = 0;
    
    // Default speed configuration (if you want to manage it here)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float OrionCharaSpeed = 500.0f;

	// --- Avoidance Settings ---
	
	// Enable/disable avoidance behavior
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance")
	bool bEnableAvoidance = true;
	
	// Radius within which to detect other characters for avoidance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance")
	float AvoidanceRadius = 200.0f;
	
	// Weight of avoidance force (0-1, higher = stronger avoidance)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance")
	float AvoidanceWeight = 0.6f;
	
	// Minimum distance to maintain from other characters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance")
	float MinSeparationDistance = 80.0f;
	
	// Time threshold for stuck detection (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance")
	float StuckTimeThreshold = 0.5f;
	
	// Distance threshold for stuck detection
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance")
	float StuckDistanceThreshold = 10.0f;

private:
    AOrionChara* GetOrionOwner() const;
	
	// Calculate avoidance vector based on nearby characters
	FVector CalculateAvoidanceVector(const FVector& DesiredDirection) const;
	
	// Get all nearby OrionChara within avoidance radius
	TArray<AOrionChara*> GetNearbyCharacters() const;
	
	// Stuck detection variables
	FVector LastPositionForStuckCheck;
	float StuckTimer = 0.0f;
	bool bIsCurrentlyStuck = false;
	
	// Check if character is stuck and handle it
	bool CheckAndHandleStuck(float DeltaTime);
};
