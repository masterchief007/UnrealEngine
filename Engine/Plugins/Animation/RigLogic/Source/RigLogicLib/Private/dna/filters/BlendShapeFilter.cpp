// Copyright Epic Games, Inc. All Rights Reserved.

#include "dna/filters/BlendShapeFilter.h"

#include "dna/DNA.h"
#include "dna/TypeDefs.h"
#include "dna/filters/Remap.h"
#include "dna/utils/Extd.h"

namespace dna {

BlendShapeFilter::BlendShapeFilter(MemoryResource* memRes_) :
    memRes{memRes_},
    passingIndices{memRes},
    remappedIndices{memRes} {
}

void BlendShapeFilter::apply(RawDefinition& dest) {
    // Collect all distinct element position indices that are referenced by the present LODs
    dest.lodBlendShapeMapping.mergeIndicesInto(passingIndices);
    // Fill the structure that maps indices prior to deletion to indices after deletion
    remap(static_cast<std::uint16_t>(dest.blendShapeChannelNames.size()), passingIndices, remappedIndices);
    // Fix indices so they match the same elements as earlier (but their
    // actual position changed with the deletion of the unneeded entries)
    dest.lodBlendShapeMapping.mapIndices([this](std::uint16_t value) {
            return remappedIndices.at(value);
        });
    // Delete elements that are not referenced by the new subset of LODs
    extd::filter(dest.blendShapeChannelNames, extd::byPosition(passingIndices));
    // Delete entries from other mappings that reference any of the deleted elements
    auto ignoredByLODConstraint = [this](std::uint16_t  /*unused*/, std::uint16_t blendShapeIndex) {
            return !extd::contains(passingIndices, blendShapeIndex);
        };
    dest.meshBlendShapeChannelMapping.removeIf(ignoredByLODConstraint);
    dest.meshBlendShapeChannelMapping.updateTo(remappedIndices);
}

bool BlendShapeFilter::passes(std::uint16_t index) const {
    return extd::contains(passingIndices, index);
}

}  // namespace dna
