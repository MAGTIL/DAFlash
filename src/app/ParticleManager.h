#pragma once

#include "Particle.h"
#include "Config.h"
#include <vector>
#include <string>

namespace DAFlash {

class ParticleManager {
public:
    ParticleManager();
    ~ParticleManager() = default;

    void Initialize(size_t maxParticles = 1024);
    void Update(float dt, const EngineConfig& config);
    void Clear();

    // 发射器接口
    void EmitCommitText(const std::wstring& text, float startX, float startY, float width, float height,
                        const EngineConfig& config, bool isCombo = false);
    void EmitTypingBubble(float x, float y, const EngineConfig& config, bool isCombo = false);
    void EmitBackspaceDrop(float x, float y, const EngineConfig& config);

    const std::vector<Particle>& GetParticles() const { return particles_; }
    size_t GetActiveCount() const { return activeCount_; }

private:
    Particle* AllocParticle();
    D2D1_COLOR_F PickThemeColor(ParticleTheme theme, bool isSpecial = false);

    std::vector<Particle> particles_;
    size_t activeCount_ = 0;
    size_t lastAllocIndex_ = 0;
};

} // namespace DAFlash
