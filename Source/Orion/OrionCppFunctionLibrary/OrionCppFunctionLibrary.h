// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Map.h"

/**
 * 监听元素变动的 TArray 包装类
 * 用法示例：
 *
 *     TObservableArray<int32> Arr;
 *     Arr.OnArrayChanged.AddLambda([](FName Op){
 *         UE_LOG(LogTemp, Log, TEXT("Array changed by %s"), *Op.ToString());
 *     });
 *     Arr.Add(123);          // 触发广播
 *     Arr.RemoveAt(0);       // 同上
 */
template <typename InElementType, typename Allocator = FDefaultAllocator>
class TObservableArray
{
public:
	/** 数组内容发生变动时触发 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnArrayChanged, FName /*OperationName*/);
	FOnArrayChanged OnArrayChanged;

	using ElementType = InElementType;
	using ArrayType = TArray<ElementType, Allocator>;

	/** 构造／初始化 */
	TObservableArray() = default;

	explicit TObservableArray(std::initializer_list<ElementType> Init)
		: Data(Init)
	{
	}

	/* ---------- 与 TArray 同名的常用接口 ---------- */

	FORCEINLINE int32 Num() const { return Data.Num(); }
	FORCEINLINE bool IsEmpty() const { return Data.IsEmpty(); }

	/* ---------- 只读查询 ---------- */
	FORCEINLINE bool Contains(const ElementType& Item) const { return Data.Contains(Item); }

	/* ---------- 转换 / 导出 ---------- */
	FORCEINLINE TArray<ElementType, Allocator> ToArray() const { return Data; } // 拷贝为普通 TArray
	FORCEINLINE operator ArrayType&() { return Data; } // 引用式隐式转换
	FORCEINLINE operator const ArrayType&() const { return Data; }

	/** 读取元素 */
	FORCEINLINE ElementType& operator[](int32 Index) { return Data[Index]; }
	FORCEINLINE const ElementType& operator[](int32 Index) const { return Data[Index]; }

	/** Add / Emplace */
	FORCEINLINE int32 Add(const ElementType& Item)
	{
		const int32 Idx = Data.Add(Item);
		Broadcast(TEXT("Add"));
		return Idx;
	}

	FORCEINLINE int32 Add(ElementType&& Item)
	{
		const int32 Idx = Data.Add(MoveTemp(Item));
		Broadcast(TEXT("Add"));
		return Idx;
	}

	template <typename... Args>
	FORCEINLINE int32 Emplace(Args&&... ArgsIn)
	{
		const int32 Idx = Data.Emplace(Forward<Args>(ArgsIn)...);
		Broadcast(TEXT("Emplace"));
		return Idx;
	}

	/** Insert */
	FORCEINLINE void Insert(const ElementType& Item, int32 Index)
	{
		Data.Insert(Item, Index);
		Broadcast(TEXT("Insert"));
	}

	/** Remove 系列 */
	FORCEINLINE void RemoveAt(int32 Index, int32 Count = 1, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		Data.RemoveAt(Index, Count, AllowShrinking);
		Broadcast(TEXT("RemoveAt"));
	}

	FORCEINLINE int32 Remove(const ElementType& Item)
	{
		const int32 R = Data.Remove(Item);
		if (R)
		{
			Broadcast(TEXT("Remove"));
		}
		return R;
	}

	FORCEINLINE bool RemoveSingle(const ElementType& Item)
	{
		const bool R = Data.RemoveSingle(Item);
		if (R)
		{
			Broadcast(TEXT("RemoveSingle"));
		}
		return R;
	}

	/** 其它常见操作 */
	FORCEINLINE void Empty(int32 Slack = 0)
	{
		Data.Empty(Slack);
		Broadcast(TEXT("Empty"));
	}

	FORCEINLINE void Reset(int32 NewSize = 0)
	{
		Data.Reset(NewSize);
		Broadcast(TEXT("Reset"));
	}

	FORCEINLINE void Shrink()
	{
		Data.Shrink();
		Broadcast(TEXT("Shrink"));
	}

	/* ---------- 迭代支持 ---------- */
	FORCEINLINE auto begin() { return Data.begin(); }
	FORCEINLINE auto end() { return Data.end(); }
	FORCEINLINE auto begin() const { return Data.begin(); }
	FORCEINLINE auto end() const { return Data.end(); }

	/* ---------- 直接访问内部数组 ---------- */
	FORCEINLINE ArrayType& GetRaw() { return Data; }
	FORCEINLINE const ArrayType& GetRaw() const { return Data; }

private:
	ArrayType Data;

	FORCEINLINE void Broadcast(FName Operation) { OnArrayChanged.Broadcast(Operation); }
};

class ORION_API OrionCppFunctionLibrary
{
public:
	OrionCppFunctionLibrary();
	~OrionCppFunctionLibrary();

	/**
	 * 获取输入Actor的StaticMesh Component的边线并高亮显示 (通过CustomDepth)
	 * @param Actor         目标Actor
	 * @param bEnable       是否启用高亮
	 * @param StencilValue  CustomDepth Stencil Value (默认为252，常用作高亮)
	 */
	static void SetActorHighlight(const class AActor* Actor, bool bEnable, int32 StencilValue = 252);

	/**
	 * 将输入FString打印在输入Actor的位置映射到玩家Canvas上位置
	 * 注意：此函数应在HUD的DrawHUD循环中调用，因为它需要有效的Canvas对象
	 * @param Canvas        HUD Canvas
	 * @param Player        PlayerController (用于坐标投影)
	 * @param Actor         目标Actor
	 * @param Text          要打印的文本
	 * @param TextColor     文本颜色
	 */
	// static void DrawTextOnCanvasAtActorLocation(class UCanvas* Canvas, const class APlayerController* Player, const class AActor* Actor, const FString& Text, FColor TextColor = FColor::White);

	/**
	 * 将 Debug 信息写入到本地文件 Saved/Logs/OrionDebug.log
	 * @param LogContent  要写入的内容
	 */
	static void WriteDebugLog(const FString& LogContent);

	/**
	 * 使用 UE_LOG 按行打印 TMap 的 Key / Value
	 * @param MapName    方便定位输出来源的名字
	 * @param Verbosity  UE_LOG 的日志等级 (Log / Warning / Error 等)
	 */
	template <typename KeyType, typename ValueType>
	static void OrionPrintTMap(const TMap<KeyType, ValueType>& InMap, ELogVerbosity::Type Verbosity = ELogVerbosity::Log, const FString& MapName = TEXT("TMap"))
	{
#define ORION_TMAP_LOG_LINE(VerbosityArg, ...)                     \
		do                                                         \
		{                                                          \
			switch (VerbosityArg)                                  \
			{                                                      \
			case ELogVerbosity::Error:                             \
				UE_LOG(LogTemp, Error, __VA_ARGS__);               \
				break;                                             \
			case ELogVerbosity::Warning:                           \
				UE_LOG(LogTemp, Warning, __VA_ARGS__);             \
				break;                                             \
			default:                                               \
				UE_LOG(LogTemp, Log, __VA_ARGS__);                 \
				break;                                             \
			}                                                      \
		} while (0)

		ORION_TMAP_LOG_LINE(Verbosity, TEXT("==== %s | Count: %d ===="), *MapName, InMap.Num());

		if (InMap.Num() == 0)
		{
			ORION_TMAP_LOG_LINE(Verbosity, TEXT("%s is empty"), *MapName);
			return;
		}

		int32 Index = 0;
		for (const TPair<KeyType, ValueType>& Pair : InMap)
		{
			const FString KeyString = LexToString(Pair.Key);
			const FString ValueString = LexToString(Pair.Value);
			ORION_TMAP_LOG_LINE(Verbosity, TEXT("[%d] Key: %s | Value: %s"), Index++, *KeyString, *ValueString);
		}

		ORION_TMAP_LOG_LINE(Verbosity, TEXT("==== End of %s ===="), *MapName);
#undef ORION_TMAP_LOG_LINE
	}
};
