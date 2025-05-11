#pragma once


#include "CoreMinimal.h"
#include "OrionItemInfo.generated.h"

USTRUCT(BlueprintType)
struct FOrionItemInfoTemp
{
	GENERATED_BODY()

	/** 唯一 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 ItemId = 0;

	/** 内部名称 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName Name;

	/** 英文显示名 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** 中文显示名 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText ChineseDisplayName;

	/** 标准价格 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float PriceSTD = 0.f;

	/** 标准生产/处理时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float ProductionTimeCostSTD = 0.f;
};

namespace OrionItemInfoTemp
{
	extern const TArray<FOrionItemInfoTemp> GItemInfoTable;
}
