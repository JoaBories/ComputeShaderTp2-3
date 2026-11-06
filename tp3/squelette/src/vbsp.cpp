#include "vbsp.hpp"

#include <lzma.h>
#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <cmath>

namespace {

bool decompressLzma(const unsigned char* blob, uint32_t blobLen, std::vector<unsigned char>& out) {
  if (blobLen < 17 || std::memcmp(blob, "LZMA", 4) != 0) {
    out.assign(blob, blob + blobLen);
    return true;
  }
  uint32_t actual = 0;
  uint32_t lzsz = 0;
  std::memcpy(&actual, blob + 4, 4);
  std::memcpy(&lzsz, blob + 8, 4);
  if (17u + lzsz > blobLen || actual == 0) return false;
  const unsigned char props = blob[12];
  const int lc = props % 9;
  const int rest = props / 9;
  const int lp = rest % 5;
  const int pb = rest / 5;
  uint32_t dict = 0;
  std::memcpy(&dict, blob + 13, 4);
  lzma_options_lzma opt;
  std::memset(&opt, 0, sizeof(opt));
  opt.lc = static_cast<uint32_t>(lc);
  opt.lp = static_cast<uint32_t>(lp);
  opt.pb = static_cast<uint32_t>(pb);
  opt.dict_size = dict;
  lzma_filter filters[2];
  filters[0].id = LZMA_FILTER_LZMA1;
  filters[0].options = &opt;
  filters[1].id = LZMA_VLI_UNKNOWN;
  filters[1].options = 0;
  lzma_stream strm = LZMA_STREAM_INIT;
  if (lzma_raw_decoder(&strm, filters) != LZMA_OK) return false;
  out.resize(actual);
  strm.next_in = blob + 17;
  strm.avail_in = lzsz;
  strm.next_out = out.data();
  strm.avail_out = actual;
  const lzma_ret rc = lzma_code(&strm, LZMA_FINISH);
  lzma_end(&strm);
  return rc == LZMA_OK || rc == LZMA_STREAM_END;
}

template <typename T>
T readAt(const std::vector<unsigned char>& b, size_t off) {
  T v;
  std::memcpy(&v, b.data() + off, sizeof(T));
  return v;
}

void yup(float x, float y, float z, float& ox, float& oy, float& oz) {
  ox = x;
  oy = z;
  oz = -y;
}

}

bool readSourceBspBuffer(const std::vector<unsigned char>& file, std::vector<float>& positions, std::vector<float>& normals, std::string& error) {
  if (file.size() < 8 + 64 * 16 || std::memcmp(file.data(), "VBSP", 4) != 0) {
    error = "pas un BSP Source (VBSP)";
    return false;
  }
  struct Lump { uint32_t ofs, len; };
  Lump lumps[64];
  for (int i = 0; i < 64; ++i) {
    const size_t o = 8 + static_cast<size_t>(i) * 16;
    lumps[i].ofs = readAt<uint32_t>(file, o);
    lumps[i].len = readAt<uint32_t>(file, o + 4);
    if (static_cast<size_t>(lumps[i].ofs) + lumps[i].len > file.size()) {
      error = "lump hors fichier";
      return false;
    }
  }
  std::vector<unsigned char> raw[64];
  const int need[] = {1, 3, 6, 7, 12, 13, 14, 26, 33};
  for (size_t n = 0; n < sizeof(need) / sizeof(need[0]); ++n) {
    const int i = need[n];
    const unsigned char* p = file.data() + lumps[i].ofs;
    if (!decompressLzma(p, lumps[i].len, raw[i])) {
      error = "decompression LZMA echouee";
      return false;
    }
  }
  const std::vector<unsigned char>& verts = raw[3];
  const std::vector<unsigned char>& edges = raw[12];
  const std::vector<unsigned char>& surf = raw[13];
  const std::vector<unsigned char>& faces = raw[7];
  const std::vector<unsigned char>& planes = raw[1];
  const std::vector<unsigned char>& tex = raw[6];
  const std::vector<unsigned char>& models = raw[14];
  const std::vector<unsigned char>& disp = raw[26];
  const std::vector<unsigned char>& dverts = raw[33];
  if (models.size() < 48 || faces.size() < 56) {
    error = "bsp sans geometrie";
    return false;
  }
  const int numFacesWorld = readAt<int>(models, 44);
  const int faceBytes = 56;
  const int nFaces = std::min(numFacesWorld, static_cast<int>(faces.size() / faceBytes));
  const int nEdges = static_cast<int>(edges.size() / 4);
  const int nSurf = static_cast<int>(surf.size() / 4);
  const int nVerts = static_cast<int>(verts.size() / 12);
  const unsigned skipFlags = 0x4u | 0x80u | 0x100u | 0x200u;

  positions.clear();
  normals.clear();

  for (int fi = 0; fi < nFaces; ++fi) {
    const size_t base = static_cast<size_t>(fi) * faceBytes;
    const uint16_t planenum = readAt<uint16_t>(faces, base);
    const uint8_t side = faces[base + 2];
    const int firstedge = readAt<int>(faces, base + 4);
    const short numedges = readAt<short>(faces, base + 8);
    const short texinfo = readAt<short>(faces, base + 10);
    const short dispinfo = readAt<short>(faces, base + 12);
    if (numedges < 3 || texinfo < 0) continue;
    if (static_cast<size_t>(texinfo) * 72 + 68 < tex.size()) {
      const int flags = readAt<int>(tex, static_cast<size_t>(texinfo) * 72 + 64);
      if (flags & static_cast<int>(skipFlags)) continue;
    }
    if (firstedge < 0 || firstedge + numedges > nSurf) continue;
    std::vector<int> idxs;
    idxs.reserve(static_cast<size_t>(numedges));
    bool bad = false;
    for (int k = 0; k < numedges; ++k) {
      const int se = readAt<int>(surf, static_cast<size_t>(firstedge + k) * 4);
      const int e = se >= 0 ? se : -se;
      if (e < 0 || e >= nEdges) {
        bad = true;
        break;
      }
      const int vi = se >= 0 ? readAt<uint16_t>(edges, static_cast<size_t>(e) * 4)
                             : readAt<uint16_t>(edges, static_cast<size_t>(e) * 4 + 2);
      if (vi < 0 || vi >= nVerts) {
        bad = true;
        break;
      }
      idxs.push_back(vi);
    }
    if (bad || idxs.size() < 3) continue;
    float nx, ny, nz;
    if (static_cast<size_t>(planenum) * 16 + 12 <= planes.size()) {
      float px = readAt<float>(planes, static_cast<size_t>(planenum) * 16);
      float py = readAt<float>(planes, static_cast<size_t>(planenum) * 16 + 4);
      float pz = readAt<float>(planes, static_cast<size_t>(planenum) * 16 + 8);
      if (side) {
        px = -px;
        py = -py;
        pz = -pz;
      }
      yup(px, py, pz, nx, ny, nz);
    } else {
      nx = 0.f;
      ny = 1.f;
      nz = 0.f;
    }
    auto vert = [&](int vi, float& x, float& y, float& z) {
      yup(readAt<float>(verts, static_cast<size_t>(vi) * 12),
          readAt<float>(verts, static_cast<size_t>(vi) * 12 + 4),
          readAt<float>(verts, static_cast<size_t>(vi) * 12 + 8), x, y, z);
    };
    auto pushTri = [&](float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz, float nnx, float nny, float nnz) {
      const float tri[9] = {ax, ay, az, bx, by, bz, cx, cy, cz};
      const float nrm[3] = {nnx, nny, nnz};
      for (int t = 0; t < 3; ++t) {
        positions.push_back(tri[t * 3]);
        positions.push_back(tri[t * 3 + 1]);
        positions.push_back(tri[t * 3 + 2]);
        normals.push_back(nrm[0]);
        normals.push_back(nrm[1]);
        normals.push_back(nrm[2]);
      }
    };
    if (dispinfo >= 0 && disp.size() >= static_cast<size_t>(dispinfo + 1) * 176 && dverts.size() >= 20) {
      const size_t d = static_cast<size_t>(dispinfo) * 176;
      const float sx = readAt<float>(disp, d);
      const float sy = readAt<float>(disp, d + 4);
      const float sz = readAt<float>(disp, d + 8);
      const int vstart = readAt<int>(disp, d + 12);
      const int power = readAt<int>(disp, d + 20);
      if (power < 1 || power > 4 || idxs.size() < 4) {
        // fall through to fan
      } else {
        const int size = (1 << power) + 1;
        if ((vstart + size * size) * 5 <= static_cast<int>(dverts.size() / 4)) {
          float c[4][3];
          for (int i = 0; i < 4; ++i) vert(idxs[static_cast<size_t>(i)], c[i][0], c[i][1], c[i][2]);
          float csx, csy, csz;
          yup(sx, sy, sz, csx, csy, csz);
          int nearest = 0;
          float best = 1e30f;
          for (int i = 0; i < 4; ++i) {
            const float dx = c[i][0] - csx, dy = c[i][1] - csy, dz = c[i][2] - csz;
            const float dd = dx * dx + dy * dy + dz * dz;
            if (dd < best) {
              best = dd;
              nearest = i;
            }
          }
          float rc[4][3];
          for (int i = 0; i < 4; ++i) {
            const int s = (nearest + i) % 4;
            rc[i][0] = c[s][0];
            rc[i][1] = c[s][1];
            rc[i][2] = c[s][2];
          }
          std::vector<float> grid(static_cast<size_t>(size * size * 3));
          for (int y = 0; y < size; ++y) {
            const float ty = static_cast<float>(y) / static_cast<float>(size - 1);
            for (int x = 0; x < size; ++x) {
              const float tx = static_cast<float>(x) / static_cast<float>(size - 1);
              float p[3];
              for (int a = 0; a < 3; ++a) {
                p[a] = (1.f - ty) * ((1.f - tx) * rc[0][a] + tx * rc[1][a]) + ty * ((1.f - tx) * rc[3][a] + tx * rc[2][a]);
              }
              const size_t dv = static_cast<size_t>(vstart + y * size + x) * 20;
              float vx = readAt<float>(dverts, dv);
              float vy = readAt<float>(dverts, dv + 4);
              float vz = readAt<float>(dverts, dv + 8);
              const float dist = readAt<float>(dverts, dv + 12);
              float ox, oy, oz;
              yup(vx * dist, vy * dist, vz * dist, ox, oy, oz);
              const size_t g = static_cast<size_t>(y * size + x) * 3;
              grid[g] = p[0] + ox;
              grid[g + 1] = p[1] + oy;
              grid[g + 2] = p[2] + oz;
            }
          }
          for (int y = 0; y < size - 1; ++y) {
            for (int x = 0; x < size - 1; ++x) {
              const float* p00 = &grid[static_cast<size_t>(y * size + x) * 3];
              const float* p10 = &grid[static_cast<size_t>(y * size + x + 1) * 3];
              const float* p01 = &grid[static_cast<size_t>((y + 1) * size + x) * 3];
              const float* p11 = &grid[static_cast<size_t>((y + 1) * size + x + 1) * 3];
              pushTri(p00[0], p00[1], p00[2], p10[0], p10[1], p10[2], p11[0], p11[1], p11[2], nx, ny, nz);
              pushTri(p00[0], p00[1], p00[2], p11[0], p11[1], p11[2], p01[0], p01[1], p01[2], nx, ny, nz);
            }
          }
          continue;
        }
      }
    }
    float ax, ay, az;
    vert(idxs[0], ax, ay, az);
    for (size_t t = 1; t + 1 < idxs.size(); ++t) {
      float bx, by, bz, cx, cy, cz;
      vert(idxs[t], bx, by, bz);
      vert(idxs[t + 1], cx, cy, cz);
      pushTri(ax, ay, az, bx, by, bz, cx, cy, cz, nx, ny, nz);
    }
  }
  if (positions.empty()) {
    error = "aucun triangle";
    return false;
  }
  return true;
}

bool readSourceBsp(const std::string& path, std::vector<float>& positions, std::vector<float>& normals, std::string& error) {
  std::ifstream in(path.c_str(), std::ios::binary);
  if (!in) {
    error = "lecture impossible";
    return false;
  }
  std::vector<unsigned char> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return readSourceBspBuffer(file, positions, normals, error);
}

namespace {

bool inflateZip(const unsigned char* src, size_t srcLen, std::vector<unsigned char>& dst) {
  z_stream strm;
  std::memset(&strm, 0, sizeof(strm));
  if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) return false;
  strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
  strm.avail_in = static_cast<uInt>(srcLen);
  strm.next_out = dst.data();
  strm.avail_out = static_cast<uInt>(dst.size());
  const int rc = inflate(&strm, Z_FINISH);
  inflateEnd(&strm);
  return rc == Z_STREAM_END;
}

}

bool extractBspFromPk3(const std::string& path, std::vector<unsigned char>& bsp, std::string& error) {
  std::ifstream in(path.c_str(), std::ios::binary);
  if (!in) {
    error = "lecture impossible";
    return false;
  }
  std::vector<unsigned char> zip((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (zip.size() < 22 || zip[0] != 'P' || zip[1] != 'K') {
    error = "pas un pk3";
    return false;
  }
  size_t eocd = zip.size();
  const size_t scan = zip.size() > 66000 ? zip.size() - 66000 : 0;
  for (size_t i = zip.size() - 22; i >= scan; --i) {
    if (zip[i] == 0x50 && zip[i + 1] == 0x4b && zip[i + 2] == 0x05 && zip[i + 3] == 0x06) {
      eocd = i;
      break;
    }
    if (i == 0) break;
  }
  if (eocd == zip.size()) {
    error = "repertoire zip introuvable";
    return false;
  }
  const uint16_t count = readAt<uint16_t>(zip, eocd + 10);
  size_t cursor = readAt<uint32_t>(zip, eocd + 16);
  std::string bestName;
  uint16_t bestMethod = 0;
  uint32_t bestComp = 0;
  uint32_t bestUncomp = 0;
  uint32_t bestLocal = 0;
  for (uint16_t n = 0; n < count; ++n) {
    if (cursor + 46 > zip.size() || std::memcmp(zip.data() + cursor, "PK\x01\x02", 4) != 0) {
      error = "entree zip invalide";
      return false;
    }
    const uint16_t method = readAt<uint16_t>(zip, cursor + 10);
    const uint32_t comp = readAt<uint32_t>(zip, cursor + 20);
    const uint32_t uncomp = readAt<uint32_t>(zip, cursor + 24);
    const uint16_t nameLen = readAt<uint16_t>(zip, cursor + 28);
    const uint16_t extraLen = readAt<uint16_t>(zip, cursor + 30);
    const uint16_t commentLen = readAt<uint16_t>(zip, cursor + 32);
    const uint32_t local = readAt<uint32_t>(zip, cursor + 42);
    if (cursor + 46u + nameLen + extraLen + commentLen > zip.size()) {
      error = "entree zip tronquee";
      return false;
    }
    std::string name(reinterpret_cast<const char*>(zip.data() + cursor + 46), nameLen);
    cursor += 46u + nameLen + extraLen + commentLen;
    if (name.size() < 4) continue;
    std::string lower = name;
    for (size_t c = 0; c < lower.size(); ++c) {
      if (lower[c] >= 'A' && lower[c] <= 'Z') lower[c] = static_cast<char>(lower[c] - 'A' + 'a');
    }
    if (lower.size() < 4 || lower.compare(lower.size() - 4, 4, ".bsp") != 0) continue;
    const bool inMaps = lower.find("maps/") != std::string::npos;
    const bool bestInMaps = bestName.find("maps/") != std::string::npos;
    if (bestName.empty() || (inMaps && !bestInMaps) || (inMaps == bestInMaps && uncomp > bestUncomp)) {
      bestName = lower;
      bestMethod = method;
      bestComp = comp;
      bestUncomp = uncomp;
      bestLocal = local;
    }
  }
  if (bestName.empty()) {
    error = "aucun .bsp dans le pk3";
    return false;
  }
  if (static_cast<size_t>(bestLocal) + 30 > zip.size() || std::memcmp(zip.data() + bestLocal, "PK\x03\x04", 4) != 0) {
    error = "bsp introuvable dans le pk3";
    return false;
  }
  const uint16_t nameLen = readAt<uint16_t>(zip, bestLocal + 26);
  const uint16_t extraLen = readAt<uint16_t>(zip, bestLocal + 28);
  const size_t data = static_cast<size_t>(bestLocal) + 30u + nameLen + extraLen;
  if (data + bestComp > zip.size()) {
    error = "bsp tronque dans le pk3";
    return false;
  }
  bsp.resize(bestUncomp);
  if (bestMethod == 0) {
    if (bestComp != bestUncomp) {
      error = "bsp stocke de taille incoherente";
      return false;
    }
    std::memcpy(bsp.data(), zip.data() + data, bestUncomp);
  } else if (bestMethod == 8) {
    if (!inflateZip(zip.data() + data, bestComp, bsp)) {
      error = "decompression du bsp echouee";
      return false;
    }
  } else {
    error = "compression zip non supportee";
    return false;
  }
  return true;
}
#include <csetjmp>
#include <jpeglib.h>

namespace {

std::string lowerCopy(const std::string& s) {
  std::string o = s;
  for (size_t i = 0; i < o.size(); ++i) {
    if (o[i] >= 'A' && o[i] <= 'Z') o[i] = static_cast<char>(o[i] - 'A' + 'a');
  }
  return o;
}

bool zipEntry(const std::vector<unsigned char>& zip, const std::string& want, std::vector<unsigned char>& out) {
  if (zip.size() < 22) return false;
  size_t eocd = zip.size();
  const size_t scan = zip.size() > 66000 ? zip.size() - 66000 : 0;
  for (size_t i = zip.size() - 22;; --i) {
    if (zip[i] == 0x50 && zip[i + 1] == 0x4b && zip[i + 2] == 0x05 && zip[i + 3] == 0x06) {
      eocd = i;
      break;
    }
    if (i == scan) return false;
  }
  const uint16_t count = readAt<uint16_t>(zip, eocd + 10);
  size_t cursor = readAt<uint32_t>(zip, eocd + 16);
  for (uint16_t n = 0; n < count; ++n) {
    if (cursor + 46 > zip.size()) return false;
    const uint16_t method = readAt<uint16_t>(zip, cursor + 10);
    const uint32_t comp = readAt<uint32_t>(zip, cursor + 20);
    const uint32_t uncomp = readAt<uint32_t>(zip, cursor + 24);
    const uint16_t nameLen = readAt<uint16_t>(zip, cursor + 28);
    const uint16_t extraLen = readAt<uint16_t>(zip, cursor + 30);
    const uint16_t commentLen = readAt<uint16_t>(zip, cursor + 32);
    const uint32_t local = readAt<uint32_t>(zip, cursor + 42);
    if (cursor + 46u + nameLen + extraLen + commentLen > zip.size()) return false;
    std::string name(reinterpret_cast<const char*>(zip.data() + cursor + 46), nameLen);
    cursor += 46u + nameLen + extraLen + commentLen;
    if (lowerCopy(name) != want) continue;
    if (static_cast<size_t>(local) + 30 > zip.size()) return false;
    const uint16_t ln = readAt<uint16_t>(zip, local + 26);
    const uint16_t en = readAt<uint16_t>(zip, local + 28);
    const size_t data = static_cast<size_t>(local) + 30u + ln + en;
    if (data + comp > zip.size() || uncomp > 32u * 1024u * 1024u) return false;
    out.resize(uncomp);
    if (method == 0) {
      std::memcpy(out.data(), zip.data() + data, uncomp);
      return true;
    }
    if (method == 8) return inflateZip(zip.data() + data, comp, out);
    return false;
  }
  return false;
}

void bezier(const float* a, const float* b, const float* c, float t, int n, float* o) {
  const float it = 1.f - t;
  const float w0 = it * it;
  const float w1 = 2.f * it * t;
  const float w2 = t * t;
  for (int i = 0; i < n; ++i) o[i] = w0 * a[i] + w1 * b[i] + w2 * c[i];
}

void bezier2(const float cp[3][3][5], float u, float v, float* o) {
  float row[3][5];
  for (int y = 0; y < 3; ++y) bezier(cp[y][0], cp[y][1], cp[y][2], u, 5, row[y]);
  bezier(row[0], row[1], row[2], v, 5, o);
}

struct JpegErr {
  jpeg_error_mgr pub;
  jmp_buf jump;
};

void jpegExit(j_common_ptr cinfo) {
  longjmp(reinterpret_cast<JpegErr*>(cinfo->err)->jump, 1);
}

bool decodeJpeg(const std::vector<unsigned char>& src, std::vector<unsigned char>& rgba, int& w, int& h) {
  jpeg_decompress_struct cinfo;
  JpegErr jerr;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpegExit;
  if (setjmp(jerr.jump)) {
    jpeg_destroy_decompress(&cinfo);
    return false;
  }
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, const_cast<unsigned char*>(src.data()), static_cast<unsigned long>(src.size()));
  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
    jpeg_destroy_decompress(&cinfo);
    return false;
  }
  cinfo.out_color_space = JCS_RGB;
  jpeg_start_decompress(&cinfo);
  w = static_cast<int>(cinfo.output_width);
  h = static_cast<int>(cinfo.output_height);
  rgba.assign(static_cast<size_t>(w * h * 4), 255);
  std::vector<unsigned char> row(static_cast<size_t>(w * 3));
  while (cinfo.output_scanline < cinfo.output_height) {
    unsigned char* p = row.data();
    jpeg_read_scanlines(&cinfo, &p, 1);
    const int y = static_cast<int>(cinfo.output_scanline) - 1;
    for (int x = 0; x < w; ++x) {
      const size_t d = static_cast<size_t>(y * w + x) * 4;
      rgba[d] = row[static_cast<size_t>(x) * 3];
      rgba[d + 1] = row[static_cast<size_t>(x) * 3 + 1];
      rgba[d + 2] = row[static_cast<size_t>(x) * 3 + 2];
    }
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  return true;
}

bool decodeTga(const std::vector<unsigned char>& src, std::vector<unsigned char>& rgba, int& w, int& h) {
  if (src.size() < 18) return false;
  const int idLen = src[0];
  const int type = src[2];
  w = src[12] | (src[13] << 8);
  h = src[14] | (src[15] << 8);
  const int bpp = src[16];
  if (w <= 0 || h <= 0 || (bpp != 24 && bpp != 32)) return false;
  size_t i = 18u + static_cast<size_t>(idLen);
  const int pixels = w * h;
  const int bytes = bpp / 8;
  rgba.assign(static_cast<size_t>(pixels * 4), 255);
  int written = 0;
  if (type == 2) {
    if (i + static_cast<size_t>(pixels) * bytes > src.size()) return false;
    for (int p = 0; p < pixels; ++p) {
      rgba[static_cast<size_t>(p) * 4] = src[i + 2];
      rgba[static_cast<size_t>(p) * 4 + 1] = src[i + 1];
      rgba[static_cast<size_t>(p) * 4 + 2] = src[i];
      if (bytes == 4) rgba[static_cast<size_t>(p) * 4 + 3] = src[i + 3];
      i += static_cast<size_t>(bytes);
    }
    return true;
  }
  if (type != 10) return false;
  while (written < pixels && i < src.size()) {
    const int packet = src[i++];
    const int count = (packet & 127) + 1;
    if (packet & 128) {
      if (i + static_cast<size_t>(bytes) > src.size()) return false;
      unsigned char px[4] = {src[i], src[i + 1], src[i + 2], 255};
      if (bytes == 4) px[3] = src[i + 3];
      i += static_cast<size_t>(bytes);
      for (int k = 0; k < count && written < pixels; ++k, ++written) {
        rgba[static_cast<size_t>(written) * 4 + 0] = px[2];
        rgba[static_cast<size_t>(written) * 4 + 1] = px[1];
        rgba[static_cast<size_t>(written) * 4 + 2] = px[0];
        rgba[static_cast<size_t>(written) * 4 + 3] = px[3];
      }
    } else {
      for (int k = 0; k < count && written < pixels; ++k, ++written) {
        if (i + static_cast<size_t>(bytes) > src.size()) return false;
        rgba[static_cast<size_t>(written) * 4] = src[i + 2];
        rgba[static_cast<size_t>(written) * 4 + 1] = src[i + 1];
        rgba[static_cast<size_t>(written) * 4 + 2] = src[i];
        if (bytes == 4) rgba[static_cast<size_t>(written) * 4 + 3] = src[i + 3];
        i += static_cast<size_t>(bytes);
      }
    }
  }
  return written == pixels;
}

bool decodeImage(const std::vector<unsigned char>& src, std::vector<unsigned char>& rgba, int& w, int& h) {
  if (src.size() >= 2 && src[0] == 0xff && src[1] == 0xd8) return decodeJpeg(src, rgba, w, h);
  return decodeTga(src, rgba, w, h);
}

}

bool readQuakePk3(const std::string& path, const std::vector<unsigned char>& bsp, std::vector<LevelSurface>& surfaces, std::string& error) {
  if (bsp.size() < 8 + 17 * 8 || std::memcmp(bsp.data(), "IBSP", 4) != 0) {
    error = "pas un BSP Quake 3";
    return false;
  }
  if (readAt<int>(bsp, 4) != 46) {
    error = "version IBSP non supportee";
    return false;
  }
  int lumpOfs[17];
  int lumpLen[17];
  for (int i = 0; i < 17; ++i) {
    lumpOfs[i] = readAt<int>(bsp, 8 + i * 8);
    lumpLen[i] = readAt<int>(bsp, 12 + i * 8);
    if (lumpOfs[i] < 0 || lumpLen[i] < 0 || static_cast<size_t>(lumpOfs[i]) + static_cast<size_t>(lumpLen[i]) > bsp.size()) {
      error = "lump quake hors fichier";
      return false;
    }
  }
  const int nTex = lumpLen[1] / 72;
  const int nVerts = lumpLen[10] / 44;
  const int nIdx = lumpLen[11] / 4;
  const int nFaces = lumpLen[13] / 104;
  if (nTex <= 0 || lumpLen[7] < 40) {
    error = "bsp quake sans geometrie";
    return false;
  }
  const int firstFace = readAt<int>(bsp, static_cast<size_t>(lumpOfs[7] + 24));
  const int numFaces = readAt<int>(bsp, static_cast<size_t>(lumpOfs[7] + 28));
  std::ifstream in(path.c_str(), std::ios::binary);
  std::vector<unsigned char> zip((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::vector<LevelSurface> byTex(static_cast<size_t>(nTex));

  auto vertAt = [&](int vi, float p[3], float uv[2]) {
    const size_t o = static_cast<size_t>(lumpOfs[10] + vi * 44);
    yup(readAt<float>(bsp, o), readAt<float>(bsp, o + 4), readAt<float>(bsp, o + 8), p[0], p[1], p[2]);
    uv[0] = readAt<float>(bsp, o + 12);
    uv[1] = -readAt<float>(bsp, o + 16);
  };
  auto push = [&](int tex, const float a[3], const float b[3], const float c[3], const float n[3], const float ua[2], const float ub[2], const float uc[2]) {
    if (tex < 0 || tex >= nTex) return;
    LevelSurface& s = byTex[static_cast<size_t>(tex)];
    const float* P[3] = {a, b, c};
    const float* U[3] = {ua, ub, uc};
    for (int k = 0; k < 3; ++k) {
      s.positions.insert(s.positions.end(), P[k], P[k] + 3);
      s.normals.insert(s.normals.end(), n, n + 3);
      s.uvs.insert(s.uvs.end(), U[k], U[k] + 2);
    }
  };

  const int faceBegin = firstFace < 0 ? 0 : firstFace;
  const int faceEnd = std::min(nFaces, firstFace + numFaces);
  for (int fi = faceBegin; fi < faceEnd; ++fi) {
    const size_t f = static_cast<size_t>(lumpOfs[13] + fi * 104);
    const int tex = readAt<int>(bsp, f);
    const int type = readAt<int>(bsp, f + 8);
    const int vert0 = readAt<int>(bsp, f + 12);
    const int m0 = readAt<int>(bsp, f + 20);
    const int nm = readAt<int>(bsp, f + 24);
    const int sizeX = readAt<int>(bsp, f + 96);
    const int sizeY = readAt<int>(bsp, f + 100);
    float fn[3] = {0.f, 1.f, 0.f};
    yup(readAt<float>(bsp, f + 84), readAt<float>(bsp, f + 88), readAt<float>(bsp, f + 92), fn[0], fn[1], fn[2]);
    if ((type == 1 || type == 3) && nm >= 3 && m0 >= 0 && m0 + nm <= nIdx) {
      for (int t = 0; t + 2 < nm; t += 3) {
        int id[3];
        bool ok = true;
        for (int k = 0; k < 3; ++k) {
          id[k] = readAt<int>(bsp, static_cast<size_t>(lumpOfs[11] + (m0 + t + k) * 4)) + vert0;
          if (id[k] < 0 || id[k] >= nVerts) ok = false;
        }
        if (!ok) continue;
        float p[3][3], uv[3][2];
        for (int k = 0; k < 3; ++k) vertAt(id[k], p[k], uv[k]);
        push(tex, p[0], p[1], p[2], fn, uv[0], uv[1], uv[2]);
      }
    } else if (type == 2 && sizeX >= 3 && sizeY >= 3 && (sizeX & 1) && (sizeY & 1) && vert0 >= 0 && vert0 + sizeX * sizeY <= nVerts) {
      const int div = 4;
      for (int py = 0; py < (sizeY - 1) / 2; ++py) {
        for (int px = 0; px < (sizeX - 1) / 2; ++px) {
          float cp[3][3][5];
          for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
              float p[3], uv[2];
              vertAt(vert0 + (py * 2 + y) * sizeX + (px * 2 + x), p, uv);
              cp[y][x][0] = p[0];
              cp[y][x][1] = p[1];
              cp[y][x][2] = p[2];
              cp[y][x][3] = uv[0];
              cp[y][x][4] = uv[1];
            }
          }
          for (int y = 0; y < div; ++y) {
            for (int x = 0; x < div; ++x) {
              float s[4][5];
              bezier2(cp, x / float(div), y / float(div), s[0]);
              bezier2(cp, (x + 1) / float(div), y / float(div), s[1]);
              bezier2(cp, (x + 1) / float(div), (y + 1) / float(div), s[2]);
              bezier2(cp, x / float(div), (y + 1) / float(div), s[3]);
              float nn[3] = {fn[0], fn[1], fn[2]};
              float ua[2] = {s[0][3], s[0][4]}, ub[2] = {s[1][3], s[1][4]}, uc[2] = {s[2][3], s[2][4]}, ud[2] = {s[3][3], s[3][4]};
              push(tex, s[0], s[1], s[2], nn, ua, ub, uc);
              push(tex, s[0], s[2], s[3], nn, ua, uc, ud);
            }
          }
        }
      }
    }
  }

  surfaces.clear();
  for (int tex = 0; tex < nTex; ++tex) {
    LevelSurface& s = byTex[static_cast<size_t>(tex)];
    if (s.positions.empty()) continue;
    char name[65] = {};
    std::memcpy(name, bsp.data() + lumpOfs[1] + tex * 72, 64);
    const std::string base = lowerCopy(name);
    std::vector<unsigned char> img;
    if (zipEntry(zip, base + ".jpg", img) || zipEntry(zip, base + ".tga", img) || zipEntry(zip, base + ".jpeg", img)) {
      decodeImage(img, s.rgba, s.width, s.height);
    }
    s.width = s.rgba.empty() ? 0 : s.width;
    s.height = s.rgba.empty() ? 0 : s.height;
    surfaces.push_back(std::move(s));
  }
  if (surfaces.empty()) {
    error = "aucun triangle quake";
    return false;
  }
  return true;
}
