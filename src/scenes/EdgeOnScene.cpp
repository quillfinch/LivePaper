// LivePaper - "Galactic Edge".
//
// An edge-on spiral galaxy laid across the frame: a blazing elliptical core, a
// dark dust lane slicing the disk, soft disk glow above and below the plane, and
// a sparse halo of field stars. The palette tints core and disk; the seed tilts
// and places it.
#include "Scenes.h"
#include <d2d1_1.h>
#include <algorithm>
#include <vector>

namespace lp::scenes {

namespace {

struct FieldStar {
    float x, y;
    float r;
    float phase;
};

} // namespace

class EdgeOnScene final : public Scene {
public:
    const wchar_t* Name() const override { return L"Galactic Edge"; }
    const wchar_t* Description() const override {
        return L"An edge-on galaxy: blazing core, dust lane, disk glow.";
    }
    bool SupportsCustomColor() const override { return true; }
    int ParamCount() const override { return 4; }
    const wchar_t* ParamName(int i) const override {
        switch (i) {
            case 0: return L"Core";
            case 1: return L"Shimmer";
            case 2: return L"Dust";
            case 3: return L"Glow";
            default: return L"";
        }
    }

    void Configure(const SceneCtx& ctx) override {
        ReadParams(ctx);
        Build(ctx);
        m_lastW = ctx.width;
        m_lastH = ctx.height;
    }

    void Update(const SceneCtx& ctx, float dt) override {
        ReadParams(ctx);
        if (std::abs(ctx.width - m_lastW) > 1.0f || std::abs(ctx.height - m_lastH) > 1.0f)
            Configure(ctx);
        m_time += dt * (0.3f + 0.8f * m_shimmer);
    }

    void Draw(const SceneCtx& ctx) override {
        ID2D1DeviceContext* dc = ctx.dc;
        ID2D1SolidColorBrush* brush = ctx.white;
        const float w = ctx.width, h = ctx.height;
        const float cx = w * (0.40f + 0.06f * Fract((float)ctx.variation * 0.577f));
        const float cy = h * 0.46f;
        const float rad = std::max(w, h) * 0.62f;   // disk semi-major
        Palette pal = MakePalette(ctx);
        float bh = 0, bs = 0, bv = 0;
        pal.c[0].ToHsv(bh, bs, bv);

        Color sky = Color::Hsv(Fract(bh + 0.5f), bs * 0.35f, 0.018f);
        D2D1_GRADIENT_STOP bg[2];
        bg[0].position = 0.0f; bg[0].color = D2D1::ColorF(0.003f, 0.003f, 0.008f, 1);
        bg[1].position = 1.0f; bg[1].color = D2D1::ColorF(sky.r, sky.g, sky.b, 1);
        draw::VerticalGradient(dc, w, h, bg, 2);

        // Field stars, sparse away from the plane.
        for (const auto& st : m_field) {
            float tw = 0.6f + 0.4f * std::sin(m_time * 1.3f + st.phase);
            draw::Circle(dc, brush, st.x, st.y, st.r, Color(1, 1, 1, 0.55f * tw));
        }

        // The disk lives under one tilt transform.
        const float tilt = -9.0f;
        dc->SetTransform(D2D1::Matrix3x2F::Rotation(tilt, D2D1::Point2F(cx, cy)));

        // Disk glow: nested flattened ellipses, wider and dimmer outward.
        dc->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_ADD);
        const int layers = 7;
        for (int i = layers; i >= 1; --i) {
            float t = (float)i / layers;
            float rx = rad * t;
            float ry = rad * 0.085f * t + 6.0f;
            float a = (1.0f - t) * (0.16f + 0.14f * m_glow);
            Color disk = pal.c[(i + 1) % Palette::kColors];
            float dh = 0, ds = 0, dv = 0;
            disk.ToHsv(dh, ds, dv);
            disk = Color::Hsv(Fract(dh + 0.02f), ds * 0.8f, dv);
            brush->SetColor(D2D1::ColorF(disk.r, disk.g, disk.b, a));
            dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), brush);
        }

        // Dust lane: a dark flattened band just below the plane centre.
        dc->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
        float dustA = 0.55f * (0.4f + 0.6f * m_dust);
        for (int i = 0; i < 3; ++i) {
            float off = 6.0f + 7.0f * (float)i;
            Color dust = Color::Hsv(Fract(bh + 0.5f), bs * 0.4f, 0.012f, dustA * (1.0f - 0.25f * i));
            brush->SetColor(D2D1::ColorF(dust.r, dust.g, dust.b, dust.a));
            dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy + off),
                                          rad * 0.86f, 5.0f + 3.0f * (float)i), brush);
        }

        // Core blaze, back in additive on top.
        dc->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_ADD);
        float coreA = 0.5f + 0.5f * m_core;
        float breathe = 0.9f + 0.1f * std::sin(m_time * 0.8f);
        draw::RadialGlow(dc, brush, cx, cy, rad * 0.16f * breathe,
                         pal.c[0].WithAlpha(0.45f * coreA), 1.0f, 18);
        draw::RadialGlow(dc, brush, cx, cy, rad * 0.05f,
                         Color(1.0f, 0.98f, 0.94f, 0.85f * coreA), 1.0f, 10);
        // Bright knots along the disk (star-forming regions).
        for (const auto& k : m_knots) {
            float tw = 0.6f + 0.4f * std::sin(m_time * 1.1f + k.phase);
            draw::Circle(dc, brush, k.x, k.y, k.r, Color(1, 1, 1, 0.5f * tw));
            draw::RadialGlow(dc, brush, k.x, k.y, k.r * 4.0f,
                             pal.c[k.ci].WithAlpha(0.25f * tw), 1.0f, 6);
        }
        dc->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
        dc->SetTransform(D2D1::Matrix3x2F::Identity());

        D2D1_GRADIENT_STOP vig[2];
        vig[0].position = 0.0f; vig[0].color = D2D1::ColorF(0, 0, 0, 0);
        vig[1].position = 1.0f; vig[1].color = D2D1::ColorF(0, 0, 0, 0.38f);
        draw::VerticalGradient(dc, w, h, vig, 2);
    }

private:
    void ReadParams(const SceneCtx& ctx) {
        m_core = ctx.config->sceneParam[0];
        m_shimmer = ctx.config->sceneParam[1];
        m_dust = ctx.config->sceneParam[2];
        m_glow = ctx.config->sceneParam[3];
    }

    void Build(const SceneCtx& ctx) {
        const float w = ctx.width, h = ctx.height;
        const float cx = w * 0.46f, cy = h * 0.46f;
        const float rad = std::max(w, h) * 0.62f;
        const float tilt = -9.0f * (float)kPi / 180.0f;
        Rng rng(0xED5311u + ctx.variation * 7919u);

        // Field stars: denser near the disk plane.
        m_field.clear();
        int stars = (int)(150 * ctx.density);
        m_field.reserve((size_t)stars);
        for (int i = 0; i < stars; ++i) {
            FieldStar st{};
            st.x = rng.Range(0.0f, w);
            float plane = std::abs(rng.Unit() - rng.Unit());   // triangular
            st.y = cy + (rng.Unit() - 0.5f) * h * (0.15f + plane * 1.1f);
            st.r = rng.Range(0.6f, 1.6f);
            st.phase = rng.Range(0.0f, kTau);
            m_field.push_back(st);
        }

        // Bright knots along the disk, in tilted-plane coordinates.
        m_knots.clear();
        int knots = std::min((int)(10 * ctx.density), 14);
        m_knots.reserve((size_t)knots);
        for (int i = 0; i < knots; ++i) {
            float u = rng.Range(-0.85f, 0.85f);            // position along disk
            float v = rng.Range(-1.0f, 1.0f) * 4.0f;       // slight thickness
            float kx = cx + std::cos(tilt) * u * rad;
            float ky = cy + std::sin(tilt) * u * rad + v;
            m_knots.push_back({ kx, ky, rng.Range(1.0f, 2.4f), i % Palette::kColors,
                                rng.Range(0.0f, kTau) });
        }
    }

    struct Knot { float x, y, r; int ci; float phase; };

    std::vector<FieldStar> m_field;
    std::vector<Knot> m_knots;
    float m_time = 0;
    float m_core = 0.5f, m_shimmer = 0.5f, m_dust = 0.5f, m_glow = 0.5f;
    float m_lastW = 1920, m_lastH = 1080;
};

Scene* CreateEdgeOn() { return new EdgeOnScene(); }

} // namespace lp::scenes
