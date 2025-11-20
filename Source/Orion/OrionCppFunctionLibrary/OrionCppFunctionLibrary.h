// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ContainersFwd.h"

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
};
