// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3sharp MeshConnectedComponents

#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh3.h"

/**
 * FMeshConnectedComponents calculates Connected Components of a Mesh, or sub-regions of a Mesh.
 * By default the actual mesh connectivity is used, but an optional connectivity predicate
 * can be provided to specify when two elements should be considered connected.
 */
class DYNAMICMESH_API FMeshConnectedComponents
{
public:
	const FDynamicMesh3* Mesh;

	/**
	 * Connected Component found by one of the calculation functions
	 */
	struct FComponent
	{
		/** List of indices contained in component */
		TArray<int> Indices;
	};

	/**
	 * List of Connected Components that have been found by one of the calculation functions
	 */
	TIndirectArray<FComponent> Components;

public:

	FMeshConnectedComponents(const FDynamicMesh3* MeshIn)
		: Mesh(MeshIn)
	{
	}

	//
	// Calculation functions. Call these to calculate different types of Components.
	//

	/**
	 * Find all connected triangle components of the Mesh and store in Components array.
	 * Triangle connectivity is based on edge connectivity, ie bowtie-vertices are not connections between triangles.
	 * @param TrisConnectedPredicate optional function that specifies whether two edge-connected triangles should be considered connected by the search
	 */
	void FindConnectedTriangles(TFunction<bool(int32, int32)> TrisConnectedPredicate = nullptr);

	/**
	 * Find all connected triangle components of a subset of triangles of the Mesh and store in Components array.
	 * Triangle connectivity is based on edge connectivity, ie bowtie-vertices are not connections between triangles.
	 * @param TriangleROI list of triangles to search across
	 * @param TrisConnectedPredicate optional function that specifies whether two edge-connected triangles should be considered connected by the search
	 */
	void FindConnectedTriangles(const TArray<int>& TriangleROI, TFunction<bool(int32, int32)> TrisConnectedPredicate = nullptr);

	/**
	 * Find all connected triangle components of a subset of triangles of the Mesh and store in Components array.
	 * Triangle connectivity is based on edge connectivity, ie bowtie-vertices are not connections between triangles.
	 * @param IndexFilterFunc defines set of triangles to search across, return true for triangle IDs that are to be considered
	 * @param TrisConnectedPredicate optional function that specifies whether two edge-connected triangles should be considered connected by the search
	 */
	void FindConnectedTriangles(TFunctionRef<bool(int)> IndexFilterFunc, TFunction<bool(int32, int32)> TrisConnectedPredicate = nullptr);

	/**
	 * Find all connected triangle components that contain one or more Seed Triangles and store in Components array.
	 * Search only starts from Seed Triangles.
	 * Triangle connectivity is based on edge connectivity, ie bowtie-vertices are not connections between triangles.
	 * @param SeedTriangles list of start triangles, each component contains at least one of these triangles
	 * @param TrisConnectedPredicate optional function that specifies whether two edge-connected triangles should be considered connected by the search
	 */
	void FindTrianglesConnectedToSeeds(const TArray<int>& SeedTriangles, TFunction<bool(int32, int32)> TrisConnectedPredicate = nullptr);


	//
	// Query functions. Only valid to call after a Calculation function has been called.
	//

	/** @return Number of Components that were found */
	int32 Num() const
	{
		return Components.Num();
	}

	/** @return element of Components array at given Index */
	const FComponent& GetComponent(int32 Index) const { return Components[Index]; }

	/** @return element of Components array at given Index */
	FComponent& GetComponent(int32 Index) { return Components[Index]; }


	/** 
	 * @return index of largest component by element count 
	 */
	int32 GetLargestIndexByCount() const;

	/**
	 * Sort the Components array by component element count
	 * @param bLargestFirst if true, sort by decreasing count, otherwise by increasing count
	 */
	void SortByCount(bool bLargestFirst = true);



public:
	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable ranged-based for loop support (forwarding TIndirectArray declarations)
	 */
	auto begin() { return Components.begin(); }
	auto begin() const { return Components.begin(); }
	auto end() { return Components.end();  }
	auto end() const { return Components.end(); }



protected:
	//
	// Internal functions to calculate ROI
	//
	void FindTriComponents(FInterval1i ActiveRange, TArray<uint8>& ActiveSet, TFunction<bool(int32, int32)> TriConnectedPredicate);
	void FindTriComponents(const TArray<int32>& SeedList, TArray<uint8>& ActiveSet, TFunction<bool(int32, int32)> TriConnectedPredicate);
	void FindTriComponent(FComponent* Component, TArray<int32>& ComponentQueue, TArray<uint8>& ActiveSet);
	void FindTriComponent(FComponent* Component, TArray<int32>& ComponentQueue, TArray<uint8>& ActiveSet, TFunctionRef<bool(int32, int32)> TriConnectedPredicate);
	void RemoveFromActiveSet(const FComponent* Component, TArray<uint8>& ActiveSet);



public:
	/**
	 * Utility function to expand a triangle selection to all triangles considered "connected".
	 * More efficient than using full FMeshConnectedComponents instance if ROI is small relative to Mesh size (or if temp buffers can be re-used)
	 * @param Mesh Mesh to calculate on
	 * @param InputROI input set of triangles
	 * @param ResultROI output set of triangles connected to InputROI
	 * @param QueueBuffer optional buffer used as internal Queue. If passed as nullptr, a TArray will be locally allocated
	 * @param DoneBuffer optional set used to track which triangles have already been processed. If passed as nullptr, an TSet will be locally allocated
	 * @param CanGrowPredicate determines whether two connected mesh triangles should be considered connected while growing
	 */
	static void GrowToConnectedTriangles(const FDynamicMesh3* Mesh, 
		const TArray<int>& InputROI, 
		TArray<int>& ResultROI,
		TArray<int32>* QueueBuffer = nullptr, 
		TSet<int32>* DoneBuffer = nullptr,
		TFunctionRef<bool(int32, int32)> CanGrowPredicate = [](int32, int32) { return true; }
	);


};
