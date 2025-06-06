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
	/** 在蓝图中绑定动画 */
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* FloatUp;

	/** 在蓝图中绑定控件 */
	UPROPERTY(meta = (BindWidget))
	UImage* Icon;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* DeltaText;

	/** C++ 调用：设置图标 */
	UFUNCTION(BlueprintCallable)
	void SetIcon(UTexture2D* InTex) { Icon->SetBrushFromTexture(InTex); }


	virtual void NativeConstruct() override
	{
		Super::NativeConstruct();
		// 不再使用 BindToAnimationFinished，改为定时销毁
	}

	UFUNCTION(BlueprintCallable)
	void PlayFloat()
	{
		PlayAnimation(FloatUp);
		// 动画时长固定 0.8s，0.8s 后自动从父级移除
		if (UWorld* World = GetWorld())
		{
			FTimerHandle TimerHandle;
			FTimerDelegate TimerDel;
			// 直接调用 RemoveFromParent()
			TimerDel.BindUFunction(this, FName("RemoveFromParent"));
			World->GetTimerManager().SetTimer(
				TimerHandle,
				TimerDel,
				0.8f, // 延迟 0.8 秒
				false // 不循环
			);
		}
	}
};
