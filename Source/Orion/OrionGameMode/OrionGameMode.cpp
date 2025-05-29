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
#include "PhysicsEngine/RadialForceComponent.h"

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

	if (CharaManager)
	{
		CharaManager->LoadAllCharacters(GetWorld(), CharaManager->TestCharactersSet);
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
