// PDF generation with libharu: UTF-8 TrueType fonts embedded, logos drawn
// with their aspect ratio preserved (never distorted).
#include "pdf/pdf.hpp"

#include <hpdf.h>
#include <png.h>

#include <algorithm>
#include <csetjmp>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace nh::pdf {

namespace {

struct Rgb {
    float r, g, b;
};
constexpr Rgb kInk{0.078f, 0.094f, 0.122f};        // ink #14181F
constexpr Rgb kMuted{0.361f, 0.388f, 0.420f};
constexpr Rgb kRule{0.863f, 0.843f, 0.808f};
constexpr Rgb kHeaderBg{0.953f, 0.941f, 0.918f};
constexpr Rgb kAccent{0.090f, 0.247f, 0.373f};   // Nhill night blue #173F5F
constexpr Rgb kCopper{0.718f, 0.525f, 0.165f};    // summit gold #B7862A
constexpr Rgb kWatermark{0.88f, 0.86f, 0.83f};

// ---- PNG pre-validation. libharu hands PNG data to libpng without a
// recovery point: a corrupted file aborts the process. Every PNG is therefore
// fully decoded here first, with a proper setjmp-protected error handler.
struct MemReader {
    const unsigned char* data;
    size_t size;
    size_t pos;
};
void pngRead(png_structp png, png_bytep out, png_size_t len) {
    auto* r = static_cast<MemReader*>(png_get_io_ptr(png));
    if (r->pos + len > r->size) png_error(png, "truncated");
    std::memcpy(out, r->data + r->pos, len);
    r->pos += len;
}
void pngError(png_structp png, png_const_charp) { longjmp(png_jmpbuf(png), 1); }
void pngWarning(png_structp, png_const_charp) {}

bool decodePngSafely(const std::string& bytes) {
    if (bytes.size() < 8 || png_sig_cmp(reinterpret_cast<png_const_bytep>(bytes.data()), 0, 8) != 0) return false;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, pngError, pngWarning);
    if (!png) return false;
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        return false;
    }
    MemReader reader{reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), 0};
    std::vector<unsigned char> row;
    bool ok = false;
    if (setjmp(png_jmpbuf(png)) == 0) {
        png_set_read_fn(png, &reader, pngRead);
        png_set_user_limits(png, 4096, 4096);
        png_read_info(png, info);
        int passes = png_set_interlace_handling(png);
        png_read_update_info(png, info);
        row.resize(png_get_rowbytes(png, info));
        png_uint_32 height = png_get_image_height(png, info);
        for (int p = 0; p < passes; ++p)
            for (png_uint_32 y = 0; y < height; ++y) png_read_row(png, row.data(), nullptr);
        png_read_end(png, nullptr);
        ok = true;
    }
    png_destroy_read_struct(&png, &info, nullptr);
    return ok;
}

void onError(HPDF_STATUS error, HPDF_STATUS detail, void* user) {
    auto* msg = static_cast<std::string*>(user);
    char buf[96];
    std::snprintf(buf, sizeof buf, "libharu error 0x%04X detail %u", static_cast<unsigned>(error),
                  static_cast<unsigned>(detail));
    *msg = buf;
}

class Pdf {
public:
    Pdf() {
        doc_ = HPDF_New(onError, &error_);
        if (!doc_) throw std::runtime_error("cannot create PDF");
        HPDF_SetCompressionMode(doc_, HPDF_COMP_ALL);
        HPDF_UseUTFEncodings(doc_);
        HPDF_SetCurrentEncoder(doc_, "UTF-8");
    }
    ~Pdf() { HPDF_Free(doc_); }
    HPDF_Doc doc() { return doc_; }
    void check(const char* what) {
        if (!error_.empty()) throw std::runtime_error(std::string(what) + ": " + error_);
    }
    HPDF_Font font(const std::string& path) {
        const char* name = HPDF_LoadTTFontFromFile(doc_, path.c_str(), HPDF_TRUE);
        check("load font");
        if (!name) throw std::runtime_error("cannot load font " + path);
        HPDF_Font f = HPDF_GetFont(doc_, name, "UTF-8");
        check("get font");
        return f;
    }
    HPDF_Image image(const std::string& bytes, const std::string& mime) {
        HPDF_Image img = nullptr;
        auto* data = reinterpret_cast<const HPDF_BYTE*>(bytes.data());
        if (mime == "image/png" && !decodePngSafely(bytes)) return nullptr;
        if (mime == "image/png") img = HPDF_LoadPngImageFromMem(doc_, data, static_cast<HPDF_UINT>(bytes.size()));
        else if (mime == "image/jpeg") img = HPDF_LoadJpegImageFromMem(doc_, data, static_cast<HPDF_UINT>(bytes.size()));
        if (!img || !error_.empty()) {
            error_.clear();
            HPDF_ResetError(doc_);
            return nullptr;
        }
        return img;
    }
    std::string save() {
        HPDF_SaveToStream(doc_);
        check("save");
        HPDF_ResetStream(doc_);
        std::string out;
        for (;;) {
            HPDF_BYTE buf[8192];
            HPDF_UINT32 n = sizeof buf;
            HPDF_ReadFromStream(doc_, buf, &n);
            if (n == 0) break;
            out.append(reinterpret_cast<char*>(buf), n);
        }
        HPDF_ResetError(doc_);
        error_.clear();
        return out;
    }
    void setDate(const std::string& iso) {
        // "2026-10-25T16:30:00Z"
        if (iso.size() < 19) return;
        HPDF_Date d{};
        d.year = std::stoi(iso.substr(0, 4));
        d.month = std::stoi(iso.substr(5, 2));
        d.day = std::stoi(iso.substr(8, 2));
        d.hour = std::stoi(iso.substr(11, 2));
        d.minutes = std::stoi(iso.substr(14, 2));
        d.seconds = std::stoi(iso.substr(17, 2));
        d.ind = 'Z';
        HPDF_SetInfoDateAttr(doc_, HPDF_INFO_CREATION_DATE, d);
        HPDF_SetInfoDateAttr(doc_, HPDF_INFO_MOD_DATE, d);
    }

private:
    HPDF_Doc doc_;
    std::string error_;
};

struct Fontset {
    HPDF_Font regular, semibold, mono;
};

void color(HPDF_Page p, Rgb c) {
    HPDF_Page_SetRGBFill(p, c.r, c.g, c.b);
    HPDF_Page_SetRGBStroke(p, c.r, c.g, c.b);
}

float width(HPDF_Page p, HPDF_Font f, float size, const std::string& s) {
    HPDF_Page_SetFontAndSize(p, f, size);
    return HPDF_Page_TextWidth(p, s.c_str());
}

void text(HPDF_Page p, HPDF_Font f, float size, float x, float y, const std::string& s, Rgb c = kInk) {
    if (s.empty()) return;
    color(p, c);
    HPDF_Page_BeginText(p);
    HPDF_Page_SetFontAndSize(p, f, size);
    HPDF_Page_TextOut(p, x, y, s.c_str());
    HPDF_Page_EndText(p);
}

void textRight(HPDF_Page p, HPDF_Font f, float size, float right, float y, const std::string& s, Rgb c = kInk) {
    text(p, f, size, right - width(p, f, size, s), y, s, c);
}

void rule(HPDF_Page p, float x1, float y, float x2, float w = 0.6f, Rgb c = kRule) {
    color(p, c);
    HPDF_Page_SetLineWidth(p, w);
    HPDF_Page_MoveTo(p, x1, y);
    HPDF_Page_LineTo(p, x2, y);
    HPDF_Page_Stroke(p);
}

void rect(HPDF_Page p, float x, float y, float w, float h, Rgb c) {
    color(p, c);
    HPDF_Page_Rectangle(p, x, y, w, h);
    HPDF_Page_Fill(p);
}

// Splits UTF-8 text into lines no wider than maxWidth (word wrap, then
// character wrap for very long words).
std::vector<std::string> wrap(HPDF_Page p, HPDF_Font f, float size, const std::string& s, float maxWidth) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= s.size()) {
        size_t nl = s.find('\n', start);
        std::string para = s.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        std::string line;
        size_t i = 0;
        while (i < para.size()) {
            size_t sp = para.find(' ', i);
            std::string word = para.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
            std::string candidate = line.empty() ? word : line + " " + word;
            if (width(p, f, size, candidate) <= maxWidth) {
                line = candidate;
            } else {
                if (!line.empty()) lines.push_back(line);
                line.clear();
                // break the word itself if needed
                std::string piece;
                for (size_t k = 0; k < word.size();) {
                    unsigned char c = static_cast<unsigned char>(word[k]);
                    size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
                    std::string ch = word.substr(k, n);
                    if (width(p, f, size, piece + ch) > maxWidth && !piece.empty()) {
                        lines.push_back(piece);
                        piece.clear();
                    }
                    piece += ch;
                    k += n;
                }
                line = piece;
            }
            if (sp == std::string::npos) break;
            i = sp + 1;
        }
        lines.push_back(line);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return lines;
}

void watermark(HPDF_Page p, HPDF_Font f, const std::string& s) {
    if (s.empty()) return;
    float W = HPDF_Page_GetWidth(p), H = HPDF_Page_GetHeight(p);
    float size = 46;
    float w = width(p, f, size, s);
    float angle = 0.6f;
    float c = std::cos(angle), sn = std::sin(angle);
    color(p, kWatermark);
    HPDF_Page_BeginText(p);
    HPDF_Page_SetFontAndSize(p, f, size);
    HPDF_Page_SetTextMatrix(p, c, sn, -sn, c, W / 2 - (w / 2) * c, H / 2 - (w / 2) * sn);
    HPDF_Page_ShowText(p, s.c_str());
    HPDF_Page_EndText(p);
}

// Draws the logo inside a box, keeping its proportions (never distorted).
// Returns the drawn height; the drawn width is stored in *drawnW.
float drawLogo(Pdf& pdf, HPDF_Page p, const std::optional<std::string>& bytes, const std::string& mime, float x, float top,
               float maxW, float maxH, float* drawnW = nullptr) {
    if (drawnW) *drawnW = 0;
    if (!bytes) return 0;
    HPDF_Image img = pdf.image(*bytes, mime);
    if (!img) return 0;  // invalid logo: neutral text identity is used instead
    float iw = static_cast<float>(HPDF_Image_GetWidth(img)), ih = static_cast<float>(HPDF_Image_GetHeight(img));
    if (iw <= 0 || ih <= 0) return 0;
    float scale = std::min(maxW / iw, maxH / ih);
    float w = iw * scale, h = ih * scale;
    HPDF_Page_DrawImage(p, img, x, top - h, w, h);
    if (drawnW) *drawnW = w;
    return h;
}

void footers(std::vector<HPDF_Page>& pages, const Fontset& f, const std::string& left, const std::string& pageLabel,
             float margin) {
    for (size_t i = 0; i < pages.size(); ++i) {
        HPDF_Page p = pages[i];
        float W = HPDF_Page_GetWidth(p);
        rule(p, margin, margin + 14, W - margin, 0.5f);
        text(p, f.regular, 7, margin, margin + 3, left, kMuted);
        textRight(p, f.regular, 7, W - margin, margin + 3,
                  pageLabel + " " + std::to_string(i + 1) + " / " + std::to_string(pages.size()), kMuted);
    }
}

}  // namespace

Fonts Fonts::fromDir(const std::string& dir) {
    Fonts f;
    f.regular = dir + "/IBMPlexSans-Regular.ttf";
    f.semibold = dir + "/IBMPlexSans-SemiBold.ttf";
    f.mono = dir + "/IBMPlexMono-Regular.ttf";
    for (const auto* p : {&f.regular, &f.semibold, &f.mono})
        if (!std::filesystem::exists(*p)) throw std::runtime_error("font not found: " + *p);
    return f;
}

bool probeImage(const std::string& bytes, const std::string& mime) {
    try {
        Pdf pdf;
        return pdf.image(bytes, mime) != nullptr;
    } catch (...) {
        return false;
    }
}

std::string renderSalesDocument(const SalesDocView& v, const Fonts& fonts) {
    Pdf pdf;
    Fontset f{pdf.font(fonts.regular), pdf.font(fonts.semibold), pdf.font(fonts.mono)};
    HPDF_SetInfoAttr(pdf.doc(), HPDF_INFO_TITLE, (v.title + (v.number.empty() ? "" : " " + v.number)).c_str());
    HPDF_SetInfoAttr(pdf.doc(), HPDF_INFO_PRODUCER, "Nhill");
    pdf.setDate(v.creationDateIso);

    const float M = 42;
    std::vector<HPDF_Page> pages;
    auto newPage = [&]() {
        HPDF_Page p = HPDF_AddPage(pdf.doc());
        HPDF_Page_SetSize(p, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
        watermark(p, f.semibold, v.watermark);
        pages.push_back(p);
        return p;
    };
    HPDF_Page p = newPage();
    const float W = HPDF_Page_GetWidth(p), H = HPDF_Page_GetHeight(p);
    const float right = W - M;
    float y = H - M;

    // ---- header: logo / identity (left), title block (right)
    float logoH = drawLogo(pdf, p, v.logoBytes, v.logoMime, M, y, 160, 58);
    float leftY = y - (logoH > 0 ? logoH + 12 : 0);
    if (logoH == 0) {
        for (const auto& l : wrap(p, f.semibold, 15, v.sellerName, 260)) {
            text(p, f.semibold, 15, M, leftY - 14, l);
            leftY -= 19;
        }
        leftY -= 4;
    } else {
        text(p, f.semibold, 10.5f, M, leftY - 10, v.sellerName);
        leftY -= 15;
    }
    for (const auto& line : v.sellerLines)
        for (const auto& l : wrap(p, f.regular, 8.5f, line, 270)) {
            text(p, f.regular, 8.5f, M, leftY - 9, l, kMuted);
            leftY -= 11.5f;
        }

    float ry = y;
    textRight(p, f.semibold, 21, right, ry - 20, v.title, kAccent);
    ry -= 30;
    if (!v.number.empty()) {
        textRight(p, f.mono, 11, right, ry - 10, v.number);
        ry -= 18;
    }
    for (const auto& [k, val] : v.meta) {
        textRight(p, f.regular, 8.5f, right, ry - 9, val);
        textRight(p, f.regular, 8.5f, right - width(p, f.regular, 8.5f, val) - 8, ry - 9, k, kMuted);
        ry -= 12;
    }

    // ---- buyer block
    float by = std::min(leftY, ry) - 16;
    float bx = W / 2 + 10;
    float bw = right - bx;
    std::vector<std::string> blines;
    for (const auto& line : v.buyerLines)
        for (const auto& l : wrap(p, f.regular, 8.5f, line, bw - 20)) blines.push_back(l);
    auto nameLines = wrap(p, f.semibold, 10.5f, v.buyerName, bw - 20);
    float boxH = 22 + 14 * nameLines.size() + 11.5f * blines.size() + 8;
    rect(p, bx, by - boxH, bw, boxH, kHeaderBg);
    rect(p, bx, by - boxH, 2.2f, boxH, kAccent);
    float yy = by - 14;
    text(p, f.semibold, 7, bx + 12, yy, v.buyerLabel, kMuted);
    yy -= 15;
    for (const auto& l : nameLines) {
        text(p, f.semibold, 10.5f, bx + 12, yy, l);
        yy -= 14;
    }
    for (const auto& l : blines) {
        text(p, f.regular, 8.5f, bx + 12, yy, l, kMuted);
        yy -= 11.5f;
    }
    y = std::min(by - boxH, leftY) - 22;

    // ---- lines table
    const std::vector<float> fixed = {52, 0, 62, 70, 42, 40, 74};
    float descW = (right - M) - (fixed[0] + fixed[2] + fixed[3] + fixed[4] + fixed[5] + fixed[6]);
    std::vector<float> colW = fixed;
    colW[1] = descW;
    std::vector<float> colX(colW.size());
    colX[0] = M;
    for (size_t i = 1; i < colW.size(); ++i) colX[i] = colX[i - 1] + colW[i - 1];
    auto header = [&](HPDF_Page pg, float top) {
        rect(pg, M, top - 18, right - M, 18, kHeaderBg);
        for (size_t i = 0; i < v.columns.size() && i < colW.size(); ++i) {
            if (i <= 1) text(pg, f.semibold, 7.5f, colX[i] + 4, top - 12, v.columns[i], kMuted);
            else textRight(pg, f.semibold, 7.5f, colX[i] + colW[i] - 4, top - 12, v.columns[i], kMuted);
        }
        return top - 18;
    };
    const float bottomLimit = M + 30;
    float payBottom = H;  // lowest point of the payments block, when there is one
    y = header(p, y);
    for (const auto& l : v.lines) {
        auto desc = wrap(p, f.regular, 8.5f, l.description, colW[1] - 8);
        float rowH = 7 + 11.5f * desc.size() + 4;
        if (y - rowH < bottomLimit) {
            p = newPage();
            y = header(p, H - M);
        }
        float ty = y - 12;
        text(p, f.mono, 7.5f, colX[0] + 4, ty, l.ref, kMuted);
        float dy = ty;
        for (const auto& d : desc) {
            text(p, f.regular, 8.5f, colX[1] + 4, dy, d);
            dy -= 11.5f;
        }
        textRight(p, f.regular, 8.5f, colX[2] + colW[2] - 4, ty, l.qty);
        textRight(p, f.regular, 8.5f, colX[3] + colW[3] - 4, ty, l.unitPrice);
        textRight(p, f.regular, 8.5f, colX[4] + colW[4] - 4, ty, l.discount, kMuted);
        textRight(p, f.regular, 8.5f, colX[5] + colW[5] - 4, ty, l.tax, kMuted);
        textRight(p, f.semibold, 8.5f, colX[6] + colW[6] - 4, ty, l.total);
        y -= rowH;
        rule(p, M, y, right, 0.4f);
    }

    // ---- totals (right) and payments received (left)
    float needed = 16.0f * std::max(v.totals.size() + v.afterTotals.size(), v.payments.size() + 1) + 40;
    if (y - needed < bottomLimit) {
        p = newPage();
        y = H - M;
    }
    y -= 10;
    const float tx = right - 230;
    if (!v.payments.empty()) {
        float py = y;
        text(p, f.semibold, 7.5f, M, py - 10, v.paymentsTitle, kMuted);
        py -= 16;
        const float pw = tx - 24 - M;
        for (const auto& [k, val] : v.payments) {
            float aw = width(p, f.regular, 8.5f, val);
            auto ls = wrap(p, f.regular, 8.5f, k, std::max(60.0f, pw - aw - 10));
            textRight(p, f.regular, 8.5f, M + pw, py - 10, val);
            for (const auto& l : ls) {
                text(p, f.regular, 8.5f, M, py - 10, l);
                py -= 12;
            }
            py -= 3;
            rule(p, M, py + 1, M + pw, 0.3f);
        }
        payBottom = py;
    }
    for (const auto& [k, val] : v.totals) {
        text(p, f.regular, 8.5f, tx, y - 10, k, kMuted);
        textRight(p, f.regular, 8.5f, right, y - 10, val);
        y -= 15;
    }
    rule(p, tx, y - 2, right, 1.0f, kInk);
    y -= 6;
    text(p, f.semibold, 11, tx, y - 13, v.grandTotal.first, kAccent);
    textRight(p, f.semibold, 11, right, y - 13, v.grandTotal.second, kAccent);
    y -= 22;
    for (const auto& [k, val] : v.afterTotals) {
        text(p, f.regular, 8.5f, tx, y - 10, k, kMuted);
        textRight(p, f.semibold, 8.5f, right, y - 10, val);
        y -= 15;
    }
    y = std::min(y, payBottom);

    // ---- notices (e.g. "does not replace the DEF standardised invoice")
    y -= 8;
    for (const auto& n : v.notices) {
        auto ls = wrap(p, f.regular, 8, n, right - M - 20);
        float h = 10 + 11 * ls.size();
        if (y - h < bottomLimit) {
            p = newPage();
            y = H - M;
        }
        rect(p, M, y - h, 2.2f, h, kCopper);
        float ny = y - 11;
        for (const auto& l : ls) {
            text(p, f.regular, 8, M + 10, ny, l);
            ny -= 11;
        }
        y -= h + 8;
    }

    // ---- footnotes
    for (const auto& [k, val] : v.footnotes) {
        auto ls = wrap(p, f.regular, 8, val, right - M);
        float h = 14 + 11 * ls.size();
        if (y - h < bottomLimit) {
            p = newPage();
            y = H - M;
        }
        text(p, f.semibold, 7.5f, M, y - 10, k, kMuted);
        float ny = y - 22;
        for (const auto& l : ls) {
            text(p, f.regular, 8, M, ny, l);
            ny -= 11;
        }
        y -= h + 6;
    }

    footers(pages, f, v.footerLeft, v.pageLabel, M - 18);
    return pdf.save();
}

std::string renderTable(const TableReport& r, const Fonts& fonts) {
    Pdf pdf;
    Fontset f{pdf.font(fonts.regular), pdf.font(fonts.semibold), pdf.font(fonts.mono)};
    HPDF_SetInfoAttr(pdf.doc(), HPDF_INFO_TITLE, r.title.c_str());
    HPDF_SetInfoAttr(pdf.doc(), HPDF_INFO_PRODUCER, "Nhill");
    const float M = 36;
    std::vector<HPDF_Page> pages;
    auto newPage = [&]() {
        HPDF_Page p = HPDF_AddPage(pdf.doc());
        HPDF_Page_SetSize(p, HPDF_PAGE_SIZE_A4, r.landscape ? HPDF_PAGE_LANDSCAPE : HPDF_PAGE_PORTRAIT);
        pages.push_back(p);
        return p;
    };
    HPDF_Page p = newPage();
    const float W = HPDF_Page_GetWidth(p), H = HPDF_Page_GetHeight(p);
    const float right = W - M;
    float y = H - M;
    float logoW = 0;
    float logoH = drawLogo(pdf, p, r.logoBytes, r.logoMime, M, y, 120, 40, &logoW);
    text(p, f.semibold, 10, M + (logoH > 0 ? logoW + 12 : 0), y - (logoH > 0 ? logoH / 2 + 4 : 12), r.establishmentName, kMuted);
    y -= std::max(logoH, 18.0f) + 10;
    text(p, f.semibold, 16, M, y - 14, r.title, kAccent);
    y -= 22;
    text(p, f.regular, 9, M, y - 10, r.subtitle, kMuted);
    y -= 22;

    double total = 0;
    for (double w : r.widths) total += w;
    std::vector<float> colW, colX;
    float x = M;
    for (double w : r.widths) {
        colX.push_back(x);
        colW.push_back(static_cast<float>((right - M) * w / total));
        x += colW.back();
    }
    auto header = [&](HPDF_Page pg, float top) {
        float h = 0;
        std::vector<std::vector<std::string>> hl;
        for (size_t i = 0; i < r.columns.size(); ++i) {
            hl.push_back(wrap(pg, f.semibold, 7, r.columns[i], colW[i] - 6));
            h = std::max(h, 6 + 9.0f * hl.back().size());
        }
        rect(pg, M, top - h - 4, right - M, h + 4, kHeaderBg);
        for (size_t i = 0; i < r.columns.size(); ++i) {
            float yy = top - 11;
            for (const auto& l : hl[i]) {
                if (r.rightAlign[i]) textRight(pg, f.semibold, 7, colX[i] + colW[i] - 3, yy, l, kMuted);
                else text(pg, f.semibold, 7, colX[i] + 3, yy, l, kMuted);
                yy -= 9;
            }
        }
        return top - h - 4;
    };
    y = header(p, y);
    auto drawRow = [&](const std::vector<std::string>& row, bool bold) {
        std::vector<std::vector<std::string>> cells;
        float h = 0;
        HPDF_Font font = bold ? f.semibold : f.regular;
        for (size_t i = 0; i < row.size() && i < colW.size(); ++i) {
            cells.push_back(wrap(p, font, 7.5f, row[i], colW[i] - 6));
            h = std::max(h, 5 + 10.0f * cells.back().size());
        }
        if (y - h < M + 30) {
            p = newPage();
            y = header(p, H - M);
        }
        for (size_t i = 0; i < cells.size(); ++i) {
            float yy = y - 10;
            for (const auto& l : cells[i]) {
                if (r.rightAlign[i]) textRight(p, font, 7.5f, colX[i] + colW[i] - 3, yy, l);
                else text(p, font, 7.5f, colX[i] + 3, yy, l);
                yy -= 10;
            }
        }
        y -= h + 2;
        rule(p, M, y, right, 0.3f);
    };
    for (const auto& row : r.rows) drawRow(row, false);
    if (!r.totalRow.empty()) drawRow(r.totalRow, true);
    footers(pages, f, r.footerLeft, r.pageLabel, M - 14);
    return pdf.save();
}

std::string renderTicket(const TicketView& v, const Fonts& fonts) {
    Pdf pdf;
    Fontset f{pdf.font(fonts.regular), pdf.font(fonts.semibold), pdf.font(fonts.mono)};
    HPDF_SetInfoAttr(pdf.doc(), HPDF_INFO_TITLE, v.title.c_str());
    HPDF_SetInfoAttr(pdf.doc(), HPDF_INFO_PRODUCER, "Nhill");
    pdf.setDate(v.creationDateIso);
    const float W = v.widthMm * 72.0f / 25.4f;
    const float M = v.widthMm < 70 ? 6.0f : 10.0f;
    const float inner = W - 2 * M;
    // Layout pass on a scratch page (text widths), then drawing on a page of the exact height.
    HPDF_Page scratch = HPDF_AddPage(pdf.doc());
    struct Op {
        TicketBlock::Kind kind;
        std::vector<std::string> lines;
        std::string right, sub;
        float h;
    };
    std::vector<Op> ops;
    float total = M;
    for (const auto& b : v.blocks) {
        Op op{b.kind, {}, b.b, b.c, 0};
        switch (b.kind) {
            case TicketBlock::Title: op.lines = wrap(scratch, f.semibold, 11, b.a, inner); op.h = 14.0f * op.lines.size() + 2; break;
            case TicketBlock::Center: op.lines = wrap(scratch, f.regular, 8, b.a, inner); op.h = 10.5f * op.lines.size(); break;
            case TicketBlock::Text:
            case TicketBlock::Muted: op.lines = wrap(scratch, f.regular, 8, b.a, inner); op.h = 10.5f * op.lines.size(); break;
            case TicketBlock::KV:
            case TicketBlock::KVStrong: {
                HPDF_Font font = b.kind == TicketBlock::KVStrong ? f.semibold : f.regular;
                float size = b.kind == TicketBlock::KVStrong ? 10.0f : 8.0f;
                float rw = width(scratch, font, size, b.b);
                op.lines = wrap(scratch, font, size, b.a, std::max(40.0f, inner - rw - 6));
                op.h = (size + 3) * op.lines.size();
                break;
            }
            case TicketBlock::Item: op.lines = wrap(scratch, f.regular, 8, b.a, inner); op.h = 10.5f * op.lines.size() + 10.5f; break;
            case TicketBlock::Rule: op.h = 7; break;
            case TicketBlock::Space: op.h = 6; break;
        }
        total += op.h;
        ops.push_back(op);
    }
    float logoMax = 0;
    if (v.logoBytes) logoMax = 42;
    total += M + logoMax + (logoMax > 0 ? 6 : 0);
    // The scratch page becomes the real page with the computed height.
    HPDF_Page p = scratch;
    HPDF_Page_SetWidth(p, W);
    HPDF_Page_SetHeight(p, std::max(total, 120.0f));
    float y = HPDF_Page_GetHeight(p) - M;
    if (v.logoBytes) {
        float lw = 0;
        // centred logo, proportions kept
        HPDF_Image img = pdf.image(*v.logoBytes, v.logoMime);
        if (img) {
            float iw = static_cast<float>(HPDF_Image_GetWidth(img)), ih = static_cast<float>(HPDF_Image_GetHeight(img));
            float sc = std::min(inner * 0.6f / iw, logoMax / ih);
            lw = iw * sc;
            HPDF_Page_DrawImage(p, img, (W - lw) / 2, y - ih * sc, lw, ih * sc);
            y -= ih * sc + 6;
        }
    }
    for (const auto& op : ops) {
        switch (op.kind) {
            case TicketBlock::Title:
                for (const auto& l : op.lines) { text(p, f.semibold, 11, (W - width(p, f.semibold, 11, l)) / 2, y - 11, l, kAccent); y -= 14; }
                y -= 2;
                break;
            case TicketBlock::Center:
                for (const auto& l : op.lines) { text(p, f.regular, 8, (W - width(p, f.regular, 8, l)) / 2, y - 8, l, kMuted); y -= 10.5f; }
                break;
            case TicketBlock::Text:
            case TicketBlock::Muted:
                for (const auto& l : op.lines) { text(p, f.regular, 8, M, y - 8, l, op.kind == TicketBlock::Muted ? kMuted : kInk); y -= 10.5f; }
                break;
            case TicketBlock::KV:
            case TicketBlock::KVStrong: {
                HPDF_Font font = op.kind == TicketBlock::KVStrong ? f.semibold : f.regular;
                float size = op.kind == TicketBlock::KVStrong ? 10.0f : 8.0f;
                textRight(p, font, size, W - M, y - size, op.right);
                for (const auto& l : op.lines) { text(p, font, size, M, y - size, l); y -= size + 3; }
                break;
            }
            case TicketBlock::Item:
                for (const auto& l : op.lines) { text(p, f.regular, 8, M, y - 8, l); y -= 10.5f; }
                text(p, f.regular, 7.5f, M + 8, y - 8, op.right, kMuted);
                textRight(p, f.semibold, 8, W - M, y - 8, op.sub);
                y -= 10.5f;
                break;
            case TicketBlock::Rule:
                rule(p, M, y - 3.5f, W - M, 0.5f, kRule);
                y -= 7;
                break;
            case TicketBlock::Space: y -= 6; break;
        }
    }
    return pdf.save();
}

}  // namespace nh::pdf
