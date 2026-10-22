#include "ParticleManager.h"
#include <cmath>

namespace DAFlash {

constexpr float PI = 3.14159265358979323846f;

ParticleManager::ParticleManager() {
    Initialize(1024);
}

void ParticleManager::Initialize(size_t maxParticles) {
    particles_.resize(maxParticles);
    activeCount_ = 0;
    lastAllocIndex_ = 0;
    for (auto& p : particles_) {
        p.active = false;
    }
}

void ParticleManager::Clear() {
    for (auto& p : particles_) {
        p.active = false;
    }
    activeCount_ = 0;
    lastAllocIndex_ = 0;
}

Particle* ParticleManager::AllocParticle() {
    size_t total = particles_.size();
    for (size_t i = 0; i < total; ++i) {
        size_t idx = (lastAllocIndex_ + i) % total;
        if (!particles_[idx].active) {
            lastAllocIndex_ = (idx + 1) % total;
            particles_[idx].active = true;
            activeCount_++;
            return &particles_[idx];
        }
    }
    // If pool is completely saturated, recycle the oldest particle at lastAllocIndex_
    Particle* p = &particles_[lastAllocIndex_];
    lastAllocIndex_ = (lastAllocIndex_ + 1) % total;
    return p;
}

D2D1_COLOR_F ParticleManager::PickThemeColor(ParticleTheme theme, bool isSpecial) {
    float r = RandomFloat(0.0f, 1.0f);
    switch (theme) {
    case ParticleTheme::Cyberpunk:
        if (isSpecial) {
            return MakeColorHex(0xFF1493, 0.95f); // Deep neon pink
        }
        return (r > 0.5f) ? MakeColorHex(0x00F5FF, 0.9f)  // Cyan / Electric Blue
                          : MakeColorHex(0xFF007F, 0.9f); // Bright Magenta
    case ParticleTheme::Golden:
        if (isSpecial) {
            return MakeColorHex(0xFFFFFF, 1.0f); // Pure white sparkle
        }
        return (r > 0.5f) ? MakeColorHex(0xFFD700, 0.95f) // Gold
                          : MakeColorHex(0xFFA500, 0.9f);  // Amber Orange
    case ParticleTheme::Rainbow: {
        float hue = RandomFloat(0.0f, 6.0f);
        float x = 1.0f - fabsf(fmodf(hue, 2.0f) - 1.0f);
        if (hue < 1.0f) return MakeColor(1.0f, x, 0.0f);
        if (hue < 2.0f) return MakeColor(x, 1.0f, 0.0f);
        if (hue < 3.0f) return MakeColor(0.0f, 1.0f, x);
        if (hue < 4.0f) return MakeColor(0.0f, x, 1.0f);
        if (hue < 5.0f) return MakeColor(x, 0.0f, 1.0f);
        return MakeColor(1.0f, 0.0f, x);
    }
    case ParticleTheme::Classic:
    default:
        return (r > 0.5f) ? MakeColorHex(0x40E0D0, 0.85f) // Turquoise
                          : MakeColorHex(0x87CEFA, 0.85f); // Light Sky Blue
    }
}

void ParticleManager::EmitCommitText(
    const std::wstring& text, float startX, float startY, float width, float height,
    const EngineConfig& config, bool isCombo) {

    if (text.empty()) return;

    // 1. 发射汉字粒子 (TextGlyph Particles)
    float charSpacing = 24.0f * config.text_scale;
    float totalTextWidth = text.length() * charSpacing;
    float currentX = startX + (width > 0 ? (width - totalTextWidth) * 0.5f : 0.0f);

    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];
        if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
            currentX += charSpacing * 0.5f;
            continue;
        }

        Particle* p = AllocParticle();
        if (!p) break;

        p->type = ParticleType::TextGlyph;
        p->character = ch;
        p->x = currentX;
        p->y = startY;
        
        // 微小向外扩散的初速度，随后受到强烈反重力升空
        p->vx = RandomFloat(-35.0f, 35.0f);
        p->vy = RandomFloat(-60.0f, -20.0f);
        p->size = 22.0f * config.text_scale;
        p->scale = 1.0f;
        p->rotation = RandomFloat(-0.15f, 0.15f);
        p->rot_speed = RandomFloat(-0.8f, 0.8f);
        p->color = PickThemeColor(config.theme, true);
        p->max_life = config.text_lifetime * (isCombo ? 1.3f : 1.0f);
        p->life = p->max_life;
        p->alpha = 1.0f;
        p->is_falling = false;

        currentX += charSpacing;
    }

    // 2. 发射围绕候选词的 360° 爆破火花粒子 (Spark Burst)
    uint32_t sparkCount = isCombo ? config.commit_spark_count * 2 : config.commit_spark_count;
    float centerX = startX + (width > 0 ? width * 0.5f : totalTextWidth * 0.5f);
    float centerY = startY + (height > 0 ? height * 0.5f : 12.0f);

    for (uint32_t i = 0; i < sparkCount; ++i) {
        Particle* p = AllocParticle();
        if (!p) break;

        float angle = (i * (2.0f * PI / sparkCount)) + RandomFloat(-0.25f, 0.25f);
        float speed = RandomFloat(0.4f, 1.25f) * config.commit_burst_speed * (isCombo ? 1.4f : 1.0f);

        p->type = ParticleType::Spark;
        p->character = L'\0';
        p->x = centerX + cosf(angle) * RandomFloat(5.0f, 15.0f);
        p->y = centerY + sinf(angle) * RandomFloat(5.0f, 15.0f);
        p->vx = cosf(angle) * speed;
        p->vy = sinf(angle) * speed;
        p->size = RandomFloat(2.5f, isCombo ? 6.5f : 5.0f);
        p->scale = 1.0f;
        p->rotation = RandomFloat(0.0f, 2.0f * PI);
        p->rot_speed = RandomFloat(-3.0f, 3.0f);
        p->color = PickThemeColor(config.theme, (i % 3 == 0));
        p->max_life = config.spark_lifetime * RandomFloat(0.8f, 1.3f);
        p->life = p->max_life;
        p->alpha = 1.0f;
        p->is_falling = false;
    }
}

void ParticleManager::EmitTypingBubble(float x, float y, const EngineConfig& config, bool isCombo) {
    uint32_t count = isCombo ? 6 : 3;
    for (uint32_t i = 0; i < count; ++i) {
        Particle* p = AllocParticle();
        if (!p) break;

        p->type = ParticleType::Spark;
        p->character = L'\0';
        p->x = x + RandomFloat(-12.0f, 12.0f);
        p->y = y + RandomFloat(-6.0f, 6.0f);
        p->vx = RandomFloat(-30.0f, 30.0f);
        p->vy = RandomFloat(-140.0f, -60.0f); // 向上升腾
        p->size = RandomFloat(3.5f, isCombo ? 7.0f : 5.5f);
        p->scale = 1.0f;
        p->rotation = 0.0f;
        p->rot_speed = 0.0f;
        p->color = PickThemeColor(config.theme, (i % 2 == 0));
        p->max_life = RandomFloat(0.6f, 1.1f);
        p->life = p->max_life;
        p->alpha = 0.95f;
        p->is_falling = false;
    }
}

void ParticleManager::EmitBackspaceDrop(float x, float y, const EngineConfig& config) {
    uint32_t count = 8;
    for (uint32_t i = 0; i < count; ++i) {
        Particle* p = AllocParticle();
        if (!p) break;

        p->type = ParticleType::Spark;
        p->character = L'\0';
        p->x = x + RandomFloat(-12.0f, 12.0f);
        p->y = y + RandomFloat(-6.0f, 6.0f);
        p->vx = RandomFloat(-45.0f, 45.0f);
        p->vy = RandomFloat(20.0f, 90.0f); // 向下掉落
        p->size = RandomFloat(3.5f, 6.0f);
        p->scale = 1.0f;
        p->rotation = 0.0f;
        p->rot_speed = 0.0f;
        p->color = (i % 2 == 0) ? MakeColorHex(0xDC143C, 0.95f)  // Crimson Red
                                : MakeColorHex(0xFF4500, 0.9f);   // Orange Red Ember
        p->max_life = RandomFloat(0.5f, 0.9f);
        p->life = p->max_life;
        p->alpha = 0.95f;
        p->is_falling = true;
    }
}

void ParticleManager::Update(float dt, const EngineConfig& config) {
    if (dt <= 0.0f || dt > 0.1f) {
        dt = 0.016f; // Clamp to ~60fps step if lag occurs
    }

    size_t count = 0;
    for (auto& p : particles_) {
        if (!p.active) continue;

        if (p.is_falling) {
            // 退格删除：受正向重力向下沉坠
            p.vy += 800.0f * dt;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
        } else {
            // 反重力：Y轴向上加速 (在 Windows 屏幕坐标系中，Y 朝下，因此 vy 不断减小变负)
            p.vy -= config.gravity * dt;
            
            // 水平气泡与风阻正弦扰动
            float drift = sinf(p.life * 7.5f) * config.wind_drift;
            p.x += (p.vx + drift) * dt;
            p.y += p.vy * dt;
        }

        // 旋转与寿命消减
        p.rotation += p.rot_speed * dt;
        p.life -= dt;

        if (p.life <= 0.0f) {
            p.active = false;
        } else {
            float progress = p.life / p.max_life;
            p.alpha = (std::max)(0.0f, (std::min)(1.0f, progress));
            
            // 汉字粒子逐渐轻微膨胀升空
            if (p.type == ParticleType::TextGlyph) {
                p.scale = 1.0f + (1.0f - progress) * 0.4f;
            }
            count++;
        }
    }
    activeCount_ = count;
}

} // namespace DAFlash

