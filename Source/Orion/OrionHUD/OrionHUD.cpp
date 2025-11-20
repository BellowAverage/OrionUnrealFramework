#include "OrionHUD.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"
#include "Orion/OrionChara/OrionChara.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Orion/OrionGameInstance/OrionGameInstance.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "Orion/OrionPlayerController/OrionPlayerController.h"

void AOrionHUD::BeginPlay()
{
	Super::BeginPlay();

	/* Acquire External Game Resources Addresses */

	BuildingManagerInstance = GetGameInstance()->GetSubsystem<UOrionBuildingManager>();
	checkf(BuildingManagerInstance, TEXT("OrionHUD::BeginPlay: Unable to acquire OrionBuildingManager Subsystem."));

	OrionPlayerControllerInstance = Cast<AOrionPlayerController>(GetOwningPlayerController());
	checkf(OrionPlayerControllerInstance, TEXT("OrionHUD::BeginPlay: Unable to acquire OrionPlayerController."));

	/* Init User Widgets Blueprints Instances */

	/* 1. Developer Debug UI Layer */
	if (WB_DeveloperUIBase)
	{
		UOrionUserWidgetUIBase* DeveloperUIBase = CreateWidget<UOrionUserWidgetUIBase>(GetWorld(), WB_DeveloperUIBase);

		DeveloperUIBase->OnViewLevelUp.BindLambda([this]()
		{
			UE_LOG(LogTemp, Log, TEXT("OrionHUD::BeginPlay: View Level Up"));
			OrionPlayerControllerInstance->ViewLevel++;
		});

		DeveloperUIBase->OnViewLevelDown.BindLambda([this]()
		{
			UE_LOG(LogTemp, Log, TEXT("OrionHUD::BeginPlay: View Level Down"));
			if (OrionPlayerControllerInstance->ViewLevel > 0) OrionPlayerControllerInstance->ViewLevel--;
		});

		DeveloperUIBase->OnSaveGame.BindLambda([this]()
		{
			UE_LOG(LogTemp, Log, TEXT("OrionHUD::BeginPlay: Save Game"));
			GetWorld()->GetGameInstance<UOrionGameInstance>()->SaveGame("Developer");
		});

		DeveloperUIBase->OnLoadGame.BindLambda([this]()
		{
			UE_LOG(LogTemp, Log, TEXT("OrionHUD::BeginPlay: Load Game"));
			GetWorld()->GetGameInstance<UOrionGameInstance>()->LoadGame("Developer");
		});

		DeveloperUIBase->AddToViewport();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OrionHUD::BeginPlay: WB_DeveloperUIBase has not been set in Editor."));
	}

	/* 2. Character Info UI Layer */

	if (WB_CharaInfoPanel)
	{
		CharaInfoPanel = CreateWidget<UOrionUserWidgetCharaInfo>(GetWorld(), WB_CharaInfoPanel);
		if (CharaInfoPanel)
		{
			CharaInfoPanel->AddToViewport();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OrionHUD::BeginPlay: WB_CharaInfoPanel has not been set in Editor."));
	}

	HideCharaInfoPanel();
}

void AOrionHUD::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TickCharaInfoPanel();
}

void AOrionHUD::TickCharaInfoPanel()
{
	if (!CharaInfoPanel) return;

	if (IsShowCharaInfoPanel != IsPreviousShowCharaInfoPanel ||
		(InfoChara.IsValid() && InfoChara->GetName() != PreviousInfoChara))
	{
		if (IsShowCharaInfoPanel && InfoChara.IsValid())
		{
			CharaInfoPanel->InitCharaInfoPanelParams(InfoChara.Get());
			ShowCharaInfoPanel();
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("OrionHUD:Tick: Hide Chara Info Panel"));
			HideCharaInfoPanel();
		}

		IsPreviousShowCharaInfoPanel = IsShowCharaInfoPanel;
		PreviousInfoChara = InfoChara.IsValid() ? InfoChara->GetName() : TEXT("");
	}

	if (IsShowCharaInfoPanel && InfoChara.IsValid())
	{
		CharaInfoPanel->UpdateCharaInfo(InfoChara.Get());
	}
}


void AOrionHUD::ShowPlayerOperationMenu(const float MouseX, const float MouseY, const FHitResult& HitResult,
                                        const TArray<FString>& ArrOptionNames)
{
	if (ArrOperationAvailable.Num() > 0)
	{
		ArrOperationAvailable.Empty();
	}

	for (auto& Each : ArrOptionNames)
	{
		FString OptionName = Each;
		ArrOperationAvailable.Add(FName(*OptionName));
	}

	if (WB_PlayerOperationMenu)
	{
		if (UUserWidget* PlayerOperationMenu = CreateWidget<UUserWidget>(GetWorld(), WB_PlayerOperationMenu))
		{
			PlayerOperationMenu->AddToViewport();
			PlayerOperationMenu->SetPositionInViewport(FVector2D(MouseX, MouseY));
		}
	}
}

void AOrionHUD::DrawHUD()
{
	Super::DrawHUD();

	TickDrawOrionActorInfoInPlace();

	TickShowInfoAtMouse();
}

void AOrionHUD::TickDrawOrionActorInfoInPlace() const
{
	if (!OrionPlayerControllerInstance || !Canvas) return;

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionChara::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		AOrionChara* OrionChara = Cast<AOrionChara>(Actor);
		if (!OrionChara)
		{
			continue;
		}

		FString CurrObjectName = OrionChara->GetName();
		FString CurrObjectLocation = OrionChara->GetActorLocation().ToString();

		FString ActionQueueContent;
		for (const auto& Action : OrionChara->CharacterActionQueue.Actions)
		{
			ActionQueueContent += Action.Name + TEXT(" ");
		}
		FString CurrActionDebug = OrionChara->CurrentAction ? OrionChara->CurrentAction->Name : TEXT("None");

		FString CurrHealthDebug = FString::Printf(TEXT("Health: %f"), OrionChara->CurrHealth);

		FString CurrUnifiedAction = OrionChara->GetUnifiedActionName();

		if (CurrUnifiedAction.IsEmpty())
		{
			CurrUnifiedAction = TEXT("None");
		}

		FString CombinedText = FString::Printf(
			TEXT("Name: %s\nActionQueue: %s\nCurrHealth: %s\nCurrUnifiedAction: %s"),
			*CurrObjectName,
			//*CurrObjectLocation,
			*ActionQueueContent,
			//*CurrActionDebug,
			*CurrHealthDebug,
			*CurrUnifiedAction
		);

		// 2) Convert OrionChara's world coordinates to screen coordinates
		FVector2D ScreenPos;
		bool IsProjected = UGameplayStatics::ProjectWorldToScreen(
			OrionPlayerControllerInstance,
			// Can lift it up a bit to display text above the head
			OrionChara->GetActorLocation() + FVector(0.f, 0.f, 100.f),
			ScreenPos
		);

		// 3) Draw text on Canvas
		if (IsProjected)
		{
			// Choose a font, UE has GetLargeFont() / GetMediumFont() etc. by default
			UFont* RenderFont = GEngine->GetMediumFont();

			// First set text color
			Canvas->SetDrawColor(FColor::Yellow);

			// If shadow is needed, set FFontRenderInfo
			FFontRenderInfo RenderInfo;
			RenderInfo.bEnableShadow = true;

			// Canvas->DrawText(...) prototype in UE5.5:
			// float DrawText(const UFont*, const FString&, float X, float Y, float XScale=..., float YScale=..., const FFontRenderInfo& RenderInfo=...);

			Canvas->DrawText(
				RenderFont,
				FStringView(*CombinedText), // <- Pass FString instead of FStringView
				//CombinedText,
				ScreenPos.X,
				ScreenPos.Y,
				1.5f,
				1.5f,
				RenderInfo
			);

			// If you need to change to another color, call SetDrawColor(...) again and then draw
		}
	}
}

void AOrionHUD::TickShowInfoAtMouse()
{
	if (!PlayerOwner || !bShowActorInfo) return;

	float MouseX = 0.f, MouseY = 0.f;

	if (const bool bGotMouse = PlayerOwner->GetMousePosition(MouseX, MouseY); !bGotMouse) return;

	const UFont* RenderFont = GEngine->GetMediumFont();

	float CurrentY = MouseY;
	for (const FString& Line : InfoLines)
	{
		constexpr float YOffsetBetweenLines = 20.f;
		constexpr float XOffset = 15.f;

		Canvas->SetDrawColor(FColor::Red);

		FFontRenderInfo RenderInfo;
		RenderInfo.bEnableShadow = true;

		Canvas->DrawText(
			RenderFont,
			FStringView(*Line),
			//Line,
			MouseX + XOffset,
			CurrentY,
			1.5f,
			1.5f,
			RenderInfo
		);

		CurrentY += YOffsetBetweenLines;
	}
}


// Called by PlayerController when Hovering over an Actor
void AOrionHUD::ShowInfoAtMouse(const TArray<FString>& InLines)
{
	InfoLines = InLines;
}

void AOrionHUD::ShowCharaInfoPanel() const
{
	if (CharaInfoPanel)
	{
		CharaInfoPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void AOrionHUD::HideCharaInfoPanel() const
{
	if (CharaInfoPanel)
	{
		CharaInfoPanel->SetVisibility(ESlateVisibility::Collapsed);
		// 或者彻底：CharaPanel->RemoveFromParent();
	}
}

/* Building UI */

void AOrionHUD::ShowBuildingMenu()
{
	if (!WB_BuildingMenu)
	{
		UE_LOG(LogTemp, Error, TEXT("OrionHUD::ShowBuildingMenu: WB_BuildingMenu has not been set in Editor."));
		return;
	}

	BuildingMenu = CreateWidget<UOrionUserWidgetBuildingMenu>(
		GetWorld(), WB_BuildingMenu);

	if (BuildingMenu)
	{
		BuildingMenu->InitBuildingMenu(BuildingManagerInstance->OrionDataBuildings);

		BuildingMenu->OnBuildingOptionSelected.BindLambda(
			[&](int32 InBuildingId, bool bIsChecked)
			{
				OnBuildingOptionSelected.ExecuteIfBound(InBuildingId, bIsChecked);
			});

		BuildingMenu->OnToggleDemolishMode.BindLambda(
			[&](bool bIsChecked)
			{
				OnToggleDemolishMode.ExecuteIfBound(bIsChecked);
			});

		BuildingMenu->AddToViewport();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create Building Menu!"));
	}
}

void AOrionHUD::HideBuildingMenu()
{
	if (BuildingMenu)
	{
		BuildingMenu->RemoveFromParent();
		BuildingMenu = nullptr;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Building Menu is null!"));
	}
}
