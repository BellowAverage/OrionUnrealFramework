#pragma once

#include<CoreMinimal.h>
#include<EOrionStructure.generated.h>


UENUM()
enum class EOrionStructure : uint8
{
	None,
	BasicSquareFoundation,
	BasicTriangleFoundation,
	Wall,
	DoubleWall,
};
