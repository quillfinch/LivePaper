// LivePaper - "Stellar Nursery".
//
// A dark molecular cloud lit from within: bright protostars with four-point
// diffraction spikes, halos that color the gas around them, and a slow twinkle.
// Protostar placement, sizes and tints all come from the seed; the palette
// colors the gas.
#include "Scenes.h"
#include <d2d1_1.h>
#include <algorithm>
#include <vector>

namespace lp::scenes {

namespace {

struct Proto {
    float x, y;
    float core;      // core radius, px
    float spike;     // diffraction spike length, px
    int ci;
    float phase;
    float twinkleF;
};

struct Wisp {
    float x, y, r;
    float alpha;
};

} // namespace

class NurseryScene final : public Scene {
public:
    const wchar_t* Name() const override { return L"Stellar Nursery"; }
    const wchar_t* Description() const override {
        return L"Protostars igniting inside a dark molecular cloud.";
    }
    bool SupportsCustomColor() const override { return true; }
    int ParamCount() const override { return 4; }
    const wchar_t* ParamName(int i) const override {
        switch (i) {
            case 0: return L"Stars";
            case 1: return L"Speed";
            case 2: return L"Glow";
            case 3: return L"Cloud";
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
        m_time += dt * (0.3f + 0.8f * m_speed);
    }

    void Draw(const SceneCtx& ctx) override {
        ID2D1DeviceContext* dc = ctx.dc;
        ID2D1SolidColorBrush* brush = ctx.white;
        const float w = ctx.width, h = ctx.height;
        Palette pal = MakePalette(ctx);
        float bh = 0, bs = 0, bv = 0;
        pal.c[0].ToHsv(bh, bs, bv);

        // Near-black sky with a whisper of the palette.
        Color sky = Color::Hsv(Fract(bh + 0.5f), bs * 0.4f, 0.020f);
        D2D1_GRADIENT_STOP bg[2];
        bg[0].position = 0.0f; bg[0].color = D2D1::ColorF(0.003f, 0.003f, 0.008f, 1);
        bg[1].position = 1.0f; bg[1].color = D2D1::ColorF(sky.r, sky.g, sky.b, 1);
        draw::VerticalGradient(dc, w, h, bg, 2);
        m_starsBG.Draw(dc, brush, m_time);

        // Molecular cloud: overlapping dark wisps, faintly rim-lit.
        for (const auto& wisp : m_wisps) {
            float a = wisp.alpha * (0.5f + 0.5f * m_cloud);
            Color dark = Color::Hsv(Fract(bh + 0.52f), bs * 0.45f, 0.060f, a);
            draw::Circle(dc, brush, wisp.x, wisp.y, wisp.r, dark);
        }
        dc->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_ADD);
        for (const auto& wisp : m_wisps) {
            if (wisp.r < w * 0.16f) continue;   // only the big wisps get rims
            draw::Circle(dc, brush, wisp.x - wisp.r * 0.15f, wisp.y - wisp.r * 0.18f,
                         wisp.r * 0.8f, Color::Hsv(bh, bs * 0.4f, 1.0f, 0.045f * m_cloud));
        }

        // Protostars.
        for (const auto& st : m_protos) {
            float twinkle = 0.7f
                          + 0.3f * std::sin(m_time * st.twinkleF + st.phase)
                          * std::sin(m_time * st.twinkleF * 0.37f + st.phase * 2.0f);
            float a = (0.7f + 0.5f * m_glow) * twinkle;
            // Illuminated gas around the star.
            draw::RadialGlow(dc, brush, st.x, st.y, st.core * 11.0f,
                             pal.c[st.ci].WithAlpha(0.17f * a), 1.0f, 16);
            draw::RadialGlow(dc, brush, st.x, st.y, st.core * 3.4f,
                             pal.c[st.ci].WithAlpha(0.45f * a), 1.0f, 12);
            // Diffraction spikes: two long, two short.
            draw::Line(dc, brush, st.x - st.spike, st.y, st.x + st.spike, st.y,
                       1.6f, Color(1, 1, 1, 0.75f * a));
            draw::Line(dc, brush, st.x, st.y - st.spike, st.x, st.y + st.spike,
                       1.6f, Color(1, 1, 1, 0.75f * a));
            draw::Line(dc, brush, st.x - st.spike * 0.45f, st.y - st.spike * 0.45f,
                       st.x + st.spike * 0.45f, st.y + st.spike * 0.45f,
                       1.1f, Color(1, 1, 1, 0.40f * a));
            draw::Line(dc, brush, st.x - st.spike * 0.45f, st.y + st.spike * 0.45f,
                       st.x + st.spike * 0.45f, st.y - st.spike * 0.45f,
                       1.1f, Color(1, 1, 1, 0.40f * a));
            draw::Circle(dc, brush, st.x, st.y, st.core, Color(1, 1, 1, 0.95f * a));
        }
        dc->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);

        D2D1_GRADIENT_STOP vig[2];
        vig[0].position = 0.0f; vig[0].color = D2D1::ColorF(0, 0, 0, 0);
        vig[1].position = 1.0f; vig[1].color = D2D1::ColorF(0, 0, 0, 0.40f);
        draw::VerticalGradient(dc, w, h, vig, 2);
    }

private:
    void ReadParams(const SceneCtx& ctx) {
        m_starsAmt = ctx.config->sceneParam[0];
        m_speed = ctx.config->sceneParam[1];
        m_glow = ctx.config->sceneParam[2];
        m_cloud = ctx.config->sceneParam[3];
    }

    void Build(const SceneCtx& ctx) {
        const float w = ctx.width, h = ctx.height;
        Rng rng(0xBE4711u + ctx.variation * 7919u);

        m_starsBG.Build(w, h, (int)(120 * ctx.density), 0xBE4712u + ctx.variation);

        // Molecular cloud wisps.
        m_wisps.clear();
        int wisps = (int)((7 + 9 * m_cloud) * ctx.density);
        wisps = std::min(wisps, 18);
        m_wisps.reserve((size_t)wisps);
        for (int i = 0; i < wisps; ++i) {
            Wisp wsp{};
            wsp.x = rng.Range(0.0f, w);
            wsp.y = rng.Range(0.0f, h);
            wsp.r = std::min(w, h) * rng.Range(0.08f, 0.24f);
            wsp.alpha = rng.Range(0.25f, 0.55f);
            m_wisps.push_back(wsp);
        }

        // Protostars: kept off the very edges, spaced by rejection sampling.
        m_protos.clear();
        int want = std::min(5 + (int)(m_starsAmt * 5.0f), 11);
        m_protos.reserve((size_t)want);
        int guard = 0;
        while ((int)m_protos.size() < want && guard++ < 200) {
            Proto st{};
            st.x = rng.Range(0.10f, 0.90f) * w;
            st.y = rng.Range(0.10f, 0.90f) * h;
            bool ok = true;
            for (const auto& o : m_protos) {
                float dx = o.x - st.x, dy = o.y - st.y;
                if (std::sqrt(dx * dx + dy * dy) < std::min(w, h) * 0.16f) { ok = false; break; }
            }
            if (!ok) continue;
            st.core = rng.Range(3.5f, 7.0f);
            st.spike = st.core * rng.Range(5.0f, 8.0f);
            st.ci = (int)m_protos.size() % Palette::kColors;
            st.phase = rng.Range(0.0f, kTau);
            st.twinkleF = rng.Range(0.4f, 1.3f);
            m_protos.push_back(st);
        }
    }

    std::vector<Proto> m_protos;
    std::vector<Wisp> m_wisps;
    draw::StarField m_starsBG;
    float m_time = 0;
    float m_starsAmt = 0.5f, m_speed = 0.5f, m_glow = 0.5f, m_cloud = 0.5f;
    float m_lastW = 1920, m_lastH = 1080;
};

Scene* CreateNursery() { return new NurseryScene(); }

} // namespace lp::scenes
