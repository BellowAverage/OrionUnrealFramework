// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionGameInstance/OrionCharaManager.h"
#include "Misc/Guid.h"

TArray<FOrionCharaSerializable> UOrionCharaManager::TestCharactersSet = {
	{
		FGuid::NewGuid(),
		FVector(0.0f, 0.0f, 150.f),
		FRotator(0.0f, 0.0f, 0.0f),
		{}
	},
	{
		FGuid::NewGuid(),
		FVector(200.f, 0.0f, 150.f),
		FRotator(0.0f, 0.0f, 0.0f),
		{}
	},
	{
		FGuid::NewGuid(),
		FVector(400.f, 0.0f, 150.f),
		FRotator(0.0f, 0.0f, 0.0f),
		{}
	},
	{
		FGuid::NewGuid(),
		FVector(600.f, 0.0f, 150.f),
		FRotator(0.0f, 0.0f, 0.0f),
		{}
	},
};
