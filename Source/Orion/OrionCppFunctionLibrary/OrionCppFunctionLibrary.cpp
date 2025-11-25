// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionCppFunctionLibrary/OrionCppFunctionLibrary.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

OrionCppFunctionLibrary::OrionCppFunctionLibrary()
{
}

OrionCppFunctionLibrary::~OrionCppFunctionLibrary()
{
}

void OrionCppFunctionLibrary::SetActorHighlight(const AActor* Actor, const bool bEnable, const int32 StencilValue)
{
	if (!Actor)
	{
		return;
	}

	TArray<UStaticMeshComponent*> Components;
	Actor->GetComponents<UStaticMeshComponent>(Components);

	for (UStaticMeshComponent* Mesh : Components)
	{
		if (Mesh)
		{
			Mesh->SetRenderCustomDepth(bEnable);
			if (bEnable)
			{
				Mesh->SetCustomDepthStencilValue(StencilValue);
			}
		}
	}
}

/*
void OrionCppFunctionLibrary::DrawTextOnCanvasAtActorLocation(UCanvas* Canvas, const APlayerController* Player, const AActor* Actor, const FString& Text, FColor TextColor)
{
	if (!Canvas || !Player || !Actor || !GEngine)
	{
		return;
	}

	const FVector ActorLocation = Actor->GetActorLocation();

	// 将世界坐标投影到屏幕坐标
	if (FVector2D ScreenPosition; Player->ProjectWorldLocationToScreen(ActorLocation, ScreenPosition))
	{
		if (const UFont* Font = GEngine->GetMediumFont())
		{
			const FColor OldColor = Canvas->DrawColor;
			Canvas->DrawColor = TextColor;
			
			// 支持多行文本绘制
			TArray<FString> Lines;
			Text.ParseIntoArray(Lines, TEXT("\n"), false);

			float CurrentY = ScreenPosition.Y;
			const float LineHeight = Font->GetMaxCharHeight();

			for (const FString& Line : Lines)
			{
				// 处理可能存在的 Windows 换行符 \r
				const FString CleanLine = Line.Replace(TEXT("\r"), TEXT(""));
				Canvas->DrawText(Font, CleanLine, ScreenPosition.X, CurrentY);
				CurrentY += LineHeight;
			}
			
			Canvas->DrawColor = OldColor;
		}
	}
}
*/

void OrionCppFunctionLibrary::WriteDebugLog(const FString& LogContent)
{
	const FString LogFilePath = FPaths::ProjectSavedDir() / TEXT("Logs/OrionDebug.log");
	FFileHelper::SaveStringToFile(LogContent + TEXT("\n"), *LogFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}