﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionStructureFoundation.h"
#include "OrionStructureWall.h"

AOrionStructureFoundation::AOrionStructureFoundation()
{
	if (!StructureComponent)
	{
		return;
	}
}


void AOrionStructureFoundation::BeginPlay()
{
	Super::BeginPlay();
}
