// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "OrionUserWidgetResourceFloat.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class ORION_API UOrionUserWidgetResourceFloat : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind animation in Blueprint */
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* FloatUp;

	/** Bind widget in Blueprint */
	UPROPERTY(meta = (BindWidget))
	UImage* Icon;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* DeltaText;

	/** C++ call: Set icon */
	UFUNCTION(BlueprintCallable)
	void SetIcon(UTexture2D* InTex) { Icon->SetBrushFromTexture(InTex); }


	virtual void NativeConstruct() override
	{
		Super::NativeConstruct();
		// No longer using BindToAnimationFinished, changed to timed destruction
	}

	UFUNCTION(BlueprintCallable)
	void PlayFloat()
	{
		PlayAnimation(FloatUp);
		// Animation duration is fixed at 0.8s, automatically remove from parent after 0.8s
		if (UWorld* World = GetWorld())
		{
			FTimerHandle TimerHandle;
			FTimerDelegate TimerDel;
			// Directly call RemoveFromParent()
			TimerDel.BindUFunction(this, FName("RemoveFromParent"));
			World->GetTimerManager().SetTimer(
				TimerHandle,
				TimerDel,
				0.8f, // Delay 0.8 seconds
				false // No loop
			);
		}
	}
};
