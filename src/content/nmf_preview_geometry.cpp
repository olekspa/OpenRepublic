#include "openrepublic/content/nmf_preview_geometry.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <stdexcept>

namespace openrepublic::content {
namespace {

bool Fail(std::string* error, const char* message) {
    if (error)
        *error = message;
    return false;
}

bool IsFinite(const NmfPosition& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsFinite(const NmfNormal& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

bool BuildNmfPreviewGeometry(const NmfStaticGeometry& geometry,
                             NmfPreviewGeometry& output,
                             std::string* error) {
    if (geometry.objects.empty() || geometry.objects.size() > MaxNmfGeometryObjects)
        return Fail(error, "NMF preview requires a bounded, nonempty static-object list");

    std::size_t vertexCount = 0;
    std::size_t indexCount = 0;
    std::size_t sourceNormalCount = 0;
    double minimumX = std::numeric_limits<double>::infinity();
    double minimumY = std::numeric_limits<double>::infinity();
    double minimumZ = std::numeric_limits<double>::infinity();
    double maximumX = -std::numeric_limits<double>::infinity();
    double maximumY = -std::numeric_limits<double>::infinity();
    double maximumZ = -std::numeric_limits<double>::infinity();
    double minimumNormalLength = std::numeric_limits<double>::infinity();
    double maximumNormalLength = 0.0;
    double totalNormalLength = 0.0;

    for (const auto& object : geometry.objects) {
        const auto& lod = object.lod;
        if (lod.positions.size() > MaxNmfGeometryVertices - vertexCount ||
            lod.indices.size() > MaxNmfGeometryIndices - indexCount)
            return Fail(error, "NMF preview geometry exceeds the bounded vertex or index profile");
        if (!lod.vertexNormals.empty() && lod.vertexNormals.size() != lod.positions.size())
            return Fail(error, "NMF source normal count does not match its vertex positions");

        vertexCount += lod.positions.size();
        indexCount += lod.indices.size();
        for (const auto& position : lod.positions) {
            if (!IsFinite(position))
                return Fail(error, "NMF preview contains a non-finite source position");
            minimumX = std::min(minimumX, static_cast<double>(position.x));
            minimumY = std::min(minimumY, static_cast<double>(position.y));
            minimumZ = std::min(minimumZ, static_cast<double>(position.z));
            maximumX = std::max(maximumX, static_cast<double>(position.x));
            maximumY = std::max(maximumY, static_cast<double>(position.y));
            maximumZ = std::max(maximumZ, static_cast<double>(position.z));
        }
        for (const auto& normal : lod.vertexNormals) {
            if (!IsFinite(normal))
                return Fail(error, "NMF preview contains a non-finite source normal");
            const double length = std::sqrt(static_cast<double>(normal.x) * normal.x +
                                            static_cast<double>(normal.y) * normal.y +
                                            static_cast<double>(normal.z) * normal.z);
            if (!std::isfinite(length))
                return Fail(error, "NMF source normal length is not finite");
            minimumNormalLength = std::min(minimumNormalLength, length);
            maximumNormalLength = std::max(maximumNormalLength, length);
            totalNormalLength += length;
            ++sourceNormalCount;
        }
    }

    const double maximumExtent = std::max({maximumX - minimumX,
                                           maximumY - minimumY,
                                           maximumZ - minimumZ});
    if (vertexCount < 3 || indexCount < 3 || indexCount % 3 != 0 ||
        !std::isfinite(maximumExtent) || maximumExtent <= 0.0)
        return Fail(error, "NMF geometry could not be mapped into a bounded static mesh preview");

    NmfPreviewGeometry candidate;
    candidate.displayScale = NmfPreviewDisplaySpan / maximumExtent;
    const double centerX = (minimumX + maximumX) * 0.5;
    const double centerZ = (minimumZ + maximumZ) * 0.5;
    candidate.sourceMinimum = {static_cast<float>(minimumX), static_cast<float>(minimumY),
                               static_cast<float>(minimumZ)};
    candidate.sourceMaximum = {static_cast<float>(maximumX), static_cast<float>(maximumY),
                               static_cast<float>(maximumZ)};
    candidate.cameraTarget = {
        static_cast<float>(NmfPreviewCenterX),
        static_cast<float>((maximumY - minimumY) * candidate.displayScale * 0.5),
        0.0f};
    candidate.cameraDistance = static_cast<float>(NmfPreviewDisplaySpan *
                                                  NmfPreviewCameraDistanceFactor);
    candidate.sourceNormalCount = sourceNormalCount;
    if (sourceNormalCount != 0) {
        candidate.sourceNormalLengthMinimum = minimumNormalLength;
        candidate.sourceNormalLengthMean = totalNormalLength /
                                           static_cast<double>(sourceNormalCount);
        candidate.sourceNormalLengthMaximum = maximumNormalLength;
    }

    try {
        candidate.positions.reserve(vertexCount);
        candidate.sourceNormals.reserve(vertexCount);
        candidate.sourceNormalMask.reserve(vertexCount);
        candidate.indices.reserve(indexCount);

        for (const auto& object : geometry.objects) {
            const auto& lod = object.lod;
            if (candidate.positions.size() > std::numeric_limits<std::uint32_t>::max() -
                                                  lod.positions.size())
                return Fail(error, "NMF combined vertex index exceeds the renderer's 32-bit range");
            if (candidate.indices.size() > std::numeric_limits<std::uint32_t>::max() -
                                                lod.indices.size())
                return Fail(error, "NMF combined index offset exceeds the renderer's 32-bit range");

            const auto baseVertex = static_cast<std::uint32_t>(candidate.positions.size());
            const auto baseIndex = static_cast<std::uint32_t>(candidate.indices.size());
            for (std::size_t vertexIndex = 0; vertexIndex < lod.positions.size(); ++vertexIndex) {
                const auto& position = lod.positions[vertexIndex];
                const NmfPosition normalized{
                    static_cast<float>(NmfPreviewCenterX +
                                       (static_cast<double>(position.x) - centerX) * candidate.displayScale),
                    static_cast<float>((static_cast<double>(position.y) - minimumY) *
                                       candidate.displayScale),
                    static_cast<float>((static_cast<double>(position.z) - centerZ) *
                                       candidate.displayScale)};
                if (!IsFinite(normalized))
                    return Fail(error, "NMF normalized position exceeds the finite preview range");
                candidate.positions.push_back(normalized);

                if (lod.vertexNormals.empty()) {
                    candidate.sourceNormals.push_back({});
                    candidate.sourceNormalMask.push_back(0);
                } else {
                    const auto& normal = lod.vertexNormals[vertexIndex];
                    candidate.sourceNormals.push_back(normal);
                    candidate.sourceNormalMask.push_back(1);
                }
            }

            // Source triangles are wound opposite to Wicked's counter-clockwise front
            // face (the right-handed cross product of every sampled triangle agrees
            // with its stored vertex normal), so the second and third index of each
            // triangle are exchanged. The parser guarantees a multiple of three.
            for (std::size_t index = 0; index < lod.indices.size(); index += 3) {
                for (const std::size_t offset : {std::size_t{0}, std::size_t{2}, std::size_t{1}}) {
                    const std::uint32_t source = lod.indices[index + offset];
                    if (source >= lod.positions.size() ||
                        source > std::numeric_limits<std::uint32_t>::max() - baseVertex)
                        return Fail(error, "NMF normalized mesh contains an invalid index");
                    candidate.indices.push_back(baseVertex + source);
                }
            }

            if (lod.subsets.empty()) {
                candidate.drawRanges.push_back({baseIndex,
                    static_cast<std::uint32_t>(lod.indices.size()),
                    NoNmfPreviewSubmaterialIndex});
                continue;
            }

            for (const auto& subset : lod.subsets) {
                const std::size_t subsetOffset = subset.indexOffset;
                const std::size_t subsetCount = subset.indexCount;
                if (subsetOffset > lod.indices.size() ||
                    subsetCount > lod.indices.size() - subsetOffset)
                    return Fail(error, "NMF subset draw range exceeds its object's index stream");
                if (subset.submaterialIndex >= geometry.submaterialIdentifiers.size())
                    return Fail(error, "NMF subset references a missing submaterial-table entry");

                candidate.ignoredSubsetIndexCount += subset.indexCount % 3;
                const std::uint32_t renderIndexCount = (subset.indexCount / 3) * 3;
                if (renderIndexCount == 0)
                    continue;
                if (subset.indexOffset > std::numeric_limits<std::uint32_t>::max() - baseIndex)
                    return Fail(error, "NMF subset render offset exceeds the renderer's 32-bit range");
                candidate.drawRanges.push_back({baseIndex + subset.indexOffset,
                                                renderIndexCount,
                                                subset.submaterialIndex});
            }
        }

        if (candidate.drawRanges.empty())
            return Fail(error, "NMF geometry has no complete subset triangle range to preview");

        for (const auto& range : candidate.drawRanges) {
            if (range.indexOffset > candidate.indices.size() ||
                range.indexCount > candidate.indices.size() - range.indexOffset ||
                range.indexCount % 3 != 0)
                return Fail(error, "NMF preview draw range is invalid after index rebasing");
            candidate.drawRangeTriangleCount += range.indexCount / 3;
            for (std::size_t index = range.indexOffset;
                 index < static_cast<std::size_t>(range.indexOffset) + range.indexCount;
                 index += 3) {
                const auto& a = candidate.positions[candidate.indices[index]];
                const auto& b = candidate.positions[candidate.indices[index + 1]];
                const auto& c = candidate.positions[candidate.indices[index + 2]];
                const double abx = static_cast<double>(b.x) - a.x;
                const double aby = static_cast<double>(b.y) - a.y;
                const double abz = static_cast<double>(b.z) - a.z;
                const double acx = static_cast<double>(c.x) - a.x;
                const double acy = static_cast<double>(c.y) - a.y;
                const double acz = static_cast<double>(c.z) - a.z;
                const double crossX = aby * acz - abz * acy;
                const double crossY = abz * acx - abx * acz;
                const double crossZ = abx * acy - aby * acx;
                if (crossX == 0.0 && crossY == 0.0 && crossZ == 0.0)
                    ++candidate.zeroAreaTriangleCount;
            }
        }
    } catch (const std::bad_alloc&) {
        return Fail(error, "unable to allocate bounded NMF preview geometry");
    } catch (const std::length_error&) {
        return Fail(error, "bounded NMF preview geometry exceeds a container limit");
    }

    output = std::move(candidate);
    if (error)
        error->clear();
    return true;
}

} // namespace openrepublic::content
