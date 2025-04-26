// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionAIController.h"
#include "GameFramework/Actor.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

AOrionAIController::AOrionAIController()
{
	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));
	SetPerceptionComponent(*AIPerceptionComp);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->SightRadius = 1500.f; // 能看到的最大距离
	SightConfig->LoseSightRadius = 1600.f; // 失去目标的距离
	SightConfig->PeripheralVisionAngleDegrees = 90.f; // 左右视野(一侧)角度
	SightConfig->SetMaxAge(5.f); // 感知保留时长

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
}

void AOrionAIController::BeginPlay()
{
	Super::BeginPlay();

	if (AIPerceptionComp)
	{
		AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AOrionAIController::OnTargetPerceptionUpdated);
	}

	ControlledPawn = Cast<AOrionChara>(GetPawn());
}

void AOrionAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ControlledPawn == nullptr)
	{
		//UE_LOG(LogTemp, Warning, TEXT("AOrionAIController::Tick: ControlledPawn is nullptr."));
		return;
	}

	if (ControlledPawn->CharaAIState == EAIState::Defensive)
	{
		RegisterDefensiveAIActon();
	}
}


void AOrionAIController::RegisterDefensiveAIActon()
{
	if (ControlledPawn->CharaState == ECharaState::Alive && ControlledPawn->CurrentAction == nullptr && ControlledPawn
		->CharacterActionQueue.IsEmpty())
	{
		if (AOrionChara* TargetOrionChara = GetClosestOrionCharaByRelation(
			ERelation::Hostile, ControlledPawn->HostileGroupsIndex, ControlledPawn->FriendlyGroupsIndex))
		{
			UE_LOG(LogTemp, Log, TEXT("AOrionAIController::RegisterDefensiveAIActon: Found target %s"),
			       *TargetOrionChara->GetName());

			const FString ActionName = FString::Printf(TEXT("AttackOnCharaLongRange|%s"), *TargetOrionChara->GetName());
			ControlledPawn->CharacterActionQueue.Actions.push_back(
				Action(ActionName,
				       [charPtr = ControlledPawn, targetChara = TargetOrionChara, inHitOffset = FVector()](
				       float DeltaTime) -> bool
				       {
					       return charPtr->AttackOnChara(DeltaTime, targetChara, inHitOffset);
				       }
				)
			);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("AOrionAIController::RegisterDefensiveAIActon: No available target found"));
		}
	}
}


void AOrionAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if ((ControlledPawn == nullptr) || (Actor == nullptr))
	{
		return;
	}

	// Stimulus.IsActive() == true 表示当前还能“看见”该Actor
	const FString DebugMessage = FString::Printf(TEXT("%s: %s sighted"), *ControlledPawn->GetName(), *Actor->GetName());
	if (Stimulus.IsActive())
	{
	}
	else
	{
		// 一旦看不见了，通常会收到 Stimulus.IsActive() == false
		//FString DebugMessage2 = FString::Printf(TEXT("%s: Lost sight of %s"), *GetControlledPawnName(), *Actor->GetName());
	}
}

AOrionChara* AOrionAIController::GetClosestOrionCharaByRelation(
	ERelation Relation,
	const TArray<int32>& HostileGroups,
	const TArray<int32>& FriendlyGroups
) const
{
	if (!ControlledPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GetClosest] ControlledPawn is nullptr"));
		return nullptr;
	}

	/* 
	UE_LOG(LogTemp, Log, TEXT("[GetClosest] Relation: %s"),
	       Relation == ERelation::Hostile ? TEXT("Hostile") :
	       Relation == ERelation::Friendly ? TEXT("Friendly") : TEXT("Neutral"));
	UE_LOG(LogTemp, Log, TEXT("[GetClosest] HostileGroups (%d): %s"),
	       HostileGroups.Num(),
	       *FString::JoinBy(HostileGroups, TEXT(","), [](int32 Side) { return FString::FromInt(Side); })
	);
	UE_LOG(LogTemp, Log, TEXT("[GetClosest] FriendlyGroups (%d): %s"),
	       FriendlyGroups.Num(),
	       *FString::JoinBy(FriendlyGroups, TEXT(","), [](int32 Side) { return FString::FromInt(Side); })
	);
	*/

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionChara::StaticClass(), AllActors);

	AOrionChara* Closest = nullptr;
	float MinDistSquared = FLT_MAX;
	const FVector MyLocation = ControlledPawn->GetActorLocation();

	for (AActor* Actor : AllActors)
	{
		if (Actor == ControlledPawn)
		{
			continue;
		}

		AOrionChara* Other = Cast<AOrionChara>(Actor);
		if (!Other)
		{
			continue;
		}

		int32 OtherSide = Other->CharaSide;

		bool bMatch = false;
		switch (Relation)
		{
		case ERelation::Hostile:
			bMatch = HostileGroups.Contains(OtherSide);
			break;

		case ERelation::Friendly:
			bMatch = FriendlyGroups.Contains(OtherSide);
			break;

		case ERelation::Neutral:
			bMatch = !HostileGroups.Contains(OtherSide)
				&& !FriendlyGroups.Contains(OtherSide);
			break;
		}

		if (!bMatch)
		{
			UE_LOG(LogTemp, Verbose, TEXT("  - Skipped: relation mismatch"));
			continue;
		}

		float Dist2 = FVector::DistSquared(MyLocation, Other->GetActorLocation());

		if (Dist2 < MinDistSquared)
		{
			MinDistSquared = Dist2;
			Closest = Other;
		}
	}

	/*
	if (Closest)
	{
		UE_LOG(LogTemp, Log, TEXT("[GetClosest] Final Closest = %s (Dist2=%.2f)"),
		       *Closest->GetName(), MinDistSquared);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[GetClosest] No matching AOrionChara found"));
	}
	*/

	return Closest;
}
