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

private:
    AOrionChara* GetOrionOwner() const;
};
