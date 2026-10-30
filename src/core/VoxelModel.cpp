#include "core/VoxelModel.h"

#include <cstdio>
#include <unordered_map>

namespace lc {

namespace {

std::string escapeJson(std::string const& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}

std::size_t skipWs(std::string const& s, std::size_t pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n')) ++pos;
    return pos;
}

bool findValueAfterKey(std::string const& s, std::string const& key, std::size_t& valuePos) {
    std::string pat = "\"" + key + "\"";
    auto        p   = s.find(pat);
    if (p == std::string::npos) return false;
    p = s.find(':', p + pat.size());
    if (p == std::string::npos) return false;
    valuePos = skipWs(s, p + 1);
    return true;
}

// Reads a JSON string starting at s[pos] == '"'. Advances pos past the closing quote.
bool readJsonString(std::string const& s, std::size_t& pos, std::string& out) {
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != '"') return false;
    ++pos;
    out.clear();
    while (pos < s.size()) {
        char c = s[pos];
        if (c == '"') {
            ++pos;
            return true;
        }
        if (c == '\\') {
            ++pos;
            if (pos >= s.size()) return false;
            char e = s[pos];
            switch (e) {
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            case '/':
                out += '/';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'u': {
                if (pos + 4 >= s.size()) return false;
                unsigned code = 0;
                for (int k = 0; k < 4; ++k) {
                    char h   = s[++pos];
                    code <<= 4;
                    if (h >= '0' && h <= '9') code += static_cast<unsigned>(h - '0');
                    else if (h >= 'a' && h <= 'f') code += static_cast<unsigned>(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') code += static_cast<unsigned>(h - 'A' + 10);
                    else return false;
                }
                // Basic UTF-8 encoding (BMP only; sufficient for block names)
                if (code < 0x80) {
                    out += static_cast<char>(code);
                } else if (code < 0x800) {
                    out += static_cast<char>(0xC0 | (code >> 6));
                    out += static_cast<char>(0x80 | (code & 0x3F));
                } else {
                    out += static_cast<char>(0xE0 | (code >> 12));
                    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (code & 0x3F));
                }
                break;
            }
            default:
                return false;
            }
            ++pos;
        } else {
            out += c;
            ++pos;
        }
    }
    return false;
}

bool readJsonIntArray(std::string const& s, std::size_t& pos, std::vector<long long>& out) {
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != '[') return false;
    ++pos;
    out.clear();
    pos = skipWs(s, pos);
    if (pos < s.size() && s[pos] == ']') {
        ++pos;
        return true;
    }
    while (pos < s.size()) {
        pos = skipWs(s, pos);
        if (pos >= s.size()) return false;
        bool neg = false;
        if (s[pos] == '-') {
            neg = true;
            ++pos;
        }
        if (pos >= s.size() || s[pos] < '0' || s[pos] > '9') return false;
        long long v = 0;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
            v = v * 10 + (s[pos] - '0');
            ++pos;
        }
        out.push_back(neg ? -v : v);
        pos = skipWs(s, pos);
        if (pos < s.size() && s[pos] == ',') {
            ++pos;
            continue;
        }
        if (pos < s.size() && s[pos] == ']') {
            ++pos;
            return true;
        }
        return false;
    }
    return false;
}

bool readJsonStringArray(std::string const& s, std::size_t& pos, std::vector<std::string>& out) {
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != '[') return false;
    ++pos;
    out.clear();
    pos = skipWs(s, pos);
    if (pos < s.size() && s[pos] == ']') {
        ++pos;
        return true;
    }
    while (pos < s.size()) {
        std::string v;
        if (!readJsonString(s, pos, v)) return false;
        out.push_back(std::move(v));
        pos = skipWs(s, pos);
        if (pos < s.size() && s[pos] == ',') {
            ++pos;
            continue;
        }
        if (pos < s.size() && s[pos] == ']') {
            ++pos;
            return true;
        }
        return false;
    }
    return false;
}

} // namespace

std::string VoxelModel::toJson() const {
    std::string out;
    out.reserve(vox.size() / 4 + 128);
    out += "{\"format\":\"levi-create-model\",\"version\":1";
    out += ",\"size\":[" + std::to_string(sx) + "," + std::to_string(sy) + "," + std::to_string(sz) + "]";
    out += ",\"palette\":[";
    for (std::size_t i = 0; i < palette.size(); ++i) {
        if (i) out += ',';
        out += '"';
        out += escapeJson(palette[i]);
        out += '"';
    }
    out += "],\"data\":[";
    // Run-length encode: pairs of (count, paletteIndex+1)
    bool        first = true;
    VoxelIndex  cur   = 0;
    std::size_t run   = 0;
    auto        flush = [&](VoxelIndex v, std::size_t n) {
        if (!first) out += ',';
        first = false;
        out += std::to_string(n);
        out += ',';
        out += std::to_string(v);
    };
    for (VoxelIndex v : vox) {
        if (run == 0 || v == cur) {
            cur = v;
            ++run;
        } else {
            flush(cur, run);
            run = 1;
            cur = v;
        }
    }
    if (run) flush(cur, run);
    out += "]}";
    return out;
}

std::optional<VoxelModel> VoxelModel::fromJson(std::string const& text) {
    std::size_t pos = 0;
    long long   sx = 0, sy = 0, sz = 0;

    std::vector<long long> sizeArr;
    if (!findValueAfterKey(text, "size", pos) || !readJsonIntArray(text, pos, sizeArr) || sizeArr.size() != 3) {
        return std::nullopt;
    }
    sx = sizeArr[0];
    sy = sizeArr[1];
    sz = sizeArr[2];
    if (sx <= 0 || sy <= 0 || sz <= 0) return std::nullopt;
    if (static_cast<std::uint64_t>(sx) * static_cast<std::uint64_t>(sy) * static_cast<std::uint64_t>(sz) > 2000000ULL) {
        return std::nullopt;
    }

    VoxelModel m;
    m.sx = static_cast<int>(sx);
    m.sy = static_cast<int>(sy);
    m.sz = static_cast<int>(sz);

    pos = 0;
    if (!findValueAfterKey(text, "palette", pos) || !readJsonStringArray(text, pos, m.palette)) {
        return std::nullopt;
    }

    std::vector<long long> data;
    pos = 0;
    if (!findValueAfterKey(text, "data", pos) || !readJsonIntArray(text, pos, data) || data.size() % 2 != 0) {
        return std::nullopt;
    }

    m.vox.assign(m.volume(), VoxelIndex{0});
    std::size_t idx = 0;
    for (std::size_t i = 0; i + 1 < data.size(); i += 2) {
        long long count = data[i];
        long long val   = data[i + 1];
        if (count < 0 || val < 0 || val > 65535) return std::nullopt;
        if (val != 0 && static_cast<std::size_t>(val) > m.palette.size()) return std::nullopt;
        for (long long k = 0; k < count && idx < m.vox.size(); ++k) {
            m.vox[idx++] = static_cast<VoxelIndex>(val);
        }
    }
    return m;
}

VoxelModel scaleModel(VoxelModel const& src, float factor) {
    if (src.empty()) return src;
    if (!std::isfinite(factor)) return src;
    if (factor < 1.0f / 16.0f) factor = 1.0f / 16.0f;
    if (factor > 16.0f) factor = 16.0f;
    if (std::fabs(factor - 1.0f) < 1e-4f) return src;

    int ox = std::max(1, static_cast<int>(std::llround(src.sx * factor)));
    int oy = std::max(1, static_cast<int>(std::llround(src.sy * factor)));
    int oz = std::max(1, static_cast<int>(std::llround(src.sz * factor)));
    if (static_cast<std::uint64_t>(ox) * oy * oz > 2000000ULL) {
        // Too large, clamp the scale down so it fits.
        double over = static_cast<double>(ox) * oy * oz / 2000000.0;
        double fix  = std::pow(over, 1.0 / 3.0);
        ox          = std::max(1, static_cast<int>(ox / fix));
        oy          = std::max(1, static_cast<int>(oy / fix));
        oz          = std::max(1, static_cast<int>(oz / fix));
    }

    VoxelModel out;
    out.palette = src.palette;
    out.resize(ox, oy, oz);
    // Map each output voxel center to a source voxel by relative position.
    // This handles both upscale and extreme downscale (1/16 minis) consistently.
    auto sampleAxis = [](int out, int outDim, int srcDim) -> int {
        if (outDim <= 0 || srcDim <= 0) return 0;
        double v = (static_cast<double>(out) + 0.5) * static_cast<double>(srcDim) / static_cast<double>(outDim);
        int    i = static_cast<int>(std::floor(v));
        if (i < 0) i = 0;
        if (i >= srcDim) i = srcDim - 1;
        return i;
    };
    for (int z = 0; z < oz; ++z) {
        int szv = sampleAxis(z, oz, src.sz);
        for (int y = 0; y < oy; ++y) {
            int syv = sampleAxis(y, oy, src.sy);
            for (int x = 0; x < ox; ++x) {
                int sxv = sampleAxis(x, ox, src.sx);
                out.set(x, y, z, src.get(sxv, syv, szv));
            }
        }
    }
    return out;
}

namespace {

struct Mat3 {
    double m[3][3]{};

    static Mat3 identity() {
        Mat3 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = 1.0;
        return r;
    }
    // Applies the matrix to a vector.
    void apply(double& x, double& y, double& z) const {
        double nx = m[0][0] * x + m[0][1] * y + m[0][2] * z;
        double ny = m[1][0] * x + m[1][1] * y + m[1][2] * z;
        double nz = m[2][0] * x + m[2][1] * y + m[2][2] * z;
        x = nx;
        y = ny;
        z = nz;
    }
    Mat3 transposed() const {
        Mat3 r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) r.m[i][j] = m[j][i];
        return r;
    }
};

Mat3 rotationMatrix(int axis, double radians) {
    Mat3    r = Mat3::identity();
    double  c = std::cos(radians);
    double  s = std::sin(radians);
    if (axis == 0) { // X: y' = c*y - s*z, z' = s*y + c*z
        r.m[1][1] = c; r.m[1][2] = -s;
        r.m[2][1] = s; r.m[2][2] = c;
    } else if (axis == 1) { // Y: x' = c*x + s*z, z' = -s*x + c*z
        r.m[0][0] = c;  r.m[0][2] = s;
        r.m[2][0] = -s; r.m[2][2] = c;
    } else { // Z: x' = c*x - s*y, y' = s*x + c*y
        r.m[0][0] = c;  r.m[0][1] = -s;
        r.m[1][0] = s;  r.m[1][1] = c;
    }
    return r;
}

bool isNearMultipleOf90(double degrees) {
    double m = std::fmod(degrees, 90.0);
    if (m < 0) m += 90.0;
    return m < 0.01 || m > 89.99;
}

// Exact voxel-exact rotation for multiples of 90 degrees.
VoxelModel rotateExact(VoxelModel const& src, int axis, int quarterTurns) {
    quarterTurns = ((quarterTurns % 4) + 4) % 4;
    if (quarterTurns == 0) return src;
    VoxelModel out;
    out.palette = src.palette;
    int sx = src.sx, sy = src.sy, sz = src.sz;
    int ox, oy, oz;
    if (axis == 1) { // Y
        ox = (quarterTurns == 2) ? sx : sz;
        oz = (quarterTurns == 2) ? sz : sx;
        oy = sy;
    } else if (axis == 0) { // X
        oy = (quarterTurns == 2) ? sy : sz;
        oz = (quarterTurns == 2) ? sz : sy;
        ox = sx;
    } else { // Z
        ox = (quarterTurns == 2) ? sx : sy;
        oy = (quarterTurns == 2) ? sy : sx;
        oz = sz;
    }
    out.resize(ox, oy, oz);
    for (int z = 0; z < sz; ++z) {
        for (int y = 0; y < sy; ++y) {
            for (int x = 0; x < sx; ++x) {
                int nx, ny, nz;
                if (axis == 1) { // Y rotation (x,z plane)
                    if (quarterTurns == 1) { nx = z; nz = sx - 1 - x; }
                    else if (quarterTurns == 2) { nx = sx - 1 - x; nz = sz - 1 - z; }
                    else { nx = sz - 1 - z; nz = x; }
                    ny = y;
                } else if (axis == 0) { // X rotation (y,z plane)
                    if (quarterTurns == 1) { ny = z; nz = sy - 1 - y; }
                    else if (quarterTurns == 2) { ny = sy - 1 - y; nz = sz - 1 - z; }
                    else { ny = sz - 1 - z; nz = y; }
                    nx = x;
                } else { // Z rotation (x,y plane)
                    if (quarterTurns == 1) { nx = sy - 1 - y; ny = x; }
                    else if (quarterTurns == 2) { nx = sx - 1 - x; ny = sy - 1 - y; }
                    else { nx = y; ny = sx - 1 - x; }
                    nz = z;
                }
                out.set(nx, ny, nz, src.get(x, y, z));
            }
        }
    }
    return out;
}

VoxelModel rotateGeneric(VoxelModel const& src, int axis, double degrees) {
    if (src.empty()) return src;
    Mat3  r = rotationMatrix(axis, degrees * 3.14159265358979323846 / 180.0);
    Mat3  rInv = r.transposed(); // rotation matrices are orthonormal

    double csx = src.sx / 2.0, csy = src.sy / 2.0, csz = src.sz / 2.0;

    // Transform the 8 corners to get the output bounding box.
    double minX = 1e18, minY = 1e18, minZ = 1e18;
    double maxX = -1e18, maxY = -1e18, maxZ = -1e18;
    for (int i = 0; i < 8; ++i) {
        double px = (i & 1) ? src.sx : 0.0;
        double py = (i & 2) ? src.sy : 0.0;
        double pz = (i & 4) ? src.sz : 0.0;
        double qx = px - csx, qy = py - csy, qz = pz - csz;
        r.apply(qx, qy, qz);
        minX = std::min(minX, qx); minY = std::min(minY, qy); minZ = std::min(minZ, qz);
        maxX = std::max(maxX, qx); maxY = std::max(maxY, qy); maxZ = std::max(maxZ, qz);
    }
    int ox = std::max(1, static_cast<int>(std::ceil(maxX - minX - 1e-9)));
    int oy = std::max(1, static_cast<int>(std::ceil(maxY - minY - 1e-9)));
    int oz = std::max(1, static_cast<int>(std::ceil(maxZ - minZ - 1e-9)));
    if (static_cast<std::uint64_t>(ox) * oy * oz > 2000000ULL) {
        VoxelModel empty;
        return empty;
    }

    VoxelModel out;
    out.palette = src.palette;
    out.resize(ox, oy, oz);
    double cox = ox / 2.0, coy = oy / 2.0, coz = oz / 2.0;

    for (int z = 0; z < oz; ++z) {
        for (int y = 0; y < oy; ++y) {
            for (int x = 0; x < ox; ++x) {
                double px = (x + 0.5) - cox;
                double py = (y + 0.5) - coy;
                double pz = (z + 0.5) - coz;
                rInv.apply(px, py, pz);
                px += csx;
                py += csy;
                pz += csz;
                int sxv = static_cast<int>(std::floor(px));
                int syv = static_cast<int>(std::floor(py));
                int szv = static_cast<int>(std::floor(pz));
                out.set(x, y, z, src.get(sxv, syv, szv));
            }
        }
    }
    return out;
}

} // namespace

VoxelModel rotateModel(VoxelModel const& src, int axis, double degrees) {
    if (src.empty() || degrees == 0.0) return src;
    if (isNearMultipleOf90(degrees)) {
        int q = static_cast<int>(std::llround(degrees / 90.0));
        return rotateExact(src, axis, q);
    }
    return rotateGeneric(src, axis, degrees);
}

VoxelModel transformModel(VoxelModel const& src, ModelTransform const& t) {
    VoxelModel m = scaleModel(src, t.scale);
    m = rotateModel(m, 1, static_cast<double>(t.yawDegrees));
    m = rotateModel(m, 0, static_cast<double>(t.pitchDegrees));
    m = rotateModel(m, 2, static_cast<double>(t.rollDegrees));
    return m;
}

VoxelModel flipModel(VoxelModel const& src, int axis) {
    if (src.empty()) return src;
    VoxelModel out;
    out.palette = src.palette;
    out.resize(src.sx, src.sy, src.sz);
    for (int z = 0; z < src.sz; ++z) {
        for (int y = 0; y < src.sy; ++y) {
            for (int x = 0; x < src.sx; ++x) {
                int nx = x, ny = y, nz = z;
                if (axis == 0) nx = src.sx - 1 - x;
                else if (axis == 1) ny = src.sy - 1 - y;
                else nz = src.sz - 1 - z;
                out.set(nx, ny, nz, src.get(x, y, z));
            }
        }
    }
    return out;
}

std::string formatVolume(std::uint64_t blocks) {
    if (blocks >= 1000000) {
        double m = static_cast<double>(blocks) / 1000000.0;
        char   buf[32];
        std::snprintf(buf, sizeof(buf), "%.2fM", m);
        return buf;
    }
    if (blocks >= 1000) {
        double k = static_cast<double>(blocks) / 1000.0;
        char   buf[32];
        std::snprintf(buf, sizeof(buf), "%.1fK", k);
        return buf;
    }
    return std::to_string(blocks);
}

} // namespace lc
