#pragma once

#include <cstdint>
#include <string>

namespace DAFlash {

enum class ParticleTheme {
    Cyberpunk,  // 赛博朋克 (霓虹粉 + 青蓝)
    Golden,     // 璀璨黄金 (华丽金色 + 火花)
    Classic,    // 经典冷光 (冰蓝 + 白光)
    Rainbow     // 彩虹渐变 (流光溢彩)
};

struct EngineConfig {
    bool enabled = true;
    float gravity = 650.0f;           // 反重力向上加速度 (px/s^2)
    float wind_drift = 45.0f;         // 正弦波漂移晃动幅度
    uint32_t max_particles = 1024;    // 粒子池上限
    float spark_lifetime = 1.0f;      // 火花粒子寿命 (秒)
    float text_lifetime = 1.6f;       // 汉字粒子寿命 (秒)
    float text_scale = 1.25f;         // 汉字粒子字号缩放比例
    
    // 爆发与连击机制
    uint32_t commit_spark_count = 24; // 候选字上屏爆破火花数
    float commit_burst_speed = 320.0f;// 爆炸初速度
    float combo_wpm_threshold = 55.0f;// 开启暴击连击的打字速率 (WPM)
    bool combo_shake_enabled = true;  // 是否开启连击屏幕微震

    ParticleTheme theme = ParticleTheme::Cyberpunk;
};

} // namespace DAFlash
