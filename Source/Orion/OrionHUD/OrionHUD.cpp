#include "OrionHUD.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"
#include "Orion/OrionChara/OrionChara.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include <vector>

#include "Orion/OrionPlayerController/OrionPlayerController.h"

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

	// —— 状态切换 —— //
	// 无论 InfoChara 是否为空，都先检查 bShowCharaInfoPanel 的变化
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

	// —— 每帧刷新 —— //
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

		// 2) 将OrionChara的世界坐标 转成 屏幕坐标
		FVector2D ScreenPos;
		bool bProjected = UGameplayStatics::ProjectWorldToScreen(
			PC,
			// 可稍微抬高一点，让文字显示在头顶
			OrionChara->GetActorLocation() + FVector(0.f, 0.f, 100.f),
			ScreenPos
		);

		// 3) 在Canvas上绘制文字
		if (bProjected)
		{
			// 选一个字体，UE 默认有 GetLargeFont() / GetMediumFont() 等等
			UFont* RenderFont = GEngine->GetMediumFont();

			// 先设置文本颜色
			Canvas->SetDrawColor(FColor::Yellow);

			// 如果需要阴影，可设置 FFontRenderInfo
			FFontRenderInfo RenderInfo;
			RenderInfo.bEnableShadow = true;

			// Canvas->DrawText(...) 在 UE5.5 中的原型：
			// float DrawText(const UFont*, const FString&, float X, float Y, float XScale=..., float YScale=..., const FFontRenderInfo& RenderInfo=...);

			Canvas->DrawText(
				RenderFont,
				//FStringView(*CombinedText), // <- 传 FString 而不是 FStringView
				CombinedText,
				ScreenPos.X,
				ScreenPos.Y,
				1.5f,
				1.5f,
				RenderInfo
			);

			// 如果需要再次换别的颜色，就再次 SetDrawColor(...) 后再绘制
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
}

void AOrionHUD::HideBuildingMenu()
{
	UE_LOG(LogTemp, Log, TEXT("Hide Building Menu"));
}
