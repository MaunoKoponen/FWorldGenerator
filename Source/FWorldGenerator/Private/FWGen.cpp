// This file is part of the FWorldGenenerator.
// Copyright Aleksandr "Flone" Tretyakov (github.com/Flone-dnb).
// Licensed under the ZLib license.
// Refer to the LICENSE file included.

#include "FWGen.h"

// STL
#include <random>
#include <ctime>

// External
#include "PerlinNoise.hpp"

#if WITH_EDITOR
#include "Components/BoxComponent.h"
#endif // WITH_EDITOR

#include "Components/StaticMeshComponent.h"

// --------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------



AFWGen::AFWGen()
{
	PrimaryActorTick.bCanEverTick = false;
	bWorldCreated                 = false;


	iGeneratedSeed                = 0;


	pChunkMap                     = nullptr;




	// Mesh structure

	pRootNode = CreateDefaultSubobject<USceneComponent>("Root");
	RootComponent = pRootNode;

	pProcMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>("ProcMeshComp");
	pProcMeshComponent->bUseAsyncCooking = true;
	pProcMeshComponent->RegisterComponent();
	pProcMeshComponent->SetupAttachment(RootComponent);

	// Enable collision
	pProcMeshComponent->ContainsPhysicsTriMeshData(true);



	pChunkMap = new FWGenChunkMap();




	// Water Plane

	WaterPlane = CreateDefaultSubobject<UStaticMeshComponent>("WaterPlane");
	WaterPlane->SetupAttachment(RootComponent);
	WaterPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("StaticMesh'/Engine/BasicShapes/Plane.Plane'"));
	WaterPlane->SetStaticMesh(PlaneMeshAsset.Object);

	if (CreateWater == false) WaterPlane->SetVisibility(false);




	// Materials

	WaterMaterial  = nullptr;
	GroundMaterial = nullptr;



	// Preview Plane

#if WITH_EDITOR
	PreviewPlane = CreateDefaultSubobject<UBoxComponent>("PreviewPlane");
	PreviewPlane->SetupAttachment(RootComponent);
	PreviewPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	refreshPreview();

	if (ComplexPreview) GenerateWorld();
#endif // WITH_EDITOR
}

AFWGen::~AFWGen()
{
	if (pChunkMap)
	{
		delete pChunkMap;
	}

	if (!pProcMeshComponent->IsValidLowLevel())
	{
		return;
	}

	if (pProcMeshComponent->IsPendingKill())
	{
		return;
	}

	pProcMeshComponent->DestroyComponent();
}

void AFWGen::GenerateWorld()
{
	if (pChunkMap)
	{
		pChunkMap->clearWorld(pProcMeshComponent);
	}



	generateSeed();

	if (WorldSize != -1)
	{
		// Generate the chunks.

		int32 iSectionIndex = 0;

		for (long long x = -ViewDistance; x < ViewDistance + 1; x++)
		{
			for (long long y = -ViewDistance; y < ViewDistance + 1; y++)
			{
				pChunkMap->addChunk(generateChunk(x, y, iSectionIndex));

				iSectionIndex++;
			}
		}
	}
	else
	{
		pChunkMap->addChunk(generateChunk(0, 0, 0));
	}




	// Water Plane

	WaterPlane->SetWorldLocation(FVector(
		GetActorLocation().X,
		GetActorLocation().Y,
		GetActorLocation().Z + ((GenerationMaxZFromActorZ - GetActorLocation().Z) * ZWaterLevelInWorld)
	));

	WaterPlane->SetWorldScale3D(FVector(
		((ChunkPieceColumnCount) * ChunkPieceSizeX) * (WaterSize / 100.0f),
		((ChunkPieceRowCount) * ChunkPieceSizeY) * (WaterSize / 100.0f),
		0.1f
	));

	if (WaterMaterial)
	{
		WaterPlane->SetMaterial(0, WaterMaterial);
	}
}

#if WITH_EDITOR
void AFWGen::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName MemberPropertyChanged = (PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None);

	if (
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ChunkPieceRowCount)
		|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ChunkPieceColumnCount)
		|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ChunkPieceSizeX)
		|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ChunkPieceSizeY)
		|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, GenerationFrequency)
		|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, GenerationOctaves)
		|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, GenerationSeed)
		|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, GenerationMaxZFromActorZ)
		|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ComplexPreview)
		|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, InvertWorld)
		)
	{
		if (ChunkPieceRowCount < 1)
		{
			ChunkPieceRowCount = 1;
		}

		if (ChunkPieceColumnCount < 1)
		{
			ChunkPieceColumnCount = 1;
		}

		if (GenerationMaxZFromActorZ < 0.0f)
		{
			GenerationMaxZFromActorZ = 0.0f;
		}

		if (GenerationSeed < 0)
		{
			GenerationSeed = 0;
		}

		if (GenerationFrequency > 64.0f)
		{
			GenerationFrequency = 64.0f;
		}
		else if (GenerationFrequency < 0.1f)
		{
			GenerationFrequency = 0.1f;
		}

		if (GenerationOctaves > 16)
		{
			GenerationOctaves = 16;
		}
		else if (GenerationOctaves < 1)
		{
			GenerationOctaves = 1;
		}

		refreshPreview();

		if (ComplexPreview) GenerateWorld();
	}
	else if ( MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, CreateWater) 
			|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ZWaterLevelInWorld)
			|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, WaterSize)
			|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, WaterMaterial)
			|| MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, GroundMaterial) )
	{
		if (ZWaterLevelInWorld < 0.0f)
		{
			ZWaterLevelInWorld = 0.0f;
		}
		else if (ZWaterLevelInWorld > 1.0f)
		{
			ZWaterLevelInWorld = 1.0f;
		}

		if (GroundMaterial)
		{
			pProcMeshComponent->SetMaterial(0, GroundMaterial);
		}

		if (CreateWater)
		{
			if (WaterMaterial)
			{
				WaterPlane->SetMaterial(0, WaterMaterial);
			}

			WaterPlane->SetVisibility(true);

			WaterPlane->SetWorldLocation(FVector(
				GetActorLocation().X,
				GetActorLocation().Y,
				GetActorLocation().Z + ((GenerationMaxZFromActorZ - GetActorLocation().Z) * ZWaterLevelInWorld)
			));
			WaterPlane->SetWorldScale3D(FVector(
				((ChunkPieceColumnCount) * ChunkPieceSizeX) * (WaterSize / 100.0f),
				((ChunkPieceRowCount) * ChunkPieceSizeY)    * (WaterSize / 100.0f),
				0.1f
			));
		}
		else
		{
			WaterPlane->SetVisibility(false);
		}
	}
	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ViewDistance))
			||
			(MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, WorldSize)))
	{
		if (WorldSize < -1)
		{
			WorldSize = -1;
		}

		if (ViewDistance < 1)
		{
			ViewDistance = 1;
		}

		refreshPreview();

		if (ComplexPreview) GenerateWorld();
	}
	else
	{
		refreshPreview();

		if (ComplexPreview) GenerateWorld();
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void AFWGen::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	refreshPreview();

	if (ComplexPreview) GenerateWorld();
}
#endif // WITH_EDITOR

void AFWGen::BeginPlay()
{
	Super::BeginPlay();
}

void AFWGen::generateSeed()
{
	uint32_t seed = 0;

	if (GenerationSeed == 0)
	{
		// Generate seed
		std::mt19937_64 gen(std::random_device{}());
		std::uniform_int_distribution<unsigned int> uid(0, UINT_MAX);

		seed = uid(gen);
	}
	else
	{
		seed = GenerationSeed;
	}

	iGeneratedSeed = seed;
}

FWGenChunk* AFWGen::generateChunk(long long iX, long long iY, int32 iSectionIndex)
{
	// Generation setup

	uint32_t seed = iGeneratedSeed;



	// We ++ here because we start to make polygons from 2nd row
	int32 iCorrectedRowCount = ChunkPieceRowCount + 1;
	int32 iCorrectedColumnCount = ChunkPieceColumnCount + 1;




	// Perlin Noise setup

	const siv::PerlinNoise perlinNoise(seed);
	const double fx = ((iCorrectedColumnCount - 1) * ChunkPieceSizeX) / GenerationFrequency;
	const double fy = ((iCorrectedRowCount - 1) * ChunkPieceSizeY) / GenerationFrequency;




	// Prepare chunk coordinates

	float fChunkX = GetActorLocation().X;
	float fChunkY = GetActorLocation().Y;

	if (iX != 0)
	{
		// Left or right chunk

		fChunkX += (iX * ChunkPieceColumnCount * ChunkPieceSizeX);
	}
	
	if (iY != 0)
	{
		// Top or bottom chunk

		fChunkY += (iY * ChunkPieceRowCount * ChunkPieceSizeY);
	}




	// Generation params

	float fStartX = fChunkX - ((iCorrectedColumnCount - 1) * ChunkPieceSizeX) / 2;
	float fStartY = fChunkY - ((iCorrectedRowCount    - 1) * ChunkPieceSizeY) / 2;

	FVector vPrevLocation(fStartX, fStartY, GetActorLocation().Z);

	float fInterval = GenerationMaxZFromActorZ - GetActorLocation().Z;




	// Create chunk

	FWGenChunk* pNewChunk = new FWGenChunk(iX, iY, iSectionIndex);




	// Generation

	for (int32 i = 0; i < iCorrectedRowCount; i++)
	{
		for (int32 j = 0; j < iCorrectedColumnCount; j++)
		{
			// Generate vertex
			double generatedValue = perlinNoise.octaveNoise0_1(vPrevLocation.X / fx, vPrevLocation.Y / fy, GenerationOctaves);

			if (InvertWorld)
			{
				// Here return value from perlinNoise.octaveNoise0_1 can be
				// 0    , but should be   1
				// 1    , but should be   0

				generatedValue = 1.0 - generatedValue;
			}

			// Here return value from perlinNoise.octaveNoise0_1 can be
			// 0    , but should be   GetActorLocation().Z
			// ...  , but should be   value from interval [GetActorLocation().Z; GenerationMaxZFromActorZ]
			// 1    , but should be   GenerationMaxZFromActorZ

			// So we do this:
			vPrevLocation.Z = GetActorLocation().Z + (fInterval * generatedValue);


			pNewChunk->vVertices .Add (vPrevLocation);

			pNewChunk->vNormals      .Add(FVector(0, 0, 1.0f));
			pNewChunk->vUV0          .Add(FVector2D(i, j));
			pNewChunk->vTangents     .Add(FProcMeshTangent(0.0f, 1.0f, 0.0f));

			// Set alpha color

			float fAlphaColor = 0.0f;

			if ((generatedValue >= FirstMaterialMaxRelativeHeight) && (generatedValue <= SecondMaterialMaxRelativeHeight))
			{
				fAlphaColor = 0.5f;
			}
			else if (generatedValue >= SecondMaterialMaxRelativeHeight)
			{
				fAlphaColor = 1.0f;
			}

			pNewChunk->vVertexColors .Add(FLinearColor(0.0f, 0.75, 0.0f, fAlphaColor));


			vPrevLocation.X += ChunkPieceSizeX;

			if (i != 0)
			{
				//     j = 0,   1,   2,   3  ...
				// i = 0:  +----+----+----+- ...
				//         |   /|   /|   /|
				//         |  / |  / |  / |
				//         | /  | /  | /  |
				// i = 1:  +----+----+----+- ...

				int32 iFirstIndexInRow = (i - 1) * iCorrectedColumnCount;

				if (j == 0)
				{
					// Add triangle #1
					pNewChunk->vTriangles.Add(iFirstIndexInRow + j);
					pNewChunk->vTriangles.Add(i * iCorrectedColumnCount + j);
					pNewChunk->vTriangles.Add(iFirstIndexInRow + j + 1);
				}
				else
				{
					// Add triangle #2
					pNewChunk->vTriangles.Add(iFirstIndexInRow + j);
					pNewChunk->vTriangles.Add(i * iCorrectedColumnCount + j - 1);
					pNewChunk->vTriangles.Add(i * iCorrectedColumnCount + j);

					if (j < (iCorrectedColumnCount - 1))
					{
						// Add triangle #1
						pNewChunk->vTriangles.Add(iFirstIndexInRow + j);
						pNewChunk->vTriangles.Add(i * iCorrectedColumnCount + j);
						pNewChunk->vTriangles.Add(iFirstIndexInRow + j + 1);
					}
				}
			}
		}

		vPrevLocation.Set(fStartX, vPrevLocation.Y + ChunkPieceSizeY, GetActorLocation().Z);
	}

	pProcMeshComponent->CreateMeshSection_LinearColor(iSectionIndex, pNewChunk->vVertices, pNewChunk->vTriangles, pNewChunk->vNormals,
		pNewChunk->vUV0, pNewChunk->vVertexColors, pNewChunk->vTangents, true);

	// Set material
	if (GroundMaterial)
	{
		pProcMeshComponent->SetMaterial(iSectionIndex, GroundMaterial);
	}

	pNewChunk->setMeshSection(pProcMeshComponent->GetProcMeshSection(iSectionIndex));

	return pNewChunk;
}

#if WITH_EDITOR
void AFWGen::refreshPreview()
{
	if (WorldSize == -1)
	{
		PreviewPlane ->SetBoxExtent ( FVector (
			(ChunkPieceColumnCount * ChunkPieceSizeX / 2),
			(ChunkPieceRowCount * ChunkPieceSizeY / 2),
			GenerationMaxZFromActorZ / 2
		)
		);
	}
	else if (WorldSize == 0)
	{
		PreviewPlane ->SetBoxExtent ( FVector (
			(ViewDistance * 2 + 1) * (ChunkPieceColumnCount * ChunkPieceSizeX / 2),
			(ViewDistance * 2 + 1) * (ChunkPieceRowCount * ChunkPieceSizeY / 2),
			GenerationMaxZFromActorZ / 2
		)
		);
	}
	else
	{
		PreviewPlane ->SetBoxExtent ( FVector (
			((WorldSize * ViewDistance) * 2 + 1) * (ChunkPieceColumnCount * ChunkPieceSizeX / 2),
			((WorldSize * ViewDistance) * 2 + 1) * (ChunkPieceRowCount * ChunkPieceSizeY / 2),
			GenerationMaxZFromActorZ / 2
		)
		);
	}

	PreviewPlane ->SetWorldLocation ( FVector (
		GetActorLocation () .X,
		GetActorLocation () .Y,
		GetActorLocation () .Z + (GenerationMaxZFromActorZ) / 2
	)   
	);
}
#endif // WITH_EDITOR

