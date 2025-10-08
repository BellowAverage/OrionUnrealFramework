#include "OrionHUD.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"
#include "Orion/OrionChara/OrionChara.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Orion/OrionGameInstance/OrionGameInstance.h"
#include <vector>

#include "Orion/OrionPlayerController/OrionPlayerController.h"


const TArray<FOrionDataBuilding> AOrionHUD::BuildingMenuOptions = {
	{
		1, FName(TEXT("SquareFoundation")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/SquareFoundationSnapshot.SquareFoundationSnapshot")),
		FString(TEXT(
			"/Game/_Orion/Blueprints/Buildings/BP_OrionStructureSquareFoundation.BP_OrionStructureSquareFoundation_C")),
		EOrionStructure::BasicSquareFoundation
	},
	{
		2, FName(TEXT("TriangleFoundation")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/TriangleFoundationSnapshot.TriangleFoundationSnapshot")),
		FString(TEXT(
			"/Game/_Orion/Blueprints/Buildings/BP_OrionStructureTriangleFoundation.BP_OrionStructureTriangleFoundation_C")),
		EOrionStructure::BasicTriangleFoundation
	},
	{
		3, FName(TEXT("Wall")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/WallSnapshot.WallSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/Buildings/BP_OrionStructureWall.BP_OrionStructureWall_C")),
		EOrionStructure::Wall
	},
	{
		4, FName(TEXT("DoubleWallDoor")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/DoubleWallSnapshot.DoubleWallSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/Buildings/BP_OrionStructureDoubleWall.BP_OrionStructureDoubleWall_C")),
		EOrionStructure::DoubleWall
	},
	{
		5, FName(TEXT("BasicRoof")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/BasicRoofSnapShot.BasicRoofSnapShot")),
		FString(TEXT("/Game/_Orion/Blueprints/Buildings/BP_OrionStructureBasicRoof.BP_OrionStructureBasicRoof_C")),
		EOrionStructure::BasicRoof
	},
	{
		6, FName(TEXT("OrionActorOre")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/OrionActorOreSnapshot.OrionActorOreSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/BP_OrionActorOre.BP_OrionActorOre_C")),
		EOrionStructure::None
	},
	{
		7, FName(TEXT("OrionActorProduction")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/OrionActorProductionSnapshot.OrionActorProductionSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/BP_OrionActorProduction.BP_OrionActorProduction_C")),
		EOrionStructure::None
	},
	{
		8, FName(TEXT("OrionActorStorage")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/OrionActorStorageSnapshot.OrionActorStorageSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/BP_OrionActorStorage.BP_OrionActorStorage_C")),
		EOrionStructure::None
	},
};

const TMap<int32, FOrionDataBuilding> AOrionHUD::BuildingMenuOptionsMap = []()
{
	TMap<int32, FOrionDataBuilding> Map;
	for (const FOrionDataBuilding& Data : BuildingMenuOptions)
	{
		Map.Add(Data.BuildingId, Data);
	}
	return Map;
}();


void AOrionHUD::BeginPlay()
{
	Super::BeginPlay();

	if (WB_DeveloperUIBase)
	{
		UOrionUserWidgetUIBase* DeveloperUIBase = CreateWidget<UOrionUserWidgetUIBase>(GetWorld(), WB_DeveloperUIBase);

		DeveloperUIBase->OnViewLevelUp.BindLambda([this]()
		{
			UE_LOG(LogTemp, Log, TEXT("View Level Up"));
			OnViewLevelUp.ExecuteIfBound();
		});

		DeveloperUIBase->OnViewLevelDown.BindLambda([this]()
		{
			UE_LOG(LogTemp, Log, TEXT("View Level Down"));
			OnViewLevelDown.ExecuteIfBound();
		});

		DeveloperUIBase->OnSaveGame.BindLambda([this]()
		{
			UE_LOG(LogTemp, Log, TEXT("Save Game"));
			/*GetWorld()->GetGameInstance<UOrionGameInstance>()->SaveGameWithDialog();*/
			GetWorld()->GetGameInstance<UOrionGameInstance>()->SaveGame("Developer");
		});

		DeveloperUIBase->OnLoadGame.BindLambda([this]()
		{
			UE_LOG(LogTemp, Log, TEXT("Load Game"));
			/*GetWorld()->GetGameInstance<UOrionGameInstance>()->LoadGameWithDialog();*/
			GetWorld()->GetGameInstance<UOrionGameInstance>()->LoadGame("Developer");
		});

		DeveloperUIBase->AddToViewport();
	}

	InitCharaInfoPanel();
	HideCharaInfoPanel();

	AOrionPlayerController* OrionPlayerController = Cast<AOrionPlayerController>(GetOwningPlayerController());
	if (OrionPlayerController)
	{
		OrionPlayerController->OnToggleBuildingMode.BindLambda([this](bool bIsBuildingMode)
		{
			if (bIsBuildingMode)
			{
				ShowBuildingMenu();
			}
			else
			{
				HideBuildingMenu();
			}
		});
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get OrionPlayerController!"));
	}
}

void AOrionHUD::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime); // ①

	// —— State switching —— //
	// Regardless of whether InfoChara is empty, first check for changes in bShowCharaInfoPanel
	if (bShowCharaInfoPanel != PreviousbShowCharaInfoPanel ||
		(InfoChara && InfoChara->GetName() != PreviousInfoChara))
	{
		if (bShowCharaInfoPanel && InfoChara)
		{
			UE_LOG(LogTemp, Log, TEXT("Show Chara Info Panel"));
			if (CharaInfoPanel)
			{
				CharaInfoPanel->InitCharaInfoPanelParams(InfoChara);
			}
			ShowCharaInfoPanel();
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Hide Chara Info Panel"));
			HideCharaInfoPanel();
		}

		PreviousbShowCharaInfoPanel = bShowCharaInfoPanel;
		PreviousInfoChara = InfoChara ? InfoChara->GetName() : TEXT("");
	}

	// —— Per-frame refresh —— //
	if (CharaInfoPanel && bShowCharaInfoPanel && InfoChara)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Update Chara Info Panel"));
		CharaInfoPanel->UpdateCharaInfo(InfoChara);
	}
}

void AOrionHUD::ShowPlayerOperationMenu(float MouseX, float MouseY, const FHitResult& HitResult,
                                        const TArray<FString>& ArrOptionNames)
{
	if (ArrOperationAvailable.Num() > 0)
	{
		ArrOperationAvailable.Empty();
	}

	PlayerOperationSpawnReference = FHitResult();

	for (auto& each : ArrOptionNames)
	{
		FString OptionName = each;
		ArrOperationAvailable.Add(FName(*OptionName));
	}

	if (WB_PlayerOperationMenu)
	{
		UUserWidget* PlayerOperationMenu = CreateWidget<UUserWidget>(GetWorld(), WB_PlayerOperationMenu);
		if (PlayerOperationMenu)
		{
			PlayerOperationSpawnReference = HitResult;

			PlayerOperationMenu->AddToViewport();
			PlayerOperationMenu->SetPositionInViewport(FVector2D(MouseX, MouseY));
		}
	}
}

void AOrionHUD::DrawHUD()
{
	Super::DrawHUD();

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !Canvas)
	{
		return;
	}

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
		bool bProjected = UGameplayStatics::ProjectWorldToScreen(
			PC,
			// Can lift it up a bit to display text above the head
			OrionChara->GetActorLocation() + FVector(0.f, 0.f, 100.f),
			ScreenPos
		);

		// 3) Draw text on Canvas
		if (bProjected)
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
				//FStringView(*CombinedText), // <- Pass FString instead of FStringView
				CombinedText,
				ScreenPos.X,
				ScreenPos.Y,
				1.5f,
				1.5f,
				RenderInfo
			);

			// If you need to change to another color, call SetDrawColor(...) again and then draw
		}
	}

	// ==========================================================

	// 如果没有 PlayerOwner 或无法获得鼠标位置，就不绘制
	if (!PlayerOwner)
	{
		return;
	}

	if (!bShowActorInfo)
	{
		return;
	}

	float MouseX = 0.f, MouseY = 0.f;
	bool bGotMouse = PlayerOwner->GetMousePosition(MouseX, MouseY);

	if (!bGotMouse)
	{
		return;
	}

	float XOffset = 15.f;
	float YOffsetBetweenLines = 20.f;

	UFont* RenderFont = GEngine->GetMediumFont();

	float CurrentY = MouseY;
	for (const FString& Line : InfoLines)
	{
		Canvas->SetDrawColor(FColor::Red);

		// Canvas->DrawText 原型（UE5.5）：
		// float DrawText(const UFont* Font, const FString& Text, float X, float Y, float ScaleX=1.f, float ScaleY=1.f, const FFontRenderInfo& RenderInfo=FFontRenderInfo());
		FFontRenderInfo RenderInfo;
		RenderInfo.bEnableShadow = true;

		// 绘制
		Canvas->DrawText(
			RenderFont,
			//FStringView(*Line), // <- 这里直接传 FString
			Line,
			MouseX + XOffset,
			CurrentY,
			1.5f,
			1.5f,
			RenderInfo
		);

		CurrentY += YOffsetBetweenLines;
	}
}

void AOrionHUD::ShowInfoAtMouse(const TArray<FString>& InLines)
{
	InfoLines = InLines;
}

void AOrionHUD::InitCharaInfoPanel()
{
	if (WB_CharaInfoPanel)
	{
		CharaInfoPanel = CreateWidget<UOrionUserWidgetCharaInfo>(GetWorld(), WB_CharaInfoPanel);
		if (CharaInfoPanel)
		{
			CharaInfoPanel->AddToViewport();
		}
	}
}

void AOrionHUD::HideCharaInfoPanel()
{
	if (CharaInfoPanel)
	{
		CharaInfoPanel->SetVisibility(ESlateVisibility::Collapsed);
		// 或者彻底：CharaPanel->RemoveFromParent();
	}
}

void AOrionHUD::ShowCharaInfoPanel()
{
	if (CharaInfoPanel)
	{
		CharaInfoPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void AOrionHUD::ShowBuildingMenu()
{
	UE_LOG(LogTemp, Log, TEXT("Show Building Menu"));

	if (!WB_BuildingMenu)
	{
		return;
	}

	BuildingMenu = CreateWidget<UOrionUserWidgetBuildingMenu>(
		GetWorld(), WB_BuildingMenu);

	if (BuildingMenu)
	{
		BuildingMenu->InitBuildingMenu(BuildingMenuOptions);

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
	UE_LOG(LogTemp, Log, TEXT("Hide Building Menu"));

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
