// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "OrionChara.h"

// Sets default values
AOrionProjectile::AOrionProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bHasHit = false;

	// 1. 创建一个空的 SceneComponent 作为根组件
	USceneComponent* DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	RootComponent = DefaultRoot;

	// 2. 创建 ArrowMesh 并将其附加到根组件
	ArrowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowMesh"));
	ArrowMesh->SetupAttachment(RootComponent);
	//ArrowMesh->SetSimulatePhysics(true);
	ArrowMesh->SetNotifyRigidBodyCollision(true); // 是否生成刚体碰撞事件
	ArrowMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ArrowMesh->OnComponentHit.AddDynamic(this, &AOrionProjectile::OnHit);
	ArrowMesh->SetCanEverAffectNavigation(false); // 不参与导航系统

	// 3. 创建 ProjectileMovement 并与 ArrowMesh 关联
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = ArrowMesh;
	//ProjectileMovement->InitialSpeed = 3000.0f;
	//ProjectileMovement->MaxSpeed = 3000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	//ProjectileMovement->ProjectileGravityScale = 1.0f;
}

// Called when the game starts or when spawned
void AOrionProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (OrionProjectileType == EOrionProjectile::Bullet)
	{
		SetLifeSpan(3.0f);
	}
	else
	{
		SetLifeSpan(5.0f);
	}
}

// Called every frame
void AOrionProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (ProjectileMovement && ArrowMesh)
	{
		FVector Velocity = ProjectileMovement->Velocity;

		// 如果速度足够大，调整箭矢的旋转
		if (!Velocity.IsNearlyZero())
		{
			FRotator DesiredRotation = Velocity.Rotation();
			ArrowMesh->SetWorldRotation(DesiredRotation);
		}
	}
}

void AOrionProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                             FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHasHit)
	{
		return;
	}
	bHasHit = true;

	//UE_LOG(LogTemp, Log, TEXT("OnHit Called."));

	if (OtherActor && OtherActor != this && OtherComp)
	{
		UGameplayStatics::ApplyDamage(OtherActor, ProjectileDamage, nullptr, this, nullptr);

		this->Destroy();

		AOrionChara* OrionCharacter = Cast<AOrionChara>(OtherActor);

		if (OrionCharacter) // && OrionCharacter->CurrHealth > 0.f
		{
			//UE_LOG(LogTemp, Log, TEXT("Hit on OrionChara. "));
			if (ArrowMesh && ArrowMesh->GetStaticMesh())
			{
				FVector ProjectileDirection = GetVelocity().GetSafeNormal();
				if (OrionProjectileType == EOrionProjectile::Arrow)
				{
					OrionCharacter->SpawnArrowPenetrationEffect(Hit.Location, ProjectileDirection,
					                                            ArrowMesh->GetStaticMesh());
				}
			}
		}
	}
	else
	{
		if (OrionProjectileType == EOrionProjectile::Bullet)
		{
			this->Destroy();
		}
	}
}
