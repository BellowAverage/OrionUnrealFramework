// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "OrionStructureComponent.generated.h"



UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ORION_API UOrionStructureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	bool BIsPreviewStructure;


	UOrionStructureComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;


	UPROPERTY()
	UStaticMeshComponent* StructureMesh;



	void RegisterSquareFoundationSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterTriangleFoundationSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterWallSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterDoubleWallSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;

	/* Orion Structure Rule Type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config (Non-null)")
	EOrionStructure OrionStructureType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config (Non-null)")
	bool bForceSnapOnGrid = false;

	static const TMap<EOrionStructure, FVector> StructureBoundMap;

	static FVector GetStructureBounds(const EOrionStructure Type);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config (Non-null)")
	bool bIsAdjustable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config (Non-null)")
	bool bAutoRegisterSockets;

	// [Rust System] Current stability (0.0 - 1.0)
	UPROPERTY(VisibleInstanceOnly, Category = "Stability")
	float CurrentStability = 0.0f;

	// [Rust System] Self decay value
	// Recommended default: Wall=0.1 (supports 10 vertical layers), Floor/Roof=0.2 (supports 5 hanging blocks)
	UPROPERTY()
	float StabilityDecay = 0.1f;

	// [Rust System] Core calculation function
	void UpdateStability();

	void SocketsRegistryHandler() const;

private:
	// Helper: Ground check
	bool CheckIsTouchingGround() const;
	
	UPROPERTY()
	UOrionBuildingManager* BuildingManager = nullptr;
};
