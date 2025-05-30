﻿// Fill out your copyright notice in the Description page of Project Settings.
#include "Orion/OrionComponents/OrionStructureComponent.h"
#include "Orion/OrionGlobals/EOrionStructure.h"
#include "Orion/OrionPlayerController/OrionPlayerController.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
class UPrimitiveComponent;


#define STRUCTURE_THICKNESS_BASE 10.f
#define STRUCTURE_LENGTH_BASE 50.f * 1.25f
#define STRUCTURE_HEIGHT_BASE 150.f

const TMap<EOrionStructure, FVector> UOrionStructureComponent::StructureBoundMap =
{
	// ---------- X (half) Y (half) Z (half) -------------
	{EOrionStructure::BasicSquareFoundation, {STRUCTURE_LENGTH_BASE, STRUCTURE_LENGTH_BASE, STRUCTURE_THICKNESS_BASE}},
	{
		EOrionStructure::BasicTriangleFoundation,
		{STRUCTURE_LENGTH_BASE * (FMath::Sqrt(3.f) / 2), STRUCTURE_LENGTH_BASE, STRUCTURE_THICKNESS_BASE}
	},
	{EOrionStructure::Wall, {STRUCTURE_THICKNESS_BASE, STRUCTURE_LENGTH_BASE, STRUCTURE_HEIGHT_BASE}},
	{EOrionStructure::DoubleWall, {STRUCTURE_THICKNESS_BASE, 2.f * STRUCTURE_LENGTH_BASE, STRUCTURE_HEIGHT_BASE}},
	{EOrionStructure::BasicRoof, {STRUCTURE_LENGTH_BASE,STRUCTURE_LENGTH_BASE, STRUCTURE_LENGTH_BASE}},
};


// Sets default values for this component's properties
UOrionStructureComponent::UOrionStructureComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	StructureMesh = nullptr;

	// ...
}


void UOrionStructureComponent::BeginPlay()
{
	Super::BeginPlay();

	if (OrionStructureType == EOrionStructure::None)
	{
		UE_LOG(LogTemp, Error, TEXT("[StructureComponent] StructureType is None!"));
		return;
	}

	BuildingManager = GetWorld()->GetGameInstance()
		                  ? GetWorld()->GetGameInstance()->GetSubsystem<UOrionBuildingManager>()
		                  : nullptr;

	if (!BuildingManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[StructureComponent] BuildingManager not found!"));
		return;
	}

	if (!StructureMesh)
	{
		TArray<UActorComponent*> Tagged =
			GetOwner()->GetComponentsByTag(UStaticMeshComponent::StaticClass(),
			                               FName("StructureMesh"));

		if (Tagged.Num() > 0)
		{
			StructureMesh = Cast<UStaticMeshComponent>(Tagged[0]);
		}
	}


	AOrionPlayerController* OrionPC =
		Cast<AOrionPlayerController>(GetWorld()->GetFirstPlayerController());

	if (OrionPC && OrionPC->bIsSpawnPreviewStructure)
	{
		OrionPC->bIsSpawnPreviewStructure = false;
		return;
	}

	if (bAutoRegisterSockets && BuildingManager)
	{
		RegisterAllSockets();
	}
}

void UOrionStructureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed && BuildingManager)
	{
		if (UOrionBuildingManager* SafeInitializedBuildingManager =
			GetOwner()->GetGameInstance()->GetSubsystem<UOrionBuildingManager>())
		{
			SafeInitializedBuildingManager->RemoveSocketRegistration(*GetOwner());
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UOrionStructureComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UOrionStructureComponent::RegisterSquareFoundationSockets(const FVector& StructureLocation,
                                                               const FRotator& StructureRotation) const
{
	const FVector SquareFoundationBound = GetStructureBounds(EOrionStructure::BasicSquareFoundation);
	const FVector WallBound = GetStructureBounds(EOrionStructure::Wall);

	BuildingManager->RegisterSocket(StructureLocation, StructureRotation, EOrionStructure::BasicSquareFoundation,
	                                true, GetWorld(), GetOwner());

	/* SquareFoundation -> SquareFoundation */
	static const FVector SquareFoundation2SquareFoundationLocationOffset[4] = {
		{SquareFoundationBound.X * 2, 0.f, 0.f},
		{0.f, -SquareFoundationBound.Y * 2, 0.f},
		{-SquareFoundationBound.X * 2, 0.f, 0.f},
		{0.f, SquareFoundationBound.Y * 2, 0.f}
	};
	for (const FVector& Offset : SquareFoundation2SquareFoundationLocationOffset)
	{
		BuildingManager->RegisterSocket(StructureLocation + StructureRotation.RotateVector(Offset), StructureRotation,
		                                EOrionStructure::BasicSquareFoundation, false, GetWorld(), GetOwner());
	}

	/* SquareFoundation -> TriangleFoundation */
	{
		static const FVector SquareFoundation2TriFoundationLocationOffset[4] = {
			{SquareFoundationBound.X + SquareFoundationBound.X * (1 / FMath::Sqrt(3.f)), 0.f, 0.f},
			{-SquareFoundationBound.X - SquareFoundationBound.X * (1 / FMath::Sqrt(3.f)), 0.f, 0.f},
			{0.f, SquareFoundationBound.Y + SquareFoundationBound.Y * (1 / FMath::Sqrt(3.f)), 0.f},
			{0.f, -SquareFoundationBound.Y - SquareFoundationBound.Y * (1 / FMath::Sqrt(3.f)), 0.f}
		};
		static constexpr float SquareFoundation2TriFoundationRotationOffset[4] = {180.f, 0.f, 270.f, 90.f};

		for (int32 i = 0; i < 4; ++i)
		{
			BuildingManager->RegisterSocket(
				StructureLocation + StructureRotation.RotateVector(SquareFoundation2TriFoundationLocationOffset[i]),
				StructureRotation + FRotator(0.f, SquareFoundation2TriFoundationRotationOffset[i], 0.f),
				EOrionStructure::BasicTriangleFoundation,
				/*bOccupied=*/false, GetWorld(), GetOwner());
		}
	}

	/* SquareFoundation -> Wall */
	static const FVector SquareFoundation2WallLocationOffset[4] = {
		{SquareFoundationBound.X, 0.f, SquareFoundationBound.Z + WallBound.Z},
		{0.f, SquareFoundationBound.Y, SquareFoundationBound.Z + WallBound.Z},
		{-SquareFoundationBound.X, 0.f, SquareFoundationBound.Z + WallBound.Z},
		{0.f, -SquareFoundationBound.Y, SquareFoundationBound.Z + WallBound.Z}
	};
	static const FRotator SquareFoundation2WallRotationOffset[4] = {
		{0, 0, 0},
		{0, 90, 0},
		{0, 180, 0},
		{0, 270, 0}
	};
	for (int32 i = 0; i < 4; ++i)
	{
		BuildingManager->RegisterSocket(
			StructureLocation + StructureRotation.RotateVector(SquareFoundation2WallLocationOffset[i]),
			StructureRotation + SquareFoundation2WallRotationOffset[i],
			EOrionStructure::Wall, false, GetWorld(), GetOwner());
	}
}

void UOrionStructureComponent::RegisterTriangleFoundationSockets(const FVector& StructureLocation,
                                                                 const FRotator& StructureRotation) const
{
	const float TriEdgeLength = 2.f * GetStructureBounds(EOrionStructure::BasicTriangleFoundation).Y;
	const FVector TriFoundationBound = GetStructureBounds(EOrionStructure::BasicTriangleFoundation);
	const FVector WallBound = GetStructureBounds(EOrionStructure::Wall);

	BuildingManager->RegisterSocket(
		StructureLocation, StructureRotation,
		EOrionStructure::BasicTriangleFoundation,
		/*bOccupied=*/true, GetWorld(), GetOwner());

	/* TriangleFoundation -> TriangleFoundation */
	const FVector TriFoundation2TriFoundationLocationOffset[3] = {
		{TriEdgeLength / FMath::Sqrt(3.0f), 0.f, 0.f},
		{-TriEdgeLength / (2.f * FMath::Sqrt(3.0f)), TriEdgeLength * 0.5f, 0.f},
		{-TriEdgeLength / (2.f * FMath::Sqrt(3.0f)), -TriEdgeLength * 0.5f, 0.f}
	};

	for (int32 i = 0; i < 3; ++i)
	{
		constexpr float TriFoundation2TriFoundationRotationOffset[3] = {180.f, 180.f, 180.f};
		const FVector EachTF2TFLocationOffset = StructureLocation + StructureRotation.RotateVector(
			TriFoundation2TriFoundationLocationOffset[i]);


		BuildingManager->RegisterSocket(
			EachTF2TFLocationOffset,
			StructureRotation + FRotator(0.f, TriFoundation2TriFoundationRotationOffset[i], 0.f),
			EOrionStructure::BasicTriangleFoundation,
			/*bOccupied=*/false, GetWorld(), GetOwner());
	}

	/* TriangleFoundation -> SquareFoundation */
	const FVector TriFoundation2SquareFoundationLocationOffset[3] = {
		{(FMath::Sqrt(3.0f) + 1.f) / (2.f * FMath::Sqrt(3.0f)) * TriEdgeLength, 0.f, 0.f},
		{
			-(0.25f + FMath::Sqrt(3.0f) / 12.f) * TriEdgeLength,
			-(0.25f + FMath::Sqrt(3.0f) / 4.f) * TriEdgeLength, 0.f
		},

		{
			-(0.25f + FMath::Sqrt(3.0f) / 12.f) * TriEdgeLength,
			(0.25f + FMath::Sqrt(3.0f) / 4.f) * TriEdgeLength, 0.f
		},

	};

	for (int32 i = 0; i < 3; ++i)
	{
		const FVector EachTF2SFLocationOffset = StructureLocation + StructureRotation.RotateVector(
			TriFoundation2SquareFoundationLocationOffset[i]);

		constexpr float TriFoundation2SquareFoundationRotationOffset[3] = {0.f, 60.f, 120.f};
		BuildingManager->RegisterSocket(
			EachTF2SFLocationOffset,
			StructureRotation + FRotator(0.f, TriFoundation2SquareFoundationRotationOffset[i], 0.f),
			EOrionStructure::BasicSquareFoundation,
			/*bOccupied=*/false, GetWorld(), GetOwner());
	}

	const FVector TriangleEdgeMidpoints[3] = {
		{TriEdgeLength * (1.f / (4.f * (FMath::Sqrt(3.0f) / 2.f))), 0.f, 0.f},
		{-1.f / (4.f * FMath::Sqrt(3.0f)) * TriEdgeLength, 0.25f * TriEdgeLength, 0.f},
		{-1.f / (4.f * FMath::Sqrt(3.0f)) * TriEdgeLength, -0.25f * TriEdgeLength, 0.f},

	};

	/* TriangleFoundation -> Wall */
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector TriFoundation2WallLocationOffset =
			StructureLocation + StructureRotation.RotateVector(
				TriangleEdgeMidpoints[i] + FVector(0.f, 0.f, TriFoundationBound.Z + WallBound.Z));
		constexpr float TriFoundation2WallRotationOffset[3] = {180.f, 300.f, 60.f};

		BuildingManager->RegisterSocket(
			TriFoundation2WallLocationOffset,
			StructureRotation + FRotator(0.f, TriFoundation2WallRotationOffset[i], 0.f),
			EOrionStructure::Wall,
			/*bOccupied=*/false, GetWorld(), GetOwner());
	}
}

void UOrionStructureComponent::RegisterWallSockets(const FVector& StructureLocation,
                                                   const FRotator& StructureRotation) const
{
	const FVector WallBound = GetStructureBounds(EOrionStructure::Wall);
	const FVector TriangleFoundationBound = GetStructureBounds(EOrionStructure::BasicTriangleFoundation);
	const float TriEdgeLength = 2.f * GetStructureBounds(EOrionStructure::BasicTriangleFoundation).Y;

	BuildingManager->RegisterSocket(StructureLocation, StructureRotation, EOrionStructure::Wall,
	                                true, GetWorld(), GetOwner());

	/* Wall -> Wall */
	const FVector Wall2WallLocationOffset = StructureLocation + FVector(0.f, 0.f, WallBound.Z * 2.f);

	BuildingManager->RegisterSocket(Wall2WallLocationOffset, StructureRotation, EOrionStructure::Wall,
	                                false, GetWorld(), GetOwner());

	/* Wall -> Floor (SquareFoundation) */
	const FTransform ParentTM(StructureRotation, StructureLocation);

	const FVector FrontLocal(-WallBound.Y, 0.f, WallBound.Z);
	const FVector RearLocal(WallBound.Y, 0.f, WallBound.Z);

	const FVector FrontWorld = ParentTM.TransformPositionNoScale(FrontLocal);
	const FVector RearWorld = ParentTM.TransformPositionNoScale(RearLocal);

	BuildingManager->RegisterSocket(
		FrontWorld, StructureRotation,
		EOrionStructure::BasicSquareFoundation,
		false, GetWorld(), GetOwner());

	BuildingManager->RegisterSocket(
		RearWorld, StructureRotation,
		EOrionStructure::BasicSquareFoundation,
		false, GetWorld(), GetOwner());

	/* Wall -> TriangleFloor (TriangleFoundation) */
	const FVector FrontLocalTri(TriEdgeLength / (2.0 * FMath::Sqrt(3.f)), 0.f, WallBound.Z);
	const FVector RearLocalTri(-TriEdgeLength / (2.0 * FMath::Sqrt(3.f)), 0.f, WallBound.Z);

	const FVector FrontWorldTri = ParentTM.TransformPositionNoScale(FrontLocalTri);
	const FVector RearWorldTri = ParentTM.TransformPositionNoScale(RearLocalTri);

	BuildingManager->RegisterSocket(
		FrontWorldTri, StructureRotation + FRotator(0.f, 180.f, 0.f),
		EOrionStructure::BasicTriangleFoundation,
		false, GetWorld(), GetOwner());

	BuildingManager->RegisterSocket(
		RearWorldTri, StructureRotation,
		EOrionStructure::BasicTriangleFoundation,
		false, GetWorld(), GetOwner());
}


void UOrionStructureComponent::RegisterDoubleWallSockets(const FVector& StructureLocation,
                                                         const FRotator& StructureRotation) const
{
	{
		const FVector DoubleWallBound = GetStructureBounds(EOrionStructure::DoubleWall);
		const FVector SquareFoundationBound = GetStructureBounds(EOrionStructure::BasicSquareFoundation);

		/* DoubleWall -> Wall */
		const FVector DoubleWall2WallLocationOffset[2] = {
			StructureLocation + FVector(0.f, 0.f, DoubleWallBound.Z * 2.f),
			StructureLocation + FVector(0.f, DoubleWallBound.Y, DoubleWallBound.Z * 2.f)
		};

		for (int32 i = 0; i < 2; ++i)
		{
			BuildingManager->RegisterSocket(
				DoubleWall2WallLocationOffset[i],
				StructureRotation,
				EOrionStructure::Wall,
				false,
				GetWorld(),
				GetOwner()
			);
		}

		/* DoubleWall -> SquareRoof (SquareFoundation) */
		const FVector DoubleWall2SquareFoundationOffset[4] = {
			StructureLocation + FVector(SquareFoundationBound.X, 0.f, DoubleWallBound.Z),
			StructureLocation + FVector(-SquareFoundationBound.X, 0.f, DoubleWallBound.Z),
			StructureLocation + FVector(SquareFoundationBound.X, DoubleWallBound.Y, DoubleWallBound.Z),
			StructureLocation + FVector(-SquareFoundationBound.X, DoubleWallBound.Y, DoubleWallBound.Z),
		};

		for (int32 i = 0; i < 4; ++i)
		{
			BuildingManager->RegisterSocket(
				DoubleWall2SquareFoundationOffset[i],
				StructureRotation,
				EOrionStructure::BasicSquareFoundation,
				false,
				GetWorld(),
				GetOwner()
			);
		}
	}
}

void UOrionStructureComponent::RegisterAdjustableStructure
(
	const FVector& /*InLocation*/,
	const FRotator& /*InRotation*/
) const
{
	AActor* Owner = GetOwner();
	if (!Owner) { return; }

	TArray<UArrowComponent*> Arrows;
	Owner->GetComponents<UArrowComponent>(Arrows);

	for (UArrowComponent* Arrow : Arrows)
	{
		if (!Arrow) { continue; }

		const EOrionAxis Axis = ResolveDirection(Arrow->GetForwardVector());

		if (ClickBoxMapping.FindKey(Axis))
		{
			continue; // 已经有同一方向的 ClickBox 了
		}

		// 1) 把箭头和 Box 设为绝对缩放，这样将来 Actor 变形也不受影响
		Arrow->SetUsingAbsoluteScale(true);

		UBoxComponent* ClickBox = NewObject<UBoxComponent>(Owner, NAME_None, RF_Transient);
		if (!ClickBox) { continue; }
		ClickBox->SetUsingAbsoluteScale(true);

		ClickBox->AttachToComponent(Arrow,
		                            FAttachmentTransformRules::SnapToTargetNotIncludingScale);

		ClickBox->SetBoxExtent(FVector(15.f));
		ClickBox->SetRelativeLocation(FVector(40.f, 0.f, 0.f));

		ClickBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ClickBox->SetCollisionObjectType(ECC_WorldDynamic);
		ClickBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		ClickBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		ClickBox->SetHiddenInGame(true);

		ClickBox->RegisterComponent();

		ClickBox->OnClicked.AddUniqueDynamic(
			const_cast<UOrionStructureComponent*>(this),
			&UOrionStructureComponent::OnArrowClicked);

		const_cast<UOrionStructureComponent*>(this)->ClickBoxMapping.Add(ClickBox, Axis);

		UE_LOG(LogTemp, Warning,
		       TEXT("[StructureComponent] ClickBox created for arrow %s -> %d"),
		       *Arrow->GetName(), static_cast<int32>(Axis));
	}
}

/* ------------------------------------------------------------------ */
/*  箭头方向解析                                                      */
/* ------------------------------------------------------------------ */
EOrionAxis UOrionStructureComponent::ResolveDirection(const FVector& Forward)
{
	static const FVector AxisVec[6] =
	{
		FVector(0, 1, 0), // East  (+Y)
		FVector(1, 0, 0), // South (+X)
		FVector(0, -1, 0), // West  (-Y)
		FVector(-1, 0, 0), // North (-X)
		FVector(0, 0, 1), // Up    (+Z)
		FVector(0, 0, -1) // Down  (-Z)
	};

	float BestDot = -1.f;
	int32 BestIdx = 0;
	const FVector FwdN = Forward.GetSafeNormal();
	for (int32 i = 0; i < 6; ++i)
	{
		const float Dot = FVector::DotProduct(FwdN, AxisVec[i]);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			BestIdx = i;
		}
	}
	return static_cast<EOrionAxis>(BestIdx);
}

/* ------------------------------------------------------------------ */
/*  点击回调：只打印方向                                               */
/* ------------------------------------------------------------------ */
void UOrionStructureComponent::OnArrowClicked(UPrimitiveComponent* ClickedComp, FKey)
{
	if (!ClickedComp) { return; }

	if (EOrionAxis* AxisPtr = ClickBoxMapping.Find(ClickedComp))
	{
		static const TCHAR* AxisName[6] =
		{
			TEXT("East"), TEXT("South"), TEXT("West"),
			TEXT("North"), TEXT("Up"), TEXT("Down")
		};

		UE_LOG(LogTemp, Warning,
		       TEXT("[StructureComponent] Arrow clicked: %s  Direction=%s"),
		       *ClickedComp->GetName(),
		       AxisName[static_cast<int32>(*AxisPtr)]);
	}
}


void UOrionStructureComponent::RegisterAllSockets() const
{
	if (!StructureMesh || !BuildingManager) { return; }

	const FVector StructureLocation = StructureMesh->GetComponentLocation();
	const FRotator StructureRotation = StructureMesh->GetComponentRotation();

	switch (OrionStructureType)
	{
	case EOrionStructure::BasicSquareFoundation: RegisterSquareFoundationSockets(StructureLocation, StructureRotation);
		break;
	case EOrionStructure::BasicTriangleFoundation: RegisterTriangleFoundationSockets(
			StructureLocation, StructureRotation);
		break;
	case EOrionStructure::Wall: RegisterWallSockets(StructureLocation, StructureRotation);
		break;
	case EOrionStructure::DoubleWall: RegisterDoubleWallSockets(StructureLocation, StructureRotation);
		break;
	case EOrionStructure::BasicRoof: RegisterAdjustableStructure(StructureLocation, StructureRotation);
		break;
	default: break;
	}
}

FVector UOrionStructureComponent::GetStructureBounds(const EOrionStructure Type)
{
	if (const FVector* BoundVector = StructureBoundMap.Find(Type))
	{
		return *BoundVector;
	}
	return FVector::ZeroVector;
}
