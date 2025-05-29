// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "Orion/OrionGlobals/EOrionStructure.h"
#include "OrionStructureComponent.generated.h"

UENUM()
enum class EOrionAxis : uint8
{
	East, South, West, North,
	Up, Down
};


USTRUCT()
struct FRegisteredSocketHandle
{
	GENERATED_BODY()
	int32 RawIndex = INDEX_NONE;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ORION_API UOrionStructureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 刚开始时 Actor 的世界坐标，用来做偏移基准 */
	FVector InitialActorLocation;

	/** 第一次 RebuildGeometry 时记录位置 */
	bool bInitialLocationStored = false;

	UPROPERTY()
	TMap<UPrimitiveComponent*, EOrionAxis> ClickBoxMapping; // key = UBoxComponent*

	/*  CLICK callback registered on every arrow  */
	UFUNCTION()
	void OnArrowClicked(UPrimitiveComponent* ClickedComp, FKey Button);


	/*  Helper that returns the enum that best matches a unit forward vector      */
	static EOrionAxis ResolveDirection(const FVector& Forward);

	/*  Runtime state ----------------------------------------------------------- */
	/*  Which arrow controls which axis?                                         */
	UPROPERTY()
	TMap<UPrimitiveComponent*, EOrionAxis> ArrowMapping;

	/*  Positive expansion amount on each axis, in centimetres                   */
	float Expansion[6] = {0, 0, 0, 0, 0, 0}; // East,South,West,North,Up,Down


	UOrionStructureComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAutoRegisterSockets = true;

	void RegisterSquareFoundationSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterTriangleFoundationSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterWallSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterDoubleWallSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterAdjustableStructure(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterAllSockets() const;

	/* Orion Structure Rule Type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	EOrionStructure OrionStructureType;

	UPROPERTY()
	UStaticMeshComponent* StructureMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	bool bForceSnapOnGrid = false;

	static const TMap<EOrionStructure, FVector> StructureBoundMap;

	static FVector GetStructureBounds(const EOrionStructure Type);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	bool bIsAdjustable = false;

private:
	UPROPERTY()
	UOrionBuildingManager* BuildingManager = nullptr;
	UPROPERTY()
	TArray<FRegisteredSocketHandle> Handles;
};
