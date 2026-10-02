// LivePaper - "Atomic Model".
//
// A stylised atom: a jittering nucleus of protons and neutrons, electrons
// orbiting on tilted elliptical shells at different speeds, short trails behind
// each electron, and an occasional excitation jump between shells with a flash.
// Shell count, tilts and speeds come from the seed; colors from the palette.
#include "Scenes.h"
#include <d2d1_1.h>
#include <algorithm>
#include <vector>

namespace lp::scenes {

namespace {

struct Shell {
    float rx;          // semi-major, fraction of min dimension
    float squash;      // ry / rx
    float tilt;        // degrees
    float speed;       // rad/s, signed
    int electrons;     // electrons on this shell
    int ci;
};

struct Nucleon {
    float ox, oy;      // offset from nucleus centre
    int ci;
    float phase;
};

struct Electron {
    int shell;
    float a;           // orbital angle
    float jump;        // 0 = on shell; >0 = animating to the next shell out
    float jumpFrom;
};

} // namespace

class AtomScene final : public Scene {
public:
    const wchar_t* Name() const override { return L"Atomic Model"; }
    const wchar_t* Description() const override {
        return L"Electrons orbiting a jittering nucleus across tilted shells.";
    }
    bool SupportsCustomColor() const override { return true; }
    int ParamCount() const override { return 4; }
    const wchar_t* ParamName(int i) const override {
        switch (i) {
            case 0: return L"Shells";
            case 1: return L"Speed";
            case 2: return L"Glow";
            case 3: return L"Nucleus";
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
        m_time += dt;

        Rng rng((uint32_t)(m_time * 521.0f) + 9u + ctx.variation);
        for (auto& e : m_electrons) {
            const Shell& sh = m_shells[std::min(e.shell, (int)m_shells.size() - 1)];
            e.a += sh.speed * (0.35f + 1.3f * m_speed) * dt;
            // Excitation: occasionally an electron starts a jump outward/inward.
            if (e.jump <= 0.0f && rng.Unit() < dt * (0.05f + 0.20f * m_speed)) {
                e.jump = 0.0001f;
                e.jumpFrom = (float)e.shell;
                e.shell = (e.shell + 1) % std::max(1, (int)m_shells.size());
            }
            if (e.jump > 0.0f) {
                e.jump += dt * 2.2f;
                if (e.jump >= 1.0f) { e.jump = 0.0f; e.jumpFrom = (float)e.shell; }
            }
        }
    }

    void Draw(const SceneCtx& ctx) override {
        ID2D1DeviceContext* dc = ctx.dc;
        ID2D1SolidColorBrush* brush = ctx.white;
        const float w = ctx.width, h = ctx.height;
        const float cx = w * 0.5f, cy = h * 0.5f;
        const float unit = std::min(w, h) * 0.5f;
        Palette pal = MakePalette(ctx);
        float bh = 0, bs = 0, bv = 0;
        pal.c[0].ToHsv(bh, bs, bv);

        D2D1_GRADIENT_STOP bg[2];
        bg[0].position = 0.0f; bg[0].color = D2D1::ColorF(0.004f, 0.005f, 0.010f, 1);
        bg[1].position = 1.0f; bg[1].color = D2D1::ColorF(0.002f, 0.003f, 0.006f, 1);
        draw::VerticalGradient(dc, w, h, bg, 2);

        // Shells: tilted elliptical orbit rings.
        for (int i = 0; i < (int)m_shells.size(); ++i) {
            const Shell& sh = m_shells[i];
            float rx = sh.rx * unit;
            float ry = rx * sh.squash;
            float breathe = 1.0f + 0.03f * std::sin(m_time * 0.8f + i);
            dc->SetTransform(D2D1::Matrix3x2F::Rotation(sh.tilt, D2D1::Point2F(cx, cy)));
            brush->SetColor(D2D1::ColorF(pal.c[sh.ci].r, pal.c[sh.ci].g,
                                         pal.c[sh.ci].b, 0.28f + 0.22f * m_glow));
            dc->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx * breathe, ry * breathe),
                            brush, 1.6f, nullptr);

            // Electrons on this shell.
            for (int e = 0; e < sh.electrons; ++e) {
                // Find the matching Electron record (shells have 1-2 electrons).
                float a = 0, rr = 1.0f;
                int found = 0;
                for (const auto& el : m_electrons) {
                    if (el.shell != i) continue;
                    if (found++ == e) { a = el.a; rr = JumpRadius(el, i); break; }
                }
                float ex = cx + std::cos(a) * rx * breathe * rr;
                float ey = cy + std::sin(a) * rx * breathe * rr * sh.squash;
                draw::RadialGlow(dc, brush, ex, ey, 16.0f,
                                 pal.c[(i + 2) % Palette::kColors].WithAlpha(0.5f), 1.0f, 6);
                draw::Circle(dc, brush, ex, ey, 3.4f, Color(0.92f, 0.97f, 1.0f, 0.95f));
            }
            dc->SetTransform(D2D1::Matrix3x2F::Identity());
        }

        // Nucleus: a jittering cluster of protons and neutrons.
        const float nucR = std::min(w, h) * Lerp(0.030f, 0.055f, m_nucleus);
        float jx = std::sin(m_time * 2.3f) * nucR * 0.06f;
        float jy = std::cos(m_time * 1.9f) * nucR * 0.06f;
        draw::RadialGlow(dc, brush, cx + jx, cy + jy, nucR * 3.2f,
                         pal.c[3].WithAlpha(0.30f + 0.25f * m_glow), 1.0f, 12);
        for (const auto& n : m_nucleons) {
            float wob = std::sin(m_time * 3.1f + n.phase) * nucR * 0.08f;
            draw::Circle(dc, brush, cx + jx + n.ox * nucR + wob,
                         cy + jy + n.oy * nucR, nucR * 0.34f, pal.c[n.ci].WithAlpha(0.95f));
        }

        D2D1_GRADIENT_STOP vig[2];
        vig[0].position = 0.0f; vig[0].color = D2D1::ColorF(0, 0, 0, 0);
        vig[1].position = 1.0f; vig[1].color = D2D1::ColorF(0, 0, 0, 0.40f);
        draw::VerticalGradient(dc, w, h, vig, 2);
    }

private:
    void ReadParams(const SceneCtx& ctx) {
        m_shellsAmt = ctx.config->sceneParam[0];
        m_speed = ctx.config->sceneParam[1];
        m_glow = ctx.config->sceneParam[2];
        m_nucleus = ctx.config->sceneParam[3];
    }

    // Radius multiplier while mid-jump: eased out-and-in toward the target shell.
    float JumpRadius(const Electron& e, int fallbackShell) const {
        if (e.jump <= 0.0f) return 1.0f;
        const Shell& from = m_shells[std::min((int)e.jumpFrom, (int)m_shells.size() - 1)];
        const Shell& to = m_shells[std::min(e.shell, (int)m_shells.size() - 1)];
        float t = Smoothstep(Clamp01(e.jump));
        float rf = from.rx / std::max(1.0f, m_shells[0].rx);
        float rt = to.rx / std::max(1.0f, m_shells[0].rx);
        return Lerp(rf, rt, t) / std::max(0.001f, (fallbackShell == e.shell ? rt : rf));
    }

    void Build(const SceneCtx& ctx) {
        const float w = ctx.width, h = ctx.height;
        Rng rng(0xA7041u + ctx.variation * 7919u);

        m_shells.clear();
        int want = std::min(2 + (int)(m_shellsAmt * 3.4f), 5);
        m_shells.reserve((size_t)want);
        for (int i = 0; i < want; ++i) {
            Shell sh{};
            float t = (float)(i + 1) / want;
            sh.rx = 0.22f + 0.62f * t;
            sh.squash = rng.Range(0.30f, 0.62f);
            sh.tilt = rng.Range(-40.0f, 40.0f) + (float)i * rng.Range(-14.0f, 14.0f);
            sh.speed = (i % 2 == 0 ? 1.0f : -1.0f) * Lerp(0.5f, 1.4f, rng.Unit()) / t;
            sh.electrons = (i == 0) ? 2 : 1 + (rng.Unit() < 0.4f ? 1 : 0);
            sh.ci = i % Palette::kColors;
            m_shells.push_back(sh);
        }

        m_electrons.clear();
        for (int i = 0; i < (int)m_shells.size(); ++i) {
            for (int e = 0; e < m_shells[(size_t)i].electrons; ++e) {
                Electron el{};
                el.shell = i;
                el.a = kTau * ((float)e / m_shells[(size_t)i].electrons)
                     + rng.Range(0.0f, kTau);
                el.jump = 0.0f;
                el.jumpFrom = (float)i;
                m_electrons.push_back(el);
            }
        }

        // Nucleus: alternating proton/neutron cluster.
        m_nucleons.clear();
        int count = 7 + rng.Int(0, 4);
        m_nucleons.reserve((size_t)count);
        for (int i = 0; i < count; ++i) {
            Nucleon n{};
            float a = kTau * (float)i / count + rng.Range(-0.3f, 0.3f);
            float rr = rng.Range(0.25f, 0.70f);
            n.ox = std::cos(a) * rr;
            n.oy = std::sin(a) * rr;
            n.ci = (i % 2 == 0) ? 0 : 4;
            n.phase = rng.Range(0.0f, kTau);
            m_nucleons.push_back(n);
        }
    }

    std::vector<Shell> m_shells;
    std::vector<Electron> m_electrons;
    std::vector<Nucleon> m_nucleons;
    float m_time = 0;
    float m_shellsAmt = 0.5f, m_speed = 0.5f, m_glow = 0.5f, m_nucleus = 0.5f;
    float m_lastW = 1920, m_lastH = 1080;
};

Scene* CreateAtom() { return new AtomScene(); }

} // namespace lp::scenes
