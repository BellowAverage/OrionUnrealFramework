#include "OrionGameMode.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Orion/OrionAIController/OrionAIController.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Orion/OrionGameInstance/OrionCharaManager.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "Orion/OrionActor/OrionActorOre.h"
#include "Orion/OrionActor/OrionActorProduction.h"
#include "Orion/OrionInterface/OrionInterfaceActionable.h"
#include "Math/RandomStream.h"

#define ORION_CHARA_HALF_HEIGHT 88.f

AOrionGameMode::AOrionGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AOrionGameMode::OnTestKey3Pressed()
{
}

void AOrionGameMode::OnTestKey4Pressed()
{
}

void AOrionGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("OrionGameMode: Now using VSCode Editor. "));

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		InputComponent = NewObject<UInputComponent>(this);
		InputComponent->RegisterComponent();
		InputComponent->BindAction("TestKey1", IE_Pressed, this, &AOrionGameMode::OnTestKey1Pressed);
		InputComponent->BindAction("TestKey2", IE_Pressed, this, &AOrionGameMode::OnTestKey2Pressed);
		InputComponent->BindAction("TestKey3", IE_Pressed, this, &AOrionGameMode::OnTestKey3Pressed);
		InputComponent->BindAction("TestKey4", IE_Pressed, this, &AOrionGameMode::OnTestKey4Pressed);


		PlayerController->PushInputComponent(InputComponent);
	}
}

void AOrionGameMode::OnTestKey1Pressed()
{
	if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		FHitResult HitResult;
		PlayerController->GetHitResultUnderCursor(ECC_Visibility, false, HitResult);
		if (HitResult.bBlockingHit)
		{
			if (AActor* ClickedActor = HitResult.GetActor())
			{
				if (AOrionActor* TestOrionActor = Cast<AOrionActor>(ClickedActor))
				{
					FDamageEvent DamageEvent; // Temporary FDamageEvent for testing purposes
					TestOrionActor->TakeDamage(1.0f, FDamageEvent(), PlayerController->GetInstigatorController(), this);
					if (TestOrionActor->InventoryComp && TestOrionActor->InventoryComp->InventoryMap.Find(1) != nullptr)
					{
						TestOrionActor->InventoryComp->ModifyItemQuantity(1, -1);
					}
					if (TestOrionActor->InventoryComp && TestOrionActor->InventoryComp->InventoryMap.Find(2) != nullptr)
					{
						TestOrionActor->InventoryComp->ModifyItemQuantity(2, -1);
					}
					if (TestOrionActor->InventoryComp && TestOrionActor->InventoryComp->InventoryMap.Find(3) != nullptr)
					{
						TestOrionActor->InventoryComp->ModifyItemQuantity(3, -1);
					}
				}
				else if (AOrionChara* TestOrionChara = Cast<AOrionChara>(ClickedActor))
				{
					FDamageEvent DamageEvent; // Temporary FDamageEvent for testing purposes
					TestOrionChara->TakeDamage(10.f, FDamageEvent(), PlayerController->GetInstigatorController(), this);
				}
			}
		}
	}
}

bool AOrionGameMode::GenerateExplosionSimulation()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		return true;
	}

	FHitResult HitResult;
	bool bHit = PC->GetHitResultUnderCursor(ECC_Visibility, false, HitResult);

	if (!bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("HitResult is not blocking hit. Trying to use GameTraceChannel1."));
		bHit = PC->GetHitResultUnderCursor(ECC_GameTraceChannel1, false, HitResult);
	}

	FVector TempLocation = HitResult.Location;

	if (bHit && HitResult.bBlockingHit)
	{
		FVector ImpactPoint = HitResult.ImpactPoint;
		float Radius = 500.f; // 范围
		float Strength = 2000.f; // 强度
		bool bVelChange = true;

		// —— 调试：在世界中绘制一个红色球体来显示影响范围 —— 
		DrawDebugSphere(
			GetWorld(),
			ImpactPoint,
			Radius,
			32, // 分段数
			FColor::Red,
			false, // 不持续
			5.f // 存续 5 秒
		);

		// —— 新增：先对所有在范围内的 OrionChara 调用 BlueprintNativeVelocityExceeded —— 
		for (TActorIterator<AOrionChara> It(GetWorld()); It; ++It)
		{
			AOrionChara* Chara = *It;
			if (!Chara)
			{
				continue;
			}

			// 距离判断
			float Dist2 = FVector::DistSquared(Chara->GetActorLocation(), ImpactPoint);
			if (Dist2 <= Radius * Radius)
			{
				// 调用角色自己的 BlueprintNative 事件
				Chara->BlueprintNativeVelocityExceeded();
				UE_LOG(LogTemp, Log,
				       TEXT("Called VelocityExceeded on %s (Dist=%.1f)"),
				       *Chara->GetName(), FMath::Sqrt(Dist2));
			}
		}

		// —— 原有：创建并发射径向力 —— 
		URadialForceComponent* RadialForceComponent = NewObject<URadialForceComponent>(this);
		RadialForceComponent->RegisterComponent();
		RadialForceComponent->SetWorldLocation(ImpactPoint);
		RadialForceComponent->Radius = Radius;
		RadialForceComponent->ImpulseStrength = Strength;
		RadialForceComponent->bImpulseVelChange = bVelChange;
		RadialForceComponent->Falloff = RIF_Linear;
		RadialForceComponent->FireImpulse();

		UE_LOG(LogTemp, Log, TEXT("Fired radial impulse at %s"), *ImpactPoint.ToString());
	}

	// 原有爆炸效果生成逻辑保持不变
	if (ExplosionClass)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			World->SpawnActor<AActor>(
				ExplosionClass,
				bHit ? HitResult.Location : FVector::ZeroVector,
				FRotator::ZeroRotator,
				SpawnParams
			);
		}
	}
	return false;
}

void AOrionGameMode::OnTestKey2Pressed()
{
	UE_LOG(LogTemp, Log, TEXT("TestKey2 Pressed!"));

	/*if (GenerateExplosionSimulation()) return;*/

	UOrionCharaManager* CharaManager = GetGameInstance()->GetSubsystem<UOrionCharaManager>();

	if (!CharaManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("CharaManager is null!"));
		return;
	}

	// Load all characters
	CharaManager->LoadAllCharacters(GetWorld(), CharaManager->TestCharactersSet);

	// Check if there are any characters
	if (CharaManager->GlobalCharaMap.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("No characters found in GlobalCharaMap!"));
		return;
	}

	// Initialize random stream with seed for reproducibility
	FRandomStream RandomStream(TestRandomSeed);
	UE_LOG(LogTemp, Log, TEXT("Using random seed: %d"), TestRandomSeed);

	// Collect all available Ores and Productions
	TArray<AActor*> FoundOres;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionActorOre::StaticClass(), FoundOres);

	TArray<AActor*> FoundProduction;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionActorProduction::StaticClass(), FoundProduction);

	if (FoundOres.IsEmpty() && FoundProduction.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("No Ores or Productions found in the world!"));
		return;
	}

	// Convert to typed arrays for easier access
	TArray<AOrionActorOre*> AvailableOres;
	for (AActor* Actor : FoundOres)
	{
		if (AOrionActorOre* Ore = Cast<AOrionActorOre>(Actor))
		{
			AvailableOres.Add(Ore);
		}
	}

	TArray<AOrionActorProduction*> AvailableProductions;
	for (AActor* Actor : FoundProduction)
	{
		if (AOrionActorProduction* Production = Cast<AOrionActorProduction>(Actor))
		{
			AvailableProductions.Add(Production);
		}
	}

	// Iterate through all characters and assign random tasks
	int32 CharaIndex = 0;
	for (const TPair<FGuid, TWeakObjectPtr<AOrionChara>>& Pair : CharaManager->GlobalCharaMap)
	{
		AOrionChara* Chara = Pair.Value.Get();
		if (!Chara)
		{
			continue;
		}

		IOrionInterfaceActionable* CharaInterface = Cast<IOrionInterfaceActionable>(Chara);
		if (!CharaInterface)
		{
			UE_LOG(LogTemp, Warning, TEXT("Chara %s does not implement IOrionInterfaceActionable!"), *Chara->GetName());
			continue;
		}

		// Determine number of tasks for this character (random between MinTasksPerChara and MaxTasksPerChara)
		int32 NumTasks = RandomStream.RandRange(MinTasksPerChara, MaxTasksPerChara);
		UE_LOG(LogTemp, Log, TEXT("Assigning %d tasks to Chara %d (%s)"), NumTasks, CharaIndex, *Chara->GetName());

		// Assign random tasks
		for (int32 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
		{
			// Randomly choose between Mining (Ore) and Production
			bool bChooseMining = RandomStream.FRand() < 0.5f;

			if (bChooseMining && !AvailableOres.IsEmpty())
			{
				// Randomly select an Ore
				int32 OreIndex = RandomStream.RandRange(0, AvailableOres.Num() - 1);
				AOrionActorOre* SelectedOre = AvailableOres[OreIndex];

				FOrionAction MiningAction = CharaInterface->InitActionInteractWithActor(
					FString::Printf(TEXT("TestMiningAction_Chara%d_Task%d"), CharaIndex, TaskIndex), SelectedOre);
				Chara->InsertOrionActionToQueue(MiningAction, EActionExecution::Procedural, -1);
				UE_LOG(LogTemp, Log, TEXT("  - Assigned Mining task to Ore: %s"), *SelectedOre->GetName());
			}
			else if (!AvailableProductions.IsEmpty())
			{
				// Randomly select a Production
				int32 ProductionIndex = RandomStream.RandRange(0, AvailableProductions.Num() - 1);
				AOrionActorProduction* SelectedProduction = AvailableProductions[ProductionIndex];

				FOrionAction ProductionAction = CharaInterface->InitActionInteractWithProduction(
					FString::Printf(TEXT("TestProductionAction_Chara%d_Task%d"), CharaIndex, TaskIndex), SelectedProduction);
				Chara->InsertOrionActionToQueue(ProductionAction, EActionExecution::Procedural, -1);
				UE_LOG(LogTemp, Log, TEXT("  - Assigned Production task to: %s"), *SelectedProduction->GetName());
			}
			else if (!AvailableOres.IsEmpty())
			{
				// Fallback to Ore if Production is not available
				int32 OreIndex = RandomStream.RandRange(0, AvailableOres.Num() - 1);
				AOrionActorOre* SelectedOre = AvailableOres[OreIndex];

				FOrionAction MiningAction = CharaInterface->InitActionInteractWithActor(
					FString::Printf(TEXT("TestMiningAction_Chara%d_Task%d"), CharaIndex, TaskIndex), SelectedOre);
				Chara->InsertOrionActionToQueue(MiningAction, EActionExecution::Procedural, -1);
				UE_LOG(LogTemp, Log, TEXT("  - Assigned Mining task (fallback) to Ore: %s"), *SelectedOre->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("  - No available tasks for Chara %d, Task %d"), CharaIndex, TaskIndex);
			}
		}

		// Set character as procedural
		Chara->bIsCharaProcedural = true;
		CharaIndex++;
	}

	UE_LOG(LogTemp, Log, TEXT("Task assignment completed for %d characters"), CharaIndex);
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
	SpawnParams.Instigator = nullptr;
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

		OrionChara->MaxHealth = 10.0f;

		OrionChara->CombatComp->FireRange = 2000.0f;
		OrionChara->CharaSide = 1;
		OrionChara->HostileGroupsIndex.Add(0);
		OrionChara->CharaAIState = EAIState::Unavailable;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to spawn AOrionChara."));
	}
}
