#include "OrionGameMode.h"
#include "OrionChara.h"
#include "OrionAIController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "OrionPlayerController.h"

#define ORION_CHARA_HALF_HEIGHT 88.f

AOrionGameMode::AOrionGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AOrionGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void AOrionGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}



void AOrionGameMode::SpawnCharaInstance(FVector SpawnLocation)
{
    if (!SubclassOfOrionChara)
    {
        UE_LOG(LogTemp, Warning, TEXT("SubclassOfOrionChara is not set."));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("World is null."));
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = nullptr; // GameMode 通常没有 Instigator
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    
    float CapsuleHalfHeight = ORION_CHARA_HALF_HEIGHT;
    FVector SpawnLocationWithOffset = SpawnLocation + FVector(0.f, 0.f, CapsuleHalfHeight);
    
    AOrionChara* OrionChara = World->SpawnActor<AOrionChara>(
        SubclassOfOrionChara,
        SpawnLocationWithOffset,
        FRotator::ZeroRotator,
        SpawnParams
    );

    if (OrionChara)
    {
        UE_LOG(LogTemp, Log, TEXT("Successfully spawned AOrionChara at location: %s"), *SpawnLocation.ToString());

        // Ensure AIControllerClass is set
        if (OrionChara->AIControllerClass)
        {
            // Use SpawnDefaultController to handle AI controller assignment
            OrionChara->SpawnDefaultController();
            AController* Controller = OrionChara->GetController();
            AOrionAIController* AIController = Cast<AOrionAIController>(Controller);
            if (AIController)
            {
                UE_LOG(LogTemp, Log, TEXT("AIController possessed the character successfully."));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to assign AOrionAIController."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("OrionChara's AIControllerClass is not set."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to spawn AOrionChara."));
    }
}

