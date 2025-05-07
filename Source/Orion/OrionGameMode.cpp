#include "OrionGameMode.h"
#include "OrionChara.h"
#include "OrionAIController.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h" // For Testing Purpose
#include "OrionActor/OrionActorOre.h"
#include "PhysicsEngine/RadialForceComponent.h"

#define ORION_CHARA_HALF_HEIGHT 88.f

AOrionGameMode::AOrionGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AOrionGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		InputComponent = NewObject<UInputComponent>(this);
		InputComponent->RegisterComponent();
		InputComponent->BindAction("TestKey1", IE_Pressed, this, &AOrionGameMode::OnTestKey1Pressed);
		InputComponent->BindAction("TestKey2", IE_Pressed, this, &AOrionGameMode::OnTestKey2Pressed);


		PlayerController->PushInputComponent(InputComponent);
	}

	/*

	TArray<AActor*> FoundOres;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionActorOre::StaticClass(), FoundOres);

	if (FoundOres.Num() < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("Need at least 3 OrionActorOre in scene to test TradingCargo!"));
		return;
	}

	// 2) 构造环形搬运路线：每个源点往下一个点运 1 件 ItemId=2
	TMap<AOrionActor*, TMap<int32, int32>> TradeRoute;
	for (int32 i = 0; i < 3; ++i)
	{
		AOrionActorOre* Src = Cast<AOrionActorOre>(FoundOres[i]);
		AOrionActorOre* Dst = Cast<AOrionActorOre>(FoundOres[(i + 1) % 3]);
		if (!Src || !Dst)
		{
			continue;
		}

		TMap<int32, int32> Cargo;
		if (i == 0)
		{
			Cargo.Add(2, 1); // ItemId = 1, 数量 = 1
		}
		else if (i == 1)
		{
			Cargo.Add(2, 1); // ItemId = 2, 数量 = 2
		}
		else
		{
			Cargo.Add(2, 1); // ItemId = 1, 数量 = 1
		}

		TradeRoute.Add(Src, Cargo);
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionChara::StaticClass(), Found);

	if (Found.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No OrionChara (or subclass) found in the level."));
		return;
	}

	// 拿第一个
	AOrionChara* Ch = Cast<AOrionChara>(Found[0]);
	if (!Ch)
	{
		UE_LOG(LogTemp, Warning, TEXT("Found actor is not AOrionChara?!"));
		return;
	}

	// 4) 把一个 TradingCargo 的 Action 丢进它的队列
	Ch->CharacterActionQueue.Actions.push_back(
		Action(
			TEXT("TestTradingCargo"),
			[Ch, TradeRoute](float DeltaTime) -> bool
			{
				return Ch->TradingCargo(TradeRoute);
			}
		)
	);

	UE_LOG(LogTemp, Log, TEXT("Enqueued TestTradingCargo action."));

*/
}

void AOrionGameMode::OnTestKey1Pressed()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PlayerController)
	{
		FHitResult HitResult;
		PlayerController->GetHitResultUnderCursor(ECC_Visibility, false, HitResult);
		if (HitResult.bBlockingHit)
		{
			AActor* ClickedActor = HitResult.GetActor();
			if (ClickedActor)
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

void AOrionGameMode::OnTestKey2Pressed()
{
	UE_LOG(LogTemp, Log, TEXT("TestKey2 Pressed!"));

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		return;
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

		// 1) 创建一个 RadialForceComponent
		URadialForceComponent* RadialForceComponent = NewObject<URadialForceComponent>(this);

		// 2) 注册到游戏世界，使其生效
		RadialForceComponent->RegisterComponent();

		// 3) 设置它的世界位置为点击点
		RadialForceComponent->SetWorldLocation(ImpactPoint);

		// 4) 配置半径、强度、是否忽略质量等
		RadialForceComponent->Radius = Radius;
		RadialForceComponent->ImpulseStrength = Strength;
		RadialForceComponent->bImpulseVelChange = bVelChange;
		RadialForceComponent->Falloff = RIF_Linear;

		// 5) 发射脉冲
		RadialForceComponent->FireImpulse();

		// 6) 应用范围伤害 ===========================

		//float DamageAmount = 0.0f; // 根据需要设置伤害值
		//float DamageRadius = Radius; // 范围

		//UGameplayStatics::ApplyRadialDamage(
		//    GetWorld(),
		//    DamageAmount,
		//    ImpactPoint,
		//    DamageRadius,
		//    UDamageType::StaticClass(),
		//    TArray<AActor*>(), // 忽略的 Actors，可留空
		//    this, // Damage Causer
		//    nullptr, // Instigator
		//    true // 全伤害，不随距离衰减
		//);

		// ===========================================

		UE_LOG(LogTemp, Log, TEXT("Fired radial impulse at %s"), *ImpactPoint.ToString());
	}

	if (ExplosionClass)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.Instigator = nullptr; // GameMode 通常没有 Instigator
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AActor* DeathEffect = World->SpawnActor<AActor>(ExplosionClass, TempLocation, FRotator::ZeroRotator,
			                                                SpawnParams);
		}
	}
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
		OrionChara->FireRange = 2000.0f;
		OrionChara->CharaSide = 1;
		OrionChara->HostileGroupsIndex.Add(0);
		OrionChara->CharaAIState = EAIState::Unavailable;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to spawn AOrionChara."));
	}
}

void AOrionGameMode::ApproveCharaAttackOnActor(std::vector<AOrionChara*> OrionCharasRequested, AActor* TargetActor,
                                               FVector HitOffset, CommandType inCommandType)
{
	if (TargetActor)
	{
		if (inCommandType == CommandType::Force)
		{
			if (!OrionCharasRequested.empty())
			{
				for (auto& each : OrionCharasRequested)
				{
					if (each->bIsCharaProcedural)
					{
						each->bIsCharaProcedural = false;
					}


					FString ActionName = FString::Printf(TEXT("AttackOnCharaLongRange-%s"), *TargetActor->GetName());

					if (each->CurrentAction)
					{
						if (each->CurrentAction->Name == ActionName)
						{
							UE_LOG(LogTemp, Warning, TEXT("Already attacking chara selected as target. "), *ActionName);
							continue;
						}

						// if switching target
						if (each->CurrentAction->Name.Contains("AttackOnCharaLongRange"))
						{
							//each->CharacterActionQueue.Actions.clear();
							each->RemoveAllActions("SwitchAttackingTarget");

							each->CharacterActionQueue.Actions.push_back(
								Action(ActionName,

								       [charPtr = each, targetChara = TargetActor, inHitOffset = HitOffset](
								       float DeltaTime) -> bool
								       {
									       return charPtr->AttackOnChara(DeltaTime, targetChara, inHitOffset);
								       }
								)
							);
							continue;
						}
					}

					each->RemoveAllActions();

					each->CharacterActionQueue.Actions.push_back(
						Action(ActionName,
						       [charPtr = each, targetChara = TargetActor, inHitOffset = HitOffset](
						       float DeltaTime) -> bool
						       {
							       return charPtr->AttackOnChara(DeltaTime, targetChara, inHitOffset);
						       }
						)
					);
				}
			}
		}
		else if (inCommandType == CommandType::Append)
		{
			if (!OrionCharasRequested.empty())
			{
				for (auto& each : OrionCharasRequested)
				{
					FString ActionName = FString::Printf(TEXT("AttackOnCharaLongRange|%s"), *TargetActor->GetName());
					each->CharacterActionQueue.Actions.push_back(
						Action(ActionName,
						       [charPtr = each, targetChara = TargetActor, inHitOffset = HitOffset](
						       float DeltaTime) -> bool
						       {
							       return charPtr->AttackOnChara(DeltaTime, targetChara, inHitOffset);
						       }
						)
					);
				}
			}
		}
	}
}

bool AOrionGameMode::ApproveCharaMoveToLocation(std::vector<AOrionChara*> OrionCharasRequested, FVector TargetLocation,
                                                CommandType inCommandType)
{
	if (inCommandType == CommandType::Force)
	{
		if (!OrionCharasRequested.empty())
		{
			for (auto& each : OrionCharasRequested)
			{
				if (each->bIsCharaProcedural)
				{
					each->bIsCharaProcedural = false;
				}


				if (each->CurrentAction)
				{
					if (each->CurrentAction->Name.Contains("MoveToLocation"))
					{
						each->RemoveAllActions("TempDoNotStopMovement");
					}
					else
					{
						each->RemoveAllActions();
					}
				}
				else
				{
					each->RemoveAllActions();
				}

				each->CharacterActionQueue.Actions.push_back(
					Action(TEXT("MoveToLocation"),
					       [charPtr = each, location = TargetLocation](float DeltaTime) -> bool
					       {
						       return charPtr->MoveToLocation(location);
					       }
					)
				);
			}
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("No AOrionChara selected."));
		return false;
	}
	if (inCommandType == CommandType::Append)
	{
		if (!OrionCharasRequested.empty())
		{
			for (auto& each : OrionCharasRequested)
			{
				each->CharacterActionQueue.Actions.push_back(
					Action(TEXT("MoveToLocation"),
					       [charPtr = each, location = TargetLocation](float DeltaTime) -> bool
					       {
						       return charPtr->MoveToLocation(location);
					       }
					)
				);
			}
			return true;
		}
	}
	return false;
}

bool AOrionGameMode::ApprovePawnMoveToLocation(std::vector<AWheeledVehiclePawn*> OrionPawnsRequested,
                                               FVector TargetLocation, CommandType inCommandType)
{
	if (inCommandType == CommandType::Force)
	{
		if (!OrionPawnsRequested.empty())
		{
			for (auto& each : OrionPawnsRequested)
			{
				OrionVehicleMoveToLocation(each, TargetLocation);
			}
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("No APawn selected."));
		return false;
	}
	if (inCommandType == CommandType::Append)
	{
		if (!OrionPawnsRequested.empty())
		{
			for (auto& each : OrionPawnsRequested)
			{
				OrionVehicleMoveToLocation(each, TargetLocation);
			}
			return true;
		}
	}
	return false;
}

void AOrionGameMode::ApproveInteractWithActor(std::vector<AOrionChara*> OrionCharasRequested, AOrionActor* TargetActor,
                                              CommandType inCommandType)
{
	if (inCommandType == CommandType::Force)
	{
		for (auto& each : OrionCharasRequested)
		{
			if (!each)
			{
				continue;
			}

			if (each->bIsCharaProcedural)
			{
				each->bIsCharaProcedural = false;
			}

			FString ActionName = FString::Printf(TEXT("InteractWithActor_%s"), *TargetActor->GetName());
			each->RemoveAllActions();
			each->CharacterActionQueue.Actions.push_back(
				Action(ActionName,
				       [charPtr = each, targetActor = TargetActor](float DeltaTime) -> bool
				       {
					       return charPtr->InteractWithActor(DeltaTime, targetActor);
				       }
				)
			);
		}
	}
	else if (inCommandType == CommandType::Append)
	{
		for (auto& each : OrionCharasRequested)
		{
			if (!each)
			{
				continue;
			}

			FString ActionName = FString::Printf(TEXT("InteractWithActor_%s"), *TargetActor->GetName());
			each->CharacterProcActionQueue.Actions.push_back(
				Action(ActionName,
				       [charPtr = each, targetActor = TargetActor](float DeltaTime) -> bool
				       {
					       return charPtr->InteractWithActor(DeltaTime, targetActor);
				       }
				)
			);
		}
	}
}

void AOrionGameMode::ApproveCollectingCargo(std::vector<AOrionChara*> OrionCharasRequested,
                                            AOrionActorStorage* TargetActor, CommandType inCommandType)
{
	if (inCommandType == CommandType::Append)
	{
		for (auto& each : OrionCharasRequested)
		{
			if (!each)
			{
				continue;
			}

			FString ActionName = FString::Printf(TEXT("CollectingCargo_%s"), *TargetActor->GetName());
			each->CharacterProcActionQueue.Actions.push_back(
				Action(ActionName,
				       [charPtr = each, targetActor = TargetActor](float DeltaTime) -> bool
				       {
					       return charPtr->CollectingCargo(targetActor);
				       }
				)
			);
		}
	}
}

void AOrionGameMode::ApproveInteractWithProduction(std::vector<AOrionChara*> OrionCharasRequested,
                                                   AOrionActorProduction* TargetActor, CommandType inCommandType)
{
	if (inCommandType == CommandType::Append)
	{
		for (auto& each : OrionCharasRequested)
		{
			if (!each)
			{
				continue;
			}

			FString ActionName = FString::Printf(TEXT("InteractWithProduction_%s"), *TargetActor->GetName());
			each->CharacterProcActionQueue.Actions.push_back(
				Action(ActionName,
				       [charPtr = each, targetActor = TargetActor](float DeltaTime) -> bool
				       {
					       return charPtr->InteractWithProduction(DeltaTime, targetActor);
				       }
				)
			);
		}
	}
	else if (inCommandType == CommandType::Force)
	{
		for (auto& each : OrionCharasRequested)
		{
			if (!each)
			{
				continue;
			}

			FString ActionName = FString::Printf(TEXT("InteractWithProduction_%s"), *TargetActor->GetName());
			each->CharacterActionQueue.Actions.push_back(
				Action(ActionName,
				       [charPtr = each, targetActor = TargetActor](float DeltaTime) -> bool
				       {
					       return charPtr->InteractWithProduction(DeltaTime, targetActor);
				       }
				)
			);
		}
	}
}
