#include "openrepublic/procgeo/mesh.h"

#include <iostream>

using namespace openrepublic::procgeo;

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
        if (!ok) {
            std::cerr << "FAIL: " << label << '\n';
            ++failures;
        }
    };
    Mesh box;
    AddBox(box, {0, 1, 0}, {1, 1, 2}, {0.5f, 0.5f, 0.5f});
    check(box.Valid() && box.positions.size() == 24 && box.indices.size() == 36, "box has 24 vertices and 12 triangles");
    Vec3 lo, hi;
    check(box.Bounds(lo, hi) && lo.y == 0.0f && hi.y == 2.0f && hi.z == 2.0f && lo.x == -1.0f, "box bounds");
    // Every triangle must face away from the box centre (counter-clockwise from outside).
    bool outward = true;
    for (std::size_t i = 0; i < box.indices.size(); i += 3) {
        const Vec3 &a = box.positions[box.indices[i]], &b = box.positions[box.indices[i + 1]],
                   &c = box.positions[box.indices[i + 2]];
        const Vec3 e1{b.x - a.x, b.y - a.y, b.z - a.z}, e2{c.x - a.x, c.y - a.y, c.z - a.z};
        const Vec3 n{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
        const Vec3 toFace{a.x - 0.0f, a.y - 1.0f, a.z - 0.0f};
        outward = outward && (n.x * toFace.x + n.y * toFace.y + n.z * toFace.z) > 0.0f;
    }
    check(outward, "box triangles wind counter-clockwise from outside");
    Mesh cyl;
    AddCylinder(cyl, 0, 0, 0, 3, 0.5f, 8, {1, 1, 1});
    check(cyl.Valid() && !cyl.indices.empty(), "cylinder is valid");
    Mesh merged;
    merged.Append(box, {5, 0, 0});
    merged.Append(cyl);
    check(merged.Valid() && merged.positions.size() == box.positions.size() + cyl.positions.size(), "append rebases indices");
    if (failures) {
        return 1;
    }
    std::cout << "All procgeo mesh tests passed\n";
    return 0;
}
