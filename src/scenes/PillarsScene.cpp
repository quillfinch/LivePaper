// LivePaper - "Pillars of Creation".
//
// Towering columns of cold gas rising off a star field, in the spirit of the
// Eagle Nebula. Each pillar is a noise-edged silhouette built once ( Configure ),
// backlit by a bright rim on one side, with ionization glow at its tip. The
// whole field billows very slowly and takes its colors from the shared palette.
#include "Scenes.h"
#include <d2d1_1.h>
#include <algorithm>
#include <vector>

namespace lp::scenes {

namespace {

struct Pillar {
    std::vector<D2D1_POINT_2F> body;   // left edge down->up, right edge up->down
    float tipX, tipY;
    float sway;                        // px amplitude of the slow billow
    float swayF;
    float phase;
    int rimCi, tipCi;
    float width;
};

} // namespace

class PillarsScene final : public Scene {
public:
    const wchar_t* Name() const override { return L"Pillars of Creation"; }
    const wchar_t* Description() const override {
        return L"Gas columns rising off a star field, backlit and billowing.";
    }
    bool SupportsCustomColor() const override { return true; }
    int ParamCount() const override { return 4; }
    const wchar_t* ParamName(int i) const override {
        switch (i) {
            case 0: return L"Columns";
            case 1: return L"Speed";
            case 2: return L"Glow";
            case 3: return L"Haze";
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
        m_time += dt * (0.2f + 0.6f * m_speed);
    }

    void Draw(const SceneCtx& ctx) override {
        ID2D1DeviceContext* dc = ctx.dc;
        ID2D1SolidColorBrush* brush = ctx.white;
        const float w = ctx.width, h = ctx.height;
        Palette pal = MakePalette(ctx);
        float bh = 0, bs = 0, bv = 0;
        pal.c[0].ToHsv(bh, bs, bv);

        // Deep space with a faint emission haze behind the pillars.
        Color haze = Color::Hsv(Fract(bh + 0.04f), bs * 0.55f, 0.08f + 0.05f * m_haze);
        D2D1_GRADIENT_STOP bg[2];
        bg[0].position = 0.0f; bg[0].color = D2D1::ColorF(0.004f, 0.004f, 0.010f, 1);
        bg[1].position = 1.0f; bg[1].color = D2D1::ColorF(haze.r, haze.g, haze.b, 1);
        draw::VerticalGradient(dc, w, h, bg, 2);
        m_stars.Draw(dc, brush, m_time);
        draw::RadialGlow(dc, brush, w * 0.30f, h * 0.30f, w * 0.5f,
                         haze.WithAlpha(0.10f + 0.06f * m_haze), 1.0f, 14);

        // Pillars, near (largest) first so farther ones peek from behind.
        for (size_t i = 0; i < m_pillars.size(); ++i) {
            const Pillar& p = m_pillars[i];
            float swayX = std::sin(m_time * p.swayF + p.phase) * p.sway;
            dc->SetTransform(D2D1::Matrix3x2F::Translation(swayX, 0));

            Color body = pal.c[2 % Palette::kColors];
            float bh2 = 0, bs2 = 0, bv2 = 0;
            body.ToHsv(bh2, bs2, bv2);
            body = Color::Hsv(bh2, bs2, Lerp(0.16f, 0.07f, (float)i / std::max<size_t>(1, m_pillars.size() - 1)));
            draw::FillPolygon(dc, brush, p.body.data(), (unsigned)p.body.size(),
                              body.WithAlpha(0.92f));

            // Backlit rim along the right edge (light comes from off-frame).
            dc->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_ADD);
            size_t mid = p.body.size() / 2;
            for (size_t k = 1; k < mid; ++k) {
                draw::Line(dc, brush, p.body[k - 1].x, p.body[k - 1].y,
                           p.body[k].x, p.body[k].y, p.width * 0.20f,
                           pal.c[p.rimCi].WithAlpha(0.16f + 0.12f * m_glow));
            }
            // Ionization glow at the tip.
            draw::RadialGlow(dc, brush, p.tipX, p.tipY, p.width * 2.2f,
                             pal.c[p.tipCi].WithAlpha(0.20f + 0.20f * m_glow), 1.0f, 10);
            draw::Circle(dc, brush, p.tipX, p.tipY, p.width * 0.30f,
                         pal.c[p.tipCi].WithAlpha(0.55f));
            dc->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
            dc->SetTransform(D2D1::Matrix3x2F::Identity());
        }

        D2D1_GRADIENT_STOP vig[2];
        vig[0].position = 0.0f; vig[0].color = D2D1::ColorF(0, 0, 0, 0);
        vig[1].position = 1.0f; vig[1].color = D2D1::ColorF(0, 0, 0, 0.36f);
        draw::VerticalGradient(dc, w, h, vig, 2);
    }

private:
    void ReadParams(const SceneCtx& ctx) {
        m_columns = ctx.config->sceneParam[0];
        m_speed = ctx.config->sceneParam[1];
        m_glow = ctx.config->sceneParam[2];
        m_haze = ctx.config->sceneParam[3];
    }

    void Build(const SceneCtx& ctx) {
        const float w = ctx.width, h = ctx.height;
        Rng rng(0xD118C3u + ctx.variation * 7919u);

        m_stars.Build(w, h, (int)(170 * ctx.density), 0xD118C4u + ctx.variation);

        m_pillars.clear();
        int want = std::min(3 + (int)(m_columns * 3.0f), 6);
        m_pillars.reserve((size_t)want);
        for (int i = 0; i < want; ++i) {
            Pillar p{};
            float depth = (float)i / std::max(1, want - 1);   // 0 near .. 1 far
            p.width = Lerp(w * 0.16f, w * 0.075f, depth) * rng.Range(0.8f, 1.25f);
            float baseX = w * (0.10f + 0.80f * (float)i / std::max(1, want - 1))
                        + rng.Range(-w * 0.05f, w * 0.05f);
            float tipY = h * (0.16f + 0.30f * rng.Unit());
            p.tipX = baseX + p.width * 0.5f;
            p.tipY = tipY;

            // Left edge bottom->top with noise, right edge back down.
            const int steps = 14;
            float phase = rng.Range(0.0f, 60.0f);
            std::vector<D2D1_POINT_2F> left, right;
            for (int k = 0; k <= steps; ++k) {
                float t = (float)k / steps;
                float y = h * 1.02f + (tipY - h * 1.02f) * t;
                float pinch = std::sin(t * kPi) * 0.35f + 0.65f;   // narrower at the tip
                float n = Noise1(phase + t * 4.0f) * 0.30f;
                float halfL = p.width * (0.5f + n) * pinch;
                left.push_back(D2D1::Point2F(baseX - halfL, y));
                right.push_back(D2D1::Point2F(baseX + halfL, y));
            }
            p.body = left;
            for (int k = steps; k >= 0; --k) p.body.push_back(right[(size_t)k]);

            p.sway = rng.Range(4.0f, 14.0f);
            p.swayF = rng.Range(0.10f, 0.25f);
            p.phase = rng.Range(0.0f, kTau);
            p.rimCi = (i + 1) % Palette::kColors;
            p.tipCi = i % Palette::kColors;
            m_pillars.push_back(p);
        }
    }

    std::vector<Pillar> m_pillars;
    draw::StarField m_stars;
    float m_time = 0;
    float m_columns = 0.5f, m_speed = 0.5f, m_glow = 0.5f, m_haze = 0.5f;
    float m_lastW = 1920, m_lastH = 1080;
};

Scene* CreatePillars() { return new PillarsScene(); }

} // namespace lp::scenes
