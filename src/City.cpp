#include "City.h"

#include <fstream>
#include <array>
#include <cmath>
#include <algorithm>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <limits>
#include <cstdint>

namespace {

// Write a rectangular prism defined by four base corners to an OBJ stream.
// The corners should be specified in winding order around the base face.
void writePrism(std::ofstream &ofs,
                const std::array<std::pair<double, double>, 4> &base,
                double baseZ,
                double topZ,
                std::size_t &vertexOffset) {
    ofs << "v " << base[0].first << " " << base[0].second << " " << baseZ << "\n";
    ofs << "v " << base[1].first << " " << base[1].second << " " << baseZ << "\n";
    ofs << "v " << base[2].first << " " << base[2].second << " " << baseZ << "\n";
    ofs << "v " << base[3].first << " " << base[3].second << " " << baseZ << "\n";
    ofs << "v " << base[0].first << " " << base[0].second << " " << topZ << "\n";
    ofs << "v " << base[1].first << " " << base[1].second << " " << topZ << "\n";
    ofs << "v " << base[2].first << " " << base[2].second << " " << topZ << "\n";
    ofs << "v " << base[3].first << " " << base[3].second << " " << topZ << "\n";
    auto v = vertexOffset;
    ofs << "f " << v << " " << v + 1 << " " << v + 2 << "\n";
    ofs << "f " << v << " " << v + 2 << " " << v + 3 << "\n";
    ofs << "f " << v + 4 << " " << v + 7 << " " << v + 6 << "\n";
    ofs << "f " << v + 4 << " " << v + 6 << " " << v + 5 << "\n";
    ofs << "f " << v << " " << v + 4 << " " << v + 5 << "\n";
    ofs << "f " << v << " " << v + 5 << " " << v + 1 << "\n";
    ofs << "f " << v + 1 << " " << v + 5 << " " << v + 6 << "\n";
    ofs << "f " << v + 1 << " " << v + 6 << " " << v + 2 << "\n";
    ofs << "f " << v + 2 << " " << v + 6 << " " << v + 7 << "\n";
    ofs << "f " << v + 2 << " " << v + 7 << " " << v + 3 << "\n";
    ofs << "f " << v + 3 << " " << v + 7 << " " << v + 4 << "\n";
    ofs << "f " << v + 3 << " " << v + 4 << " " << v << "\n";
    vertexOffset += 8;
}

// Convenience helper to extrude an axis-aligned rectangle into a prism.
void writeRectPrism(std::ofstream &ofs, const Rect &r,
                    double baseZ, double topZ, std::size_t &vertexOffset) {
    std::array<std::pair<double, double>, 4> base = {{
        {r.x0, r.y0},
        {r.x1, r.y0},
        {r.x1, r.y1},
        {r.x0, r.y1}
    }};
    writePrism(ofs, base, baseZ, topZ, vertexOffset);
}

// Inset a rectangle by a fixed amount, clamping so the rectangle never flips.
Rect insetRect(const Rect &r, double inset) {
    Rect out = r;
    double maxInset = std::min(r.width(), r.height()) * 0.49;
    double applied = std::clamp(inset, 0.0, maxInset);
    out.x0 += applied;
    out.x1 -= applied;
    out.y0 += applied;
    out.y1 -= applied;
    return out;
}

// Extract filename component for mtllib usage.
std::string filenameOnly(const std::string &path) {
    std::size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

// Replace the extension of a filename with a new extension.
std::string replaceExtension(const std::string &path, const std::string &ext) {
    std::size_t slash = path.find_last_of("/\\");
    std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        dot = path.size();
    }
    return path.substr(0, dot) + ext;
}

struct MaterialDef {
    const char *name;
    double r;
    double g;
    double b;
    double ks;
    double shininess;
    double metallic;
    double roughness;
};

static const MaterialDef kMaterialPalette[] = {
    {"mat_default", 0.7, 0.7, 0.7, 0.05, 32.0, 0.0, 0.6},
    {"mat_commercial", 0.6, 0.65, 0.72, 0.5, 96.0, 0.05, 0.35}, // glassy grey
    {"mat_residential", 0.83, 0.72, 0.62, 0.08, 48.0, 0.0, 0.55}, // warm tones
    {"mat_industrial", 0.32, 0.34, 0.36, 0.04, 24.0, 0.02, 0.75}, // muted dark
    {"mat_green", 0.3, 0.62, 0.34, 0.02, 12.0, 0.0, 0.7}, // vegetation
    {"mat_road", 0.15, 0.15, 0.15, 0.02, 12.0, 0.0, 0.8} // asphalt
};

const MaterialDef *findMaterialDef(const std::string &name) {
    for (const auto &m : kMaterialPalette) {
        if (name == m.name) return &m;
    }
    return nullptr;
}

// Material palette per zone/element.
const char *materialForZone(ZoneType zone) {
    switch (zone) {
        case ZoneType::Commercial: return "mat_commercial";
        case ZoneType::Residential: return "mat_residential";
        case ZoneType::Industrial: return "mat_industrial";
        case ZoneType::Green: return "mat_green";
        default: return "mat_default";
    }
}

// Emit a single material block to an MTL stream.
void writeMaterial(std::ofstream &mtl, const std::string &name,
                   double r, double g, double b,
                   double ks, double shininess) {
    double ka = 0.25;
    mtl << "newmtl " << name << "\n";
    mtl << "Ka " << ka * r << " " << ka * g << " " << ka * b << "\n";
    mtl << "Kd " << r << " " << g << " " << b << "\n";
    mtl << "Ks " << ks << " " << ks << " " << ks << "\n";
    mtl << "Ns " << shininess << "\n";
    mtl << "d 1.0\n";
    mtl << "illum 2\n\n";
}

bool writeMaterialsFile(const std::string &mtlPath) {
    std::ofstream mtl(mtlPath);
    if (!mtl) return false;
    for (const auto &m : kMaterialPalette) {
        writeMaterial(mtl, m.name, m.r, m.g, m.b, m.ks, m.shininess);
    }
    return true;
}

constexpr double kRoadThickness = 0.05;

struct Vec3 {
    double x;
    double y;
    double z;
};

// Map internal coordinates (X horizontal, Y horizontal, Z up) into glTF's
// Y‑up convention (X/Z ground plane, +Y up).
Vec3 toGltfCoords(double x, double y, double z) {
    return {x, z, y};
}

struct MeshBuffer {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<std::uint32_t> indices;
    bool hasBounds = false;
    std::array<double, 3> minPos{};
    std::array<double, 3> maxPos{};
};

void updateBounds(MeshBuffer &buf, const Vec3 &p) {
    if (!buf.hasBounds) {
        buf.minPos = {p.x, p.y, p.z};
        buf.maxPos = buf.minPos;
        buf.hasBounds = true;
        return;
    }
    buf.minPos[0] = std::min(buf.minPos[0], p.x);
    buf.minPos[1] = std::min(buf.minPos[1], p.y);
    buf.minPos[2] = std::min(buf.minPos[2], p.z);
    buf.maxPos[0] = std::max(buf.maxPos[0], p.x);
    buf.maxPos[1] = std::max(buf.maxPos[1], p.y);
    buf.maxPos[2] = std::max(buf.maxPos[2], p.z);
}

void appendTriangle(MeshBuffer &buf, const Vec3 &p0, const Vec3 &p1,
                    const Vec3 &p2, const Vec3 &n) {
    std::uint32_t base = static_cast<std::uint32_t>(buf.positions.size() / 3);
    auto pushPos = [&](const Vec3 &p) {
        buf.positions.push_back(static_cast<float>(p.x));
        buf.positions.push_back(static_cast<float>(p.y));
        buf.positions.push_back(static_cast<float>(p.z));
        updateBounds(buf, p);
    };
    auto pushNormal = [&](const Vec3 &nn) {
        buf.normals.push_back(static_cast<float>(nn.x));
        buf.normals.push_back(static_cast<float>(nn.y));
        buf.normals.push_back(static_cast<float>(nn.z));
    };
    pushPos(p0); pushPos(p1); pushPos(p2);
    pushNormal(n); pushNormal(n); pushNormal(n);
    buf.indices.push_back(base);
    buf.indices.push_back(base + 1);
    buf.indices.push_back(base + 2);
}

void appendRectPrism(MeshBuffer &buf, const Rect &r,
                     double baseZ, double topZ) {
    Vec3 p0 = toGltfCoords(r.x0, r.y0, baseZ);
    Vec3 p1 = toGltfCoords(r.x1, r.y0, baseZ);
    Vec3 p2 = toGltfCoords(r.x1, r.y1, baseZ);
    Vec3 p3 = toGltfCoords(r.x0, r.y1, baseZ);
    Vec3 p4 = toGltfCoords(r.x0, r.y0, topZ);
    Vec3 p5 = toGltfCoords(r.x1, r.y0, topZ);
    Vec3 p6 = toGltfCoords(r.x1, r.y1, topZ);
    Vec3 p7 = toGltfCoords(r.x0, r.y1, topZ);
    const Vec3 nDown{0.0, -1.0, 0.0};
    const Vec3 nUp{0.0, 1.0, 0.0};
    const Vec3 nPosX{1.0, 0.0, 0.0};
    const Vec3 nNegX{-1.0, 0.0, 0.0};
    const Vec3 nPosZ{0.0, 0.0, 1.0};
    const Vec3 nNegZ{0.0, 0.0, -1.0};
    // bottom
    appendTriangle(buf, p0, p2, p1, nDown);
    appendTriangle(buf, p0, p3, p2, nDown);
    // top
    appendTriangle(buf, p4, p5, p6, nUp);
    appendTriangle(buf, p4, p6, p7, nUp);
    // +X
    appendTriangle(buf, p1, p2, p6, nPosX);
    appendTriangle(buf, p1, p6, p5, nPosX);
    // -X
    appendTriangle(buf, p3, p0, p4, nNegX);
    appendTriangle(buf, p3, p4, p7, nNegX);
    // +Z (internal +Y)
    appendTriangle(buf, p2, p3, p7, nPosZ);
    appendTriangle(buf, p2, p7, p6, nPosZ);
    // -Z (internal -Y)
    appendTriangle(buf, p0, p1, p5, nNegZ);
    appendTriangle(buf, p0, p5, p4, nNegZ);
}

} // namespace

City::City(int s) : size(s) {
    zones.resize(size * size, ZoneType::None);
}

void City::saveOBJ(const std::string &filename) const {
    // Precompute and emit MTL palette
    std::string mtlPath = replaceExtension(filename, ".mtl");
    bool hasMtl = writeMaterialsFile(mtlPath);
    std::string mtlName = filenameOnly(mtlPath);

    std::ofstream ofs(filename);
    if (!ofs) return;
    if (hasMtl) {
        ofs << "mtllib " << mtlName << "\n";
    }
    // Accumulate vertices and faces.  We write one object per parcel-based
    // building for clarity, but the file can contain thousands of objects.
    // A running vertex index is maintained to offset face indices.
    std::size_t vertexOffset = 1;
    auto emitStandard = [&](const Building &b) {
        double h = std::max(1.0, static_cast<double>(b.height));
        writeRectPrism(ofs, b.footprint, 0.0, h, vertexOffset);
    };
    auto emitPark = [&](const Rect &fp) {
        double margin = std::min(fp.width(), fp.height()) * 0.08;
        Rect lawn = insetRect(fp, margin);
        double padHeight = 0.08;
        writeRectPrism(ofs, lawn, 0.0, padHeight, vertexOffset);
        double baseSize = std::min(lawn.width(), lawn.height()) * 0.2;
        double planterSize = std::clamp(baseSize, 0.2, std::min(lawn.width(), lawn.height()) * 0.45);
        Rect planterA{lawn.x0, lawn.y0, lawn.x0 + planterSize, lawn.y0 + planterSize};
        Rect planterB{lawn.x1 - planterSize, lawn.y1 - planterSize, lawn.x1, lawn.y1};
        double planterHeight = padHeight * 2.5;
        writeRectPrism(ofs, planterA, padHeight, padHeight + planterHeight, vertexOffset);
        writeRectPrism(ofs, planterB, padHeight, padHeight + planterHeight, vertexOffset);
    };
    auto emitSchool = [&](const Building &b) {
        const Rect &fp = b.footprint;
        double w = fp.width();
        double h = fp.height();
        Rect field = insetRect(fp, std::min(w, h) * 0.07);
        double fieldHeight = 0.05;
        writeRectPrism(ofs, field, 0.0, fieldHeight, vertexOffset);
        bool wide = w >= h;
        double buildingW = wide ? w * 0.45 : w * 0.6;
        double buildingH = wide ? h * 0.6 : h * 0.45;
        Rect buildingRect;
        buildingRect.x0 = fp.x0 + w * 0.08;
        buildingRect.y0 = fp.y0 + h * (wide ? 0.2 : 0.08);
        buildingRect.x1 = buildingRect.x0 + buildingW;
        buildingRect.y1 = buildingRect.y0 + buildingH;
        double maxX = fp.x1 - w * 0.05;
        double maxY = fp.y1 - h * 0.05;
        if (buildingRect.x1 > maxX) {
            double shift = buildingRect.x1 - maxX;
            buildingRect.x0 -= shift;
            buildingRect.x1 -= shift;
        }
        if (buildingRect.y1 > maxY) {
            double shift = buildingRect.y1 - maxY;
            buildingRect.y0 -= shift;
            buildingRect.y1 -= shift;
        }
        double schoolHeight = std::max(2.0, static_cast<double>(b.height));
        writeRectPrism(ofs, buildingRect, 0.0, schoolHeight, vertexOffset);
    };
    auto emitHospital = [&](const Building &b) {
        const Rect &fp = b.footprint;
        double w = fp.width();
        double h = fp.height();
        Rect podium = insetRect(fp, std::min(w, h) * 0.08);
        double podiumTop = std::max(1.2, static_cast<double>(b.height) * 0.25);
        writeRectPrism(ofs, podium, 0.0, podiumTop, vertexOffset);
        double cx = fp.centreX();
        double cy = fp.centreY();
        bool wide = w >= h;
        double mainW = wide ? w * 0.7 : w * 0.45;
        double mainH = wide ? h * 0.45 : h * 0.7;
        Rect main{cx - mainW * 0.5, cy - mainH * 0.5, cx + mainW * 0.5, cy + mainH * 0.5};
        double mainTop = std::max(podiumTop + 2.0, static_cast<double>(b.height));
        writeRectPrism(ofs, main, podiumTop, mainTop, vertexOffset);
        double wingW = wide ? w * 0.28 : w * 0.85;
        double wingH = wide ? h * 0.85 : h * 0.28;
        Rect wing{cx - wingW * 0.5, cy - wingH * 0.5, cx + wingW * 0.5, cy + wingH * 0.5};
        double wingTop = std::max(podiumTop + 1.2, mainTop * 0.9);
        writeRectPrism(ofs, wing, podiumTop, wingTop, vertexOffset);
    };
    for (const auto &b : buildings) {
        if (b.zone == ZoneType::None) continue;
        if (b.zone == ZoneType::Green) {
            ofs << "usemtl " << materialForZone(b.zone) << "\n";
            emitPark(b.footprint);
            continue;
        }
        if (b.facility) {
            ofs << "usemtl " << materialForZone(b.zone) << "\n";
            if (b.facilityType == Facility::Type::Hospital) {
                emitHospital(b);
            } else {
                emitSchool(b);
            }
            continue;
        }
        ofs << "usemtl " << materialForZone(b.zone) << "\n";
        emitStandard(b);
    }
    // Roads: extrude each centreline into a thin rectangular prism so that
    // the street hierarchy is visible in the 3D export.
    for (const auto &road : roads) {
        ofs << "usemtl mat_road\n";
        double dx = road.x2 - road.x1;
        double dy = road.y2 - road.y1;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6) continue;
        double invLen = 1.0 / len;
        double nx = -dy * invLen;
        double ny = dx * invLen;
        double halfWidth = 0.5 * roadWidth(road.type);
        double hx = nx * halfWidth;
        double hy = ny * halfWidth;
        std::array<std::pair<double, double>, 4> base = {{
            {road.x1 + hx, road.y1 + hy},
            {road.x1 - hx, road.y1 - hy},
            {road.x2 - hx, road.y2 - hy},
            {road.x2 + hx, road.y2 + hy}
        }};
        writePrism(ofs, base, 0.0, kRoadThickness, vertexOffset);
    }
    ofs.close();
}

void City::saveGLTF(const std::string &filename, bool binary) const {
    std::unordered_map<std::string, MeshBuffer> meshByMaterial;
    auto bufferFor = [&](const std::string &mat) -> MeshBuffer & {
        return meshByMaterial[mat];
    };
    auto emitStandard = [&](const Building &b) {
        double h = std::max(1.0, static_cast<double>(b.height));
        appendRectPrism(bufferFor(materialForZone(b.zone)), b.footprint, 0.0, h);
    };
    auto emitPark = [&](const Rect &fp) {
        double margin = std::min(fp.width(), fp.height()) * 0.08;
        Rect lawn = insetRect(fp, margin);
        double padHeight = 0.08;
        appendRectPrism(bufferFor("mat_green"), lawn, 0.0, padHeight);
        double baseSize = std::min(lawn.width(), lawn.height()) * 0.2;
        double planterSize = std::clamp(baseSize, 0.2, std::min(lawn.width(), lawn.height()) * 0.45);
        Rect planterA{lawn.x0, lawn.y0, lawn.x0 + planterSize, lawn.y0 + planterSize};
        Rect planterB{lawn.x1 - planterSize, lawn.y1 - planterSize, lawn.x1, lawn.y1};
        double planterHeight = padHeight * 2.5;
        appendRectPrism(bufferFor("mat_green"), planterA, padHeight, padHeight + planterHeight);
        appendRectPrism(bufferFor("mat_green"), planterB, padHeight, padHeight + planterHeight);
    };
    auto emitSchool = [&](const Building &b) {
        const Rect &fp = b.footprint;
        double w = fp.width();
        double h = fp.height();
        Rect field = insetRect(fp, std::min(w, h) * 0.07);
        double fieldHeight = 0.05;
        appendRectPrism(bufferFor(materialForZone(b.zone)), field, 0.0, fieldHeight);
        bool wide = w >= h;
        double buildingW = wide ? w * 0.45 : w * 0.6;
        double buildingH = wide ? h * 0.6 : h * 0.45;
        Rect buildingRect;
        buildingRect.x0 = fp.x0 + w * 0.08;
        buildingRect.y0 = fp.y0 + h * (wide ? 0.2 : 0.08);
        buildingRect.x1 = buildingRect.x0 + buildingW;
        buildingRect.y1 = buildingRect.y0 + buildingH;
        double maxX = fp.x1 - w * 0.05;
        double maxY = fp.y1 - h * 0.05;
        if (buildingRect.x1 > maxX) {
            double shift = buildingRect.x1 - maxX;
            buildingRect.x0 -= shift;
            buildingRect.x1 -= shift;
        }
        if (buildingRect.y1 > maxY) {
            double shift = buildingRect.y1 - maxY;
            buildingRect.y0 -= shift;
            buildingRect.y1 -= shift;
        }
        double schoolHeight = std::max(2.0, static_cast<double>(b.height));
        appendRectPrism(bufferFor(materialForZone(b.zone)), buildingRect, 0.0, schoolHeight);
    };
    auto emitHospital = [&](const Building &b) {
        const Rect &fp = b.footprint;
        double w = fp.width();
        double h = fp.height();
        Rect podium = insetRect(fp, std::min(w, h) * 0.08);
        double podiumTop = std::max(1.2, static_cast<double>(b.height) * 0.25);
        appendRectPrism(bufferFor(materialForZone(b.zone)), podium, 0.0, podiumTop);
        double cx = fp.centreX();
        double cy = fp.centreY();
        bool wide = w >= h;
        double mainW = wide ? w * 0.7 : w * 0.45;
        double mainH = wide ? h * 0.45 : h * 0.7;
        Rect main{cx - mainW * 0.5, cy - mainH * 0.5, cx + mainW * 0.5, cy + mainH * 0.5};
        double mainTop = std::max(podiumTop + 2.0, static_cast<double>(b.height));
        appendRectPrism(bufferFor(materialForZone(b.zone)), main, podiumTop, mainTop);
        double wingW = wide ? w * 0.28 : w * 0.85;
        double wingH = wide ? h * 0.85 : h * 0.28;
        Rect wing{cx - wingW * 0.5, cy - wingH * 0.5, cx + wingW * 0.5, cy + wingH * 0.5};
        double wingTop = std::max(podiumTop + 1.2, mainTop * 0.9);
        appendRectPrism(bufferFor(materialForZone(b.zone)), wing, podiumTop, wingTop);
    };
    for (const auto &b : buildings) {
        if (b.zone == ZoneType::None) continue;
        if (b.zone == ZoneType::Green) {
            emitPark(b.footprint);
            continue;
        }
        if (b.facility) {
            if (b.facilityType == Facility::Type::Hospital) {
                emitHospital(b);
            } else {
                emitSchool(b);
            }
            continue;
        }
        emitStandard(b);
    }
    for (const auto &road : roads) {
        double dx = road.x2 - road.x1;
        double dy = road.y2 - road.y1;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6) continue;
        double invLen = 1.0 / len;
        double nx = -dy * invLen;
        double ny = dx * invLen;
        double halfWidth = 0.5 * roadWidth(road.type);
        double hx = nx * halfWidth;
        double hy = ny * halfWidth;
        Rect base{road.x1 + hx, road.y1 + hy, road.x2 - hx, road.y2 - hy};
        // Base rectangle might flip if hx/hy reorder bounds; normalise bounds.
        if (base.x0 > base.x1) std::swap(base.x0, base.x1);
        if (base.y0 > base.y1) std::swap(base.y0, base.y1);
        appendRectPrism(bufferFor("mat_road"), base, 0.0, kRoadThickness);
    }

    // Collect used materials in palette order so indices are stable.
    std::vector<const MaterialDef *> materials;
    std::unordered_map<std::string, int> materialIndex;
    for (const auto &def : kMaterialPalette) {
        auto it = meshByMaterial.find(def.name);
        if (it != meshByMaterial.end() && !it->second.indices.empty()) {
            int idx = static_cast<int>(materials.size());
            materials.push_back(&def);
            materialIndex[def.name] = idx;
        }
    }

    struct ViewInfo { std::size_t offset; std::size_t length; int target; };
    struct AccessorInfo {
        std::size_t bufferView;
        std::size_t count;
        int componentType;
        std::string type;
        bool hasMinMax = false;
        std::array<double, 3> min{};
        std::array<double, 3> max{};
    };
    struct MeshPrimitive {
        int positionAccessor = -1;
        int normalAccessor = -1;
        int indexAccessor = -1;
        int material = -1;
        std::string name;
    };

    std::vector<std::uint8_t> binData;
    auto align4 = [&](std::vector<std::uint8_t> &v) {
        while (v.size() % 4 != 0) v.push_back(0);
    };
    auto appendBytes = [&](const void *ptr, std::size_t len) -> std::size_t {
        align4(binData);
        std::size_t offset = binData.size();
        const auto *bytes = reinterpret_cast<const std::uint8_t *>(ptr);
        binData.insert(binData.end(), bytes, bytes + len);
        return offset;
    };
    std::vector<ViewInfo> views;
    std::vector<AccessorInfo> accessors;
    std::vector<MeshPrimitive> primitives;

    for (const auto *mat : materials) {
        auto it = meshByMaterial.find(mat->name);
        if (it == meshByMaterial.end()) continue;
        const MeshBuffer &buf = it->second;
        if (buf.indices.empty() || buf.positions.empty()) continue;
        // positions
        std::size_t posOffset = appendBytes(buf.positions.data(), buf.positions.size() * sizeof(float));
        views.push_back({posOffset, buf.positions.size() * sizeof(float), 34962});
        std::size_t posAccessor = accessors.size();
        AccessorInfo posAcc;
        posAcc.bufferView = views.size() - 1;
        posAcc.count = buf.positions.size() / 3;
        posAcc.componentType = 5126;
        posAcc.type = "VEC3";
        if (buf.hasBounds) {
            posAcc.hasMinMax = true;
            posAcc.min = buf.minPos;
            posAcc.max = buf.maxPos;
        }
        accessors.push_back(posAcc);
        // normals
        std::size_t normOffset = appendBytes(buf.normals.data(), buf.normals.size() * sizeof(float));
        views.push_back({normOffset, buf.normals.size() * sizeof(float), 34962});
        std::size_t normAccessor = accessors.size();
        AccessorInfo normAcc;
        normAcc.bufferView = views.size() - 1;
        normAcc.count = buf.normals.size() / 3;
        normAcc.componentType = 5126;
        normAcc.type = "VEC3";
        accessors.push_back(normAcc);
        // indices
        std::size_t idxOffset = appendBytes(buf.indices.data(), buf.indices.size() * sizeof(std::uint32_t));
        views.push_back({idxOffset, buf.indices.size() * sizeof(std::uint32_t), 34963});
        std::size_t idxAccessor = accessors.size();
        AccessorInfo idxAcc;
        idxAcc.bufferView = views.size() - 1;
        idxAcc.count = buf.indices.size();
        idxAcc.componentType = 5125;
        idxAcc.type = "SCALAR";
        accessors.push_back(idxAcc);

        MeshPrimitive prim;
        prim.positionAccessor = static_cast<int>(posAccessor);
        prim.normalAccessor = static_cast<int>(normAccessor);
        prim.indexAccessor = static_cast<int>(idxAccessor);
        prim.material = materialIndex[mat->name];
        prim.name = mat->name;
        primitives.push_back(prim);
    }

    // Compose glTF JSON
    std::ostringstream oss;
    oss << "{";
    oss << "\"asset\":{\"version\":\"2.0\",\"generator\":\"citygen\"},";
    // nodes/scene
    oss << "\"scene\":0,";
    oss << "\"scenes\":[{\"nodes\":[";
    for (std::size_t i = 0; i < primitives.size(); ++i) {
        if (i) oss << ",";
        oss << i;
    }
    oss << "]}],";
    // nodes
    oss << "\"nodes\":[";
    for (std::size_t i = 0; i < primitives.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"mesh\":" << i << "}";
    }
    oss << "],";
    // materials
    oss << "\"materials\":[";
    for (std::size_t i = 0; i < materials.size(); ++i) {
        if (i) oss << ",";
        const auto *m = materials[i];
        oss << "{\"name\":\"" << m->name << "\",";
        oss << "\"pbrMetallicRoughness\":{\"baseColorFactor\":["
            << m->r << "," << m->g << "," << m->b << ",1],";
        oss << "\"metallicFactor\":" << m->metallic << ",";
        oss << "\"roughnessFactor\":" << m->roughness << "},";
        oss << "\"doubleSided\":true}";
    }
    oss << "],";
    // meshes
    oss << "\"meshes\":[";
    for (std::size_t i = 0; i < primitives.size(); ++i) {
        if (i) oss << ",";
        const auto &p = primitives[i];
        oss << "{\"name\":\"" << p.name << "\",\"primitives\":[{";
        oss << "\"attributes\":{\"POSITION\":" << p.positionAccessor
            << ",\"NORMAL\":" << p.normalAccessor << "},";
        oss << "\"indices\":" << p.indexAccessor << ",";
        oss << "\"material\":" << p.material;
        oss << "}]}";
    }
    oss << "],";
    // accessors
    oss << "\"accessors\":[";
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        if (i) oss << ",";
        const auto &a = accessors[i];
        oss << "{\"bufferView\":" << a.bufferView
            << ",\"componentType\":" << a.componentType
            << ",\"count\":" << a.count
            << ",\"type\":\"" << a.type << "\"";
        if (a.hasMinMax) {
            oss << ",\"min\":[" << a.min[0] << "," << a.min[1] << "," << a.min[2] << "]";
            oss << ",\"max\":[" << a.max[0] << "," << a.max[1] << "," << a.max[2] << "]";
        }
        oss << "}";
    }
    oss << "],";
    // bufferViews
    oss << "\"bufferViews\":[";
    for (std::size_t i = 0; i < views.size(); ++i) {
        if (i) oss << ",";
        const auto &v = views[i];
        oss << "{\"buffer\":0,"
            << "\"byteOffset\":" << v.offset
            << ",\"byteLength\":" << v.length
            << ",\"target\":" << v.target
            << "}";
    }
    oss << "],";
    // buffers
    std::string binFilename = replaceExtension(filename, ".bin");
    oss << "\"buffers\":[{";
    oss << "\"byteLength\":" << binData.size();
    if (!binary) {
        oss << ",\"uri\":\"" << filenameOnly(binFilename) << "\"";
    }
    oss << "}]}";
    std::string json = oss.str();

    if (binary) {
        // Write GLB (JSON + BIN)
        std::ofstream ofs(filename, std::ios::binary);
        if (!ofs) return;
        // Pad JSON to 4-byte boundary with spaces
        std::vector<std::uint8_t> jsonBytes(json.begin(), json.end());
        while (jsonBytes.size() % 4 != 0) jsonBytes.push_back(0x20);
        align4(binData);
        std::uint32_t totalLength = 12 // header
            + 8 + static_cast<std::uint32_t>(jsonBytes.size())
            + 8 + static_cast<std::uint32_t>(binData.size());
        ofs.write("glTF", 4);
        std::uint32_t version = 2;
        ofs.write(reinterpret_cast<const char *>(&version), sizeof(version));
        ofs.write(reinterpret_cast<const char *>(&totalLength), sizeof(totalLength));
        std::uint32_t jsonLength = static_cast<std::uint32_t>(jsonBytes.size());
        std::uint32_t jsonType = 0x4E4F534Au; // JSON
        ofs.write(reinterpret_cast<const char *>(&jsonLength), sizeof(jsonLength));
        ofs.write(reinterpret_cast<const char *>(&jsonType), sizeof(jsonType));
        ofs.write(reinterpret_cast<const char *>(jsonBytes.data()), jsonBytes.size());
        std::uint32_t binLength = static_cast<std::uint32_t>(binData.size());
        std::uint32_t binType = 0x004E4942u; // BIN
        ofs.write(reinterpret_cast<const char *>(&binLength), sizeof(binLength));
        ofs.write(reinterpret_cast<const char *>(&binType), sizeof(binType));
        ofs.write(reinterpret_cast<const char *>(binData.data()), binData.size());
    } else {
        align4(binData);
        std::ofstream binOut(binFilename, std::ios::binary);
        if (!binOut) return;
        binOut.write(reinterpret_cast<const char *>(binData.data()),
                     static_cast<std::streamsize>(binData.size()));
        binOut.close();
        std::ofstream gltfOut(filename);
        if (!gltfOut) return;
        gltfOut << json;
    }
}

void City::saveSummary(const std::string &filename) const {
    std::ofstream ofs(filename);
    if (!ofs) return;
    // Count metrics
    std::size_t countResidential = 0;
    std::size_t countCommercial = 0;
    std::size_t countIndustrial = 0;
    std::size_t countGreen = 0;
    std::size_t countUndeveloped = 0;
    std::size_t totalBuildings = 0;
    int maxResidentialHeight = 0;
    int maxCommercialHeight = 0;
    int maxIndustrialHeight = 0;
    for (const auto z : zones) {
        if (z == ZoneType::None) { countUndeveloped++; continue; }
        if (z == ZoneType::Residential) countResidential++;
        else if (z == ZoneType::Commercial) countCommercial++;
        else if (z == ZoneType::Industrial) countIndustrial++;
        else if (z == ZoneType::Green) countGreen++;
    }
    std::vector<std::pair<double, double>> schoolPos;
    std::vector<std::pair<double, double>> hospitalPos;
    schoolPos.reserve(facilities.size());
    hospitalPos.reserve(facilities.size());
    for (const auto &f : facilities) {
        if (f.type == Facility::Type::School) schoolPos.push_back({f.x, f.y});
        else if (f.type == Facility::Type::Hospital) hospitalPos.push_back({f.x, f.y});
    }
    auto nearest = [](double x, double y, const std::vector<std::pair<double, double>> &pts) {
        if (pts.empty()) return -1.0;
        double best = std::numeric_limits<double>::max();
        for (const auto &p : pts) {
            double dx = x - p.first;
            double dy = y - p.second;
            double d = std::sqrt(dx * dx + dy * dy);
            if (d < best) best = d;
        }
        return best;
    };
    double maxDistSchool = -1.0;
    double maxDistHospital = -1.0;
    for (const auto &b : buildings) {
        if (b.zone != ZoneType::None && b.zone != ZoneType::Green) {
            totalBuildings++;
        }
        if (b.zone == ZoneType::Residential) {
            maxResidentialHeight = std::max(maxResidentialHeight, b.height);
            if (!schoolPos.empty()) {
                double d = nearest(b.footprint.centreX(), b.footprint.centreY(), schoolPos);
                if (d > maxDistSchool) maxDistSchool = d;
            }
            if (!hospitalPos.empty()) {
                double d = nearest(b.footprint.centreX(), b.footprint.centreY(), hospitalPos);
                if (d > maxDistHospital) maxDistHospital = d;
            }
        } else if (b.zone == ZoneType::Commercial) {
            maxCommercialHeight = std::max(maxCommercialHeight, b.height);
        } else if (b.zone == ZoneType::Industrial) {
            maxIndustrialHeight = std::max(maxIndustrialHeight, b.height);
        }
    }
    std::size_t countHospitals = 0;
    std::size_t countSchools = 0;
    for (const auto &f : facilities) {
        if (f.type == Facility::Type::Hospital) countHospitals++;
        else if (f.type == Facility::Type::School) countSchools++;
    }
    // Write JSON.  Note: this is simplistic and not pretty‑printed.
    ofs << "{\n";
    ofs << "  \"gridSize\": " << size << ",\n";
    ofs << "  \"totalBuildings\": " << totalBuildings << ",\n";
    ofs << "  \"residentialCells\": " << countResidential << ",\n";
    ofs << "  \"commercialCells\": " << countCommercial << ",\n";
    ofs << "  \"industrialCells\": " << countIndustrial << ",\n";
    ofs << "  \"greenCells\": " << countGreen << ",\n";
    ofs << "  \"undevelopedCells\": " << countUndeveloped << ",\n";
    ofs << "  \"numHospitals\": " << countHospitals << ",\n";
    ofs << "  \"numSchools\": " << countSchools << ",\n";
    ofs << "  \"maxDistanceToSchool\": " << maxDistSchool << ",\n";
    ofs << "  \"maxDistanceToHospital\": " << maxDistHospital << ",\n";
    ofs << "  \"maxResidentialHeight\": " << maxResidentialHeight << ",\n";
    ofs << "  \"maxCommercialHeight\": " << maxCommercialHeight << ",\n";
    ofs << "  \"maxIndustrialHeight\": " << maxIndustrialHeight << "\n";
    ofs << "}";
    ofs.close();
}
