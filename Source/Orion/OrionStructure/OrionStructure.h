// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "OrionStructure.generated.h"

USTRUCT()
struct FFoundationSocket
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsOccupied = false;

	UPROPERTY()
	FVector SocketLocation;

	UPROPERTY()
	FRotator SocketRotation;

	FFoundationSocket()
		: bIsOccupied(false)
		  , SocketLocation(FVector::ZeroVector)
		  , SocketRotation(FRotator::ZeroRotator)
	{
	}

	FFoundationSocket(
		bool InbIsOccupied,
		FVector InSocketLocation,
		FRotator InSocketRotator)
		: bIsOccupied(InbIsOccupied)
		  , SocketLocation(InSocketLocation)
		  , SocketRotation(InSocketRotator)
	{
	}
};

USTRUCT()
struct FWallSocket
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsOccupied = false;

	UPROPERTY()
	FVector SocketLocation;

	UPROPERTY()
	FRotator SocketRotation;

	FWallSocket()
		: bIsOccupied(false)
		  , SocketLocation(FVector::ZeroVector)
		  , SocketRotation(FRotator::ZeroRotator)
	{
	}

	FWallSocket(
		bool InbIsOccupied,
		FVector InSocketLocation,
		FRotator InSocketRotator)
		: bIsOccupied(InbIsOccupied)
		  , SocketLocation(InSocketLocation)
		  , SocketRotation(InSocketRotator)
	{
	}
};

UENUM()
enum class EOrionStructure : uint8
{
	Foundation,
	Wall
};

USTRUCT()
struct FOrionGlobalSocket
{
	GENERATED_BODY()

	FVector Location;
	FRotator Rotation;
	EOrionStructure Kind;
	TWeakObjectPtr<AActor> Owner; // 谁登记的

	FOrionGlobalSocket() = default;

	FOrionGlobalSocket(const FVector& InLoc,
	                   const FRotator& InRot,
	                   EOrionStructure InKind,
	                   AActor* InOwner)
		: Location(InLoc), Rotation(InRot), Kind(InKind), Owner(InOwner)
	{
	}
};


UCLASS()
class ORION_API AOrionStructure : public AActor
{
	GENERATED_BODY()

public:
	AOrionStructure();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/* Basics */

	bool bForceSnapOnGrid = false;


	/* 全局注册函数 —— 所有子类在 BeginPlay 末尾调用 */
	static void RegisterSocket(const FVector& Loc,
	                           const FRotator& Rot,
	                           EOrionStructure Kind,
	                           bool bOccupied,
	                           UWorld* World,
	                           AActor* Owner);

	static void RefreshDebug(UWorld* World);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	UStaticMeshComponent* StructureMesh;

	static inline TArray<FOrionGlobalSocket> FreeSockets;
	static inline TArray<FOrionGlobalSocket> OccupiedSockets;

	static constexpr float MergeTolSqr = 5.f * 5.f; // 5 cm²

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/* ① 只读访问：获取所有未占用 socket */
	static const TArray<FOrionGlobalSocket>& GetFreeSockets()
	{
		return FreeSockets;
	}

	/* ② 查询某个位置是否仍为“未占用” */
	static bool IsSocketFree(const FVector& Loc, EOrionStructure Kind);

	static inline int32 AliveCount = 0;
};
