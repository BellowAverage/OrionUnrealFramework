#include "OrionGameMode.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Orion/OrionGameInstance/OrionCharaManager.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "Orion/OrionActor/OrionActorStorage.h"
#include "Math/RandomStream.h"
#include "Orion/OrionHUD/OrionHUD.h"
#include "Orion/OrionComponents/OrionAttributeComponent.h"
#include "Orion/OrionGameInstance/OrionFactionManager.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "Math/UnrealMathUtility.h"

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

	// 初始化游戏开始时间
	GameStartTime = GetWorld()->GetTimeSeconds();
	CurrentGameTime = 0.0f;

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		InputComponent = NewObject<UInputComponent>(this);
		InputComponent->RegisterComponent();
		InputComponent->BindAction("TestKey1", IE_Pressed, this, &AOrionGameMode::OnTestKey1Pressed);
		InputComponent->BindAction("TestKey2", IE_Pressed, this, &AOrionGameMode::OnTestKey2Pressed);
		InputComponent->BindAction("TestKey3", IE_Pressed, this, &AOrionGameMode::OnTestKey3Pressed);
		InputComponent->BindAction("TestKey4", IE_Pressed, this, &AOrionGameMode::OnTestKey4Pressed);
		InputComponent->BindAction("Key5Pressed", IE_Pressed, this, &AOrionGameMode::OnTestKey5Pressed);
		InputComponent->BindAction("Key6Pressed", IE_Pressed, this, &AOrionGameMode::OnTestKey6Pressed);


		PlayerController->PushInputComponent(InputComponent);
	}

	InitSpawnTimers();
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
		float Strength = 3000.f; // 强度
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
	UE_LOG(LogTemp, Log, TEXT("TestKey2 Pressed! Spawning test characters..."));
	GenerateExplosionSimulation();

	/*UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OrionGameMode] World is null."));
		return;
	}

	// Get spawn location from player's cursor or use default location
	FVector BaseSpawnLocation = FVector::ZeroVector;
	if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0))
	{
		FHitResult HitResult;
		if (PlayerController->GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
		{
			BaseSpawnLocation = HitResult.Location;
		}
		else if (PlayerController->GetHitResultUnderCursor(ECC_GameTraceChannel1, false, HitResult))
		{
			BaseSpawnLocation = HitResult.Location;
		}
		else
		{
			// Default spawn location if no hit
			BaseSpawnLocation = FVector(0.0f, 0.0f, 200.0f);
		}
	}
	else
	{
		BaseSpawnLocation = FVector(0.0f, 0.0f, 200.0f);
	}

	// Spawn 3 characters with CharaSide = 2, HostileGroupsIndex contains 1
	for (int32 i = 0; i < 3; ++i)
	{
		FOrionCharaSpawnParams SpawnParams;
		SpawnParams.CharaSide = 2;
		SpawnParams.HostileGroupsIndex = {1};
		SpawnParams.CharaAIState = EAIState::Defensive;
		
		// Offset spawn location to avoid overlapping
		FVector SpawnLocation = BaseSpawnLocation + FVector(i * 200.0f, 0.0f, 0.0f);
		
		SpawnCharaInstance(SpawnLocation, SpawnParams);
		UE_LOG(LogTemp, Log, TEXT("[OrionGameMode] Spawned character %d: Side=2, HostileGroups=[1]"), i + 1);
	}

	// Spawn 2 characters with CharaSide = 1, HostileGroupsIndex contains 2
	for (int32 i = 0; i < 2; ++i)
	{
		FOrionCharaSpawnParams SpawnParams;
		SpawnParams.CharaSide = 1;
		SpawnParams.HostileGroupsIndex = {2};
		SpawnParams.CharaAIState = EAIState::Defensive;
		
		// Offset spawn location to avoid overlapping (spawn on the other side)
		FVector SpawnLocation = BaseSpawnLocation + FVector(0.0f, (i + 1) * 200.0f, 0.0f);
		
		SpawnCharaInstance(SpawnLocation, SpawnParams);
		UE_LOG(LogTemp, Log, TEXT("[OrionGameMode] Spawned character %d: Side=1, HostileGroups=[2]"), i + 1);
	}

	UE_LOG(LogTemp, Log, TEXT("[OrionGameMode] Finished spawning test characters."));*/
}

void AOrionGameMode::OnTestKey5Pressed()
{
	UE_LOG(LogTemp, Log, TEXT("TestKey5 Pressed! Spawning enemy character"));

	UWorld* World = GetWorld();
	if (!World) return;

	FVector SpawnLocation = FVector::ZeroVector;
	bool bFoundLocation = false;
	
	if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0))
	{
		FHitResult HitResult;
		if (PlayerController->GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
		{
			SpawnLocation = HitResult.Location;
			bFoundLocation = true;
		}
		else if (PlayerController->GetHitResultUnderCursor(ECC_GameTraceChannel1, false, HitResult))
		{
			SpawnLocation = HitResult.Location;
			bFoundLocation = true;
		}
	}

	if (!bFoundLocation)
	{
		SpawnLocation = FVector(0.0f, 0.0f, 200.0f);
	}

	SpawnEnemyCharaAt(SpawnLocation);
}

void AOrionGameMode::OnTestKey6Pressed()
{
	UE_LOG(LogTemp, Log, TEXT("TestKey6 Pressed! Spawning Player faction character"));

	UWorld* World = GetWorld();
	if (!World) return;

	FVector SpawnLocation = FVector::ZeroVector;
	bool bFoundLocation = false;
	
	if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0))
	{
		FHitResult HitResult;
		if (PlayerController->GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
		{
			SpawnLocation = HitResult.Location;
			bFoundLocation = true;
		}
		else if (PlayerController->GetHitResultUnderCursor(ECC_GameTraceChannel1, false, HitResult))
		{
			SpawnLocation = HitResult.Location;
			bFoundLocation = true;
		}
	}

	if (!bFoundLocation)
	{
		SpawnLocation = FVector(0.0f, 0.0f, 200.0f);
	}

	// Spawn character
	FOrionCharaSpawnParams SpawnParams;
	SpawnParams.CharaAIState = EAIState::Defensive;
	SpawnParams.WeaponType = EOrionWeaponType::Rifle;
	SpawnParams.InitialSpeed = 600.f;

	AOrionChara* SpawnedChara = SpawnCharaInstance(SpawnLocation, SpawnParams);
	if (SpawnedChara && SpawnedChara->AttributeComp)
	{
		SpawnedChara->AttributeComp->ActorFaction = EFaction::PlayerFaction;
		UE_LOG(LogTemp, Log, TEXT("Set spawned character faction to PlayerFaction"));
	}
}

void AOrionGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// 更新游戏时间
	UpdateGameTime(DeltaTime);

	// 处理消息队列
	ProcessMessageQueue(DeltaTime);
}

AOrionChara* AOrionGameMode::SpawnCharaInstance(const FVector& SpawnLocation, const FOrionCharaSpawnParams& SpawnParams)
{
	if (UOrionCharaManager* CharaManager = GetGameInstance()->GetSubsystem<UOrionCharaManager>())
	{
		return CharaManager->SpawnCharaInstance(SpawnLocation, SpawnParams, SubclassOfOrionChara);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[OrionGameMode] Failed to get OrionCharaManager subsystem."));
	}
	return nullptr;
}

void AOrionGameMode::UpdateGameTime(float DeltaTime)
{
	// 累加游戏时间（应用时间比例）
	CurrentGameTime += DeltaTime * TimeScale;

	// 获取第一个玩家控制器的 HUD 并更新 GameClock
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (AOrionHUD* OrionHUD = Cast<AOrionHUD>(PC->GetHUD()))
		{
			if (OrionHUD->DeveloperUIBase && OrionHUD->DeveloperUIBase->GameClock)
			{
				FString FormattedTime = FormatGameTime(CurrentGameTime);
				OrionHUD->DeveloperUIBase->GameClock->SetText(FText::FromString(FormattedTime));
			}
		}
	}
}

FString AOrionGameMode::FormatGameTime(float GameTimeInSeconds) const
{
	// 计算天数（每24小时 = 86400秒 算1天）
	constexpr float SecondsPerDay = 24.0f * 60.0f * 60.0f; // 86400秒
	int32 Days = FMath::FloorToInt(GameTimeInSeconds / SecondsPerDay);
	
	// 计算当天剩余秒数
	float RemainingSeconds = FMath::Fmod(GameTimeInSeconds, SecondsPerDay);
	
	// 计算小时和分钟
	int32 Hours = FMath::FloorToInt(RemainingSeconds / 3600.0f);
	int32 Minutes = FMath::FloorToInt(FMath::Fmod(RemainingSeconds, 3600.0f) / 60.0f);
	
	// 格式化为 "Day X | HH:MM"
	return FString::Printf(TEXT("Day %d | %02d:%02d"), Days, Hours, Minutes);
}

void AOrionGameMode::InitSpawnTimers()
{
	if (!GetWorld())
	{
		return;
	}

	// 敌人波次
	GetWorldTimerManager().ClearTimer(EnemyWaveTimerHandle);
	GetWorldTimerManager().SetTimer(
		EnemyWaveTimerHandle,
		this,
		&AOrionGameMode::SpawnEnemyWave,
		EnemyWaveInterval,
		true,
		EnemyWaveInterval);

	// 友军增援
	GetWorldTimerManager().ClearTimer(AllySpawnTimerHandle);
	GetWorldTimerManager().SetTimer(
		AllySpawnTimerHandle,
		this,
		&AOrionGameMode::SpawnAllyReinforcement,
		AllySpawnInterval,
		true,
		AllySpawnInterval);
}

void AOrionGameMode::SpawnEnemyWave()
{
	const int32 EnemyCount = GetCurrentWaveEnemyCount();
	const int32 WaveNumber = CurrentWaveIndex + 1;
	const FString WaveOrdinal = GetWaveOrdinal(WaveNumber);
	EnqueueGameMessage(FString::Printf(TEXT("%s Wave of %d Enemies is Approaching!!"), *WaveOrdinal, EnemyCount));

	for (int32 Index = 0; Index < EnemyCount; ++Index)
	{
		const FVector SpawnLocation = GetRandomPointInAnnulus(EnemySpawnInnerRadius, EnemySpawnOuterRadius);
		SpawnEnemyCharaAt(SpawnLocation);
	}

	// 递进波次，超过配置后锁定在最大值（30）
	if (EnemyWaveCounts.Num() > 0 && CurrentWaveIndex < EnemyWaveCounts.Num() - 1)
	{
		++CurrentWaveIndex;
	}
}

void AOrionGameMode::SpawnAllyReinforcement()
{
	// 尝试在原点生成，如果碰撞则稍微偏移
	const FVector SpawnLocation = FindValidSpawnLocationNearOrigin(200.f);
	if (AOrionChara* Ally = SpawnFriendlyCharaAt(SpawnLocation))
	{
		// 友军移动到原点附近 300 范围内的随机位置
		const FVector Target = GetRandomPointInRadius(300.f);
		IssueMoveOrder(Ally, Target);
	}
}

int32 AOrionGameMode::GetCurrentWaveEnemyCount() const
{
	if (EnemyWaveCounts.Num() == 0)
	{
		return 0;
	}

	if (EnemyWaveCounts.IsValidIndex(CurrentWaveIndex))
	{
		return EnemyWaveCounts[CurrentWaveIndex];
	}

	return EnemyWaveCounts.Last();
}

FVector AOrionGameMode::GetRandomPointInAnnulus(float MinRadius, float MaxRadius) const
{
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Radius = FMath::FRandRange(MinRadius, MaxRadius);
	return FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
}

FVector AOrionGameMode::GetRandomPointInRadius(float Radius) const
{
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Length = FMath::Sqrt(FMath::FRand()) * Radius;
	return FVector(FMath::Cos(Angle) * Length, FMath::Sin(Angle) * Length, 0.f);
}

FVector AOrionGameMode::FindValidSpawnLocationNearOrigin(float MaxOffsetRadius) const
{
	UWorld* World = GetWorld();
	if (!World || !SubclassOfOrionChara)
	{
		return FVector::ZeroVector;
	}

	// 获取角色的胶囊体参数
	const AOrionChara* CharaCDO = SubclassOfOrionChara->GetDefaultObject<AOrionChara>();
	if (!CharaCDO)
	{
		return FVector::ZeroVector;
	}

	UCapsuleComponent* CapsuleComp = CharaCDO->GetCapsuleComponent();
	if (!CapsuleComp)
	{
		return FVector::ZeroVector;
	}

	const float CapsuleRadius = CapsuleComp->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = CapsuleComp->GetScaledCapsuleHalfHeight();

	// 首先尝试原点
	FVector TestLocation = FVector::ZeroVector;
	
	// 投射到地面
	const FVector TraceStart(TestLocation.X, TestLocation.Y, TestLocation.Z + 10000.f);
	const FVector TraceEnd(TestLocation.X, TestLocation.Y, TestLocation.Z - 10000.f);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FindValidSpawnLocation), true);
	
	FHitResult HitResult;
	if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_GameTraceChannel1, QueryParams))
	{
		TestLocation = HitResult.Location + FVector(0.f, 0.f, CapsuleHalfHeight);
	}
	else
	{
		TestLocation = FVector(0.f, 0.f, CapsuleHalfHeight);
	}

	// 检查原点是否可以生成（碰撞检测）
	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(FindValidSpawnLocation), false);
	
	if (!World->OverlapBlockingTestByChannel(TestLocation, FQuat::Identity, ECC_Pawn, CapsuleShape, OverlapParams))
	{
		// 原点可以生成，直接返回
		return TestLocation - FVector(0.f, 0.f, CapsuleHalfHeight);
	}

	// 原点碰撞，在附近寻找有效位置
	const int32 MaxAttempts = 20;
	const float StepRadius = MaxOffsetRadius / MaxAttempts;
	
	for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
	{
		const float CurrentRadius = StepRadius * Attempt;
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const FVector Offset = FVector(FMath::Cos(Angle) * CurrentRadius, FMath::Sin(Angle) * CurrentRadius, 0.f);
		
		FVector CandidateLocation = FVector::ZeroVector + Offset;
		
		// 投射到地面
		const FVector CandidateTraceStart(CandidateLocation.X, CandidateLocation.Y, CandidateLocation.Z + 10000.f);
		const FVector CandidateTraceEnd(CandidateLocation.X, CandidateLocation.Y, CandidateLocation.Z - 10000.f);
		
		if (World->LineTraceSingleByChannel(HitResult, CandidateTraceStart, CandidateTraceEnd, ECC_GameTraceChannel1, QueryParams))
		{
			CandidateLocation = HitResult.Location + FVector(0.f, 0.f, CapsuleHalfHeight);
		}
		else
		{
			CandidateLocation = FVector(CandidateLocation.X, CandidateLocation.Y, CapsuleHalfHeight);
		}

		// 检查是否碰撞
		if (!World->OverlapBlockingTestByChannel(CandidateLocation, FQuat::Identity, ECC_Pawn, CapsuleShape, OverlapParams))
		{
			// 找到有效位置
			return CandidateLocation - FVector(0.f, 0.f, CapsuleHalfHeight);
		}
	}

	// 如果所有尝试都失败，返回原点（让 SpawnActor 自动调整）
	return FVector::ZeroVector;
}

AOrionChara* AOrionGameMode::SpawnEnemyCharaAt(const FVector& SpawnLocation)
{
	FOrionCharaSpawnParams SpawnParams;

	SpawnParams.CharaAIState = EAIState::Defensive;
	SpawnParams.WeaponType = EOrionWeaponType::Shotgun;
	SpawnParams.InitialSpeed = 200.f;

	AOrionChara* Enemy = SpawnCharaInstance(SpawnLocation, SpawnParams);
	if (Enemy && Enemy->AttributeComp)
	{
		Enemy->AttributeComp->ActorFaction = EFaction::Vagrants;
	}
	return Enemy;
}

AOrionChara* AOrionGameMode::SpawnFriendlyCharaAt(const FVector& SpawnLocation)
{
	FOrionCharaSpawnParams SpawnParams;
	SpawnParams.CharaAIState = EAIState::Defensive;
	SpawnParams.WeaponType = EOrionWeaponType::Rifle;
	SpawnParams.InitialSpeed = 400.f;

	AOrionChara* Ally = SpawnCharaInstance(SpawnLocation, SpawnParams);
	if (Ally && Ally->AttributeComp)
	{
		Ally->AttributeComp->ActorFaction = EFaction::PlayerFaction;
	}
	return Ally;
}

void AOrionGameMode::IssueMoveOrder(AOrionChara* Chara, const FVector& TargetLocation) const
{
	if (!Chara)
	{
		return;
	}

	if (UOrionCharaManager* Manager = GetGameInstance()->GetSubsystem<UOrionCharaManager>())
	{
		Manager->AddMoveToLocationAction(Chara, TargetLocation, EActionExecution::RealTime);
	}
}

void AOrionGameMode::EnqueueGameMessage(const FString& Message)
{
	PendingMessages.Add(Message);
}

void AOrionGameMode::ProcessMessageQueue(float DeltaTime)
{
	if (PendingMessages.Num() == 0)
	{
		return;
	}

	MessageCooldown -= DeltaTime;
	if (MessageCooldown > 0.f)
	{
		return;
	}

	const FString Message = PendingMessages[0];
	PendingMessages.RemoveAt(0);
	DisplayGameMessage(Message);
	MessageCooldown = MessageDisplayInterval;
}

void AOrionGameMode::DisplayGameMessage(const FString& Message) const
{
	UE_LOG(LogTemp, Log, TEXT("[GameMessage] %s"), *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, MessageDisplayInterval, FColor::Yellow, Message);
	}
}

FString AOrionGameMode::GetWaveOrdinal(int32 WaveNumber) const
{
	switch (WaveNumber)
	{
	case 1: return TEXT("First");
	case 2: return TEXT("Second");
	case 3: return TEXT("Third");
	case 4: return TEXT("Fourth");
	case 5: return TEXT("Fifth");
	case 6: return TEXT("Sixth");
	case 7: return TEXT("Seventh");
	case 8: return TEXT("Eighth");
	case 9: return TEXT("Ninth");
	case 10: return TEXT("Tenth");
	default:
		return FString::Printf(TEXT("%dth"), WaveNumber);
	}
}
