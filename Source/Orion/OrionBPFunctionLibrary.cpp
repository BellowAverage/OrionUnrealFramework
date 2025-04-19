#include "OrionBPFunctionLibrary.h"
#include "Math/UnrealMathUtility.h"
#include <string>
#include <numbers>
#include <Windows.h>

void UOrionBPFunctionLibrary::OrionPrint(FString String2Print)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Blue, String2Print);
	}
}

float UOrionBPFunctionLibrary::TimeAccumulated = 0.0f;
/*
* Defulat Amplitude equals -1 to set Amplitude as pi
| Amplitude (Radians) | Maximum Output | Minimum Output |
|---------------------|----------------|----------------|
| 0.0                 | 0.0            | 0.0            |
| 0.5                 | 0.496          | -0.496         |
| 1.0                 | 0.958          | -0.958         |
| 1.047               | 1.0            | -1.0           |
| 1.5                 | 1.363          | -1.363         |
| 2.0                 | 1.682          | -1.682         |
| 3.14                | 2.0            | -2.0           |
*/
float UOrionBPFunctionLibrary::OrionOscillation(float DeltaTime, float OscillatingSpeed, float Amplitude)
{
	if (Amplitude == -1.f)
	{
		Amplitude = std::numbers::pi;
	}
	TimeAccumulated += DeltaTime * OscillatingSpeed;
	return FMath::Sin(TimeAccumulated + Amplitude) - FMath::Sin(TimeAccumulated);
}

void UOrionBPFunctionLibrary::FindSourceInURL(const FString& url)
{
	std::string urlStr(TCHAR_TO_UTF8(*url));
	std::wstring wideURL(urlStr.begin(), urlStr.end());
	ShellExecuteW(NULL, L"open", wideURL.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void UOrionBPFunctionLibrary::DisablePhysicsOnActor(AActor* TargetActor)
{
    if (TargetActor)
    {
        DisablePhysicsAndCollisionForAllComponents(TargetActor);
        UE_LOG(LogTemp, Log, TEXT("Disabled physics and collision for all StaticMeshComponents of %s"), *TargetActor->GetName());

        // 递归禁用子Actor
        TArray<AActor*> ChildActors;
        TargetActor->GetAttachedActors(ChildActors);
        for (AActor* Child : ChildActors)
        {
            DisablePhysicsOnActor(Child);
        }
    }
}

void UOrionBPFunctionLibrary::DisablePhysicsAndCollision(UActorComponent* Component)
{
    if (!Component)
        return;

    // 仅处理静态网格体组件
    if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Component))
    {
        StaticMeshComp->SetSimulatePhysics(false);
        StaticMeshComp->SetEnableGravity(false);
        StaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        UE_LOG(LogTemp, Log, TEXT("Disabled physics for StaticMeshComponent: %s"), *StaticMeshComp->GetName());
    }
    else
    {
        // 其他组件类型不处理
        UE_LOG(LogTemp, Warning, TEXT("Component %s is not a StaticMeshComponent and was skipped."), *Component->GetName());
    }
}

void UOrionBPFunctionLibrary::DisablePhysicsAndCollisionForAllComponents(AActor* TargetActor)
{
    if (!TargetActor)
        return;

    // 遍历目标 Actor 的所有组件
    TArray<UActorComponent*> Components;
    TargetActor->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        // 仅禁用静态网格体组件的物理和碰撞
        DisablePhysicsAndCollision(Component);
    }
}

bool UOrionBPFunctionLibrary::CheckActorPhysicsStatus(AActor* TargetActor)
{
    if (!TargetActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("TargetActor is null!"));
        return true;
    }

    bool bAllDisabled = true;

    // 检查当前Actor的静态网格体组件
    TArray<UActorComponent*> Components;
    TargetActor->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Component))
        {
            bool bSimulatePhysics = StaticMeshComp->IsSimulatingPhysics();
            bool bEnableGravity = StaticMeshComp->IsGravityEnabled();
            ECollisionEnabled::Type CollisionStatus = StaticMeshComp->GetCollisionEnabled();
            bool bCollisionEnabled = CollisionStatus != ECollisionEnabled::NoCollision;

            if (bSimulatePhysics || bEnableGravity || bCollisionEnabled)
            {
                bAllDisabled = false;
                UE_LOG(LogTemp, Warning, TEXT("StaticMeshComponent with enabled feature: %s"), *StaticMeshComp->GetName());
            }
        }
    }

    // 检查子Actor的物理状态
    TArray<AActor*> ChildActors;
    TargetActor->GetAttachedActors(ChildActors);
    for (AActor* Child : ChildActors)
    {
        if (!CheckActorPhysicsStatus(Child))
        {
            bAllDisabled = false;
        }
    }

    if (bAllDisabled)
    {
        UE_LOG(LogTemp, Log, TEXT("All StaticMeshComponents of %s and its children have physics, gravity, and collision disabled."), *TargetActor->GetName());
    }

    return bAllDisabled;
}

