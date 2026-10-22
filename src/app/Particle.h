#pragma once

#include "Common.h"

namespace DAFlash {

enum class ParticleType {
    Spark,      // 发光粒子/气泡/火花
    TextGlyph   // 真实汉字字符
};

struct Particle {
    ParticleType type = ParticleType::Spark;
    wchar_t character = L'\0'; // 若是汉字粒子，具体的 unicode 字符
    
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    
    float size = 4.0f;          // 火花半径或基准字号
    float scale = 1.0f;         // 缩放尺寸
    float rotation = 0.0f;      // 旋转弧度
    float rot_speed = 0.0f;     // 角速度
    
    float alpha = 1.0f;         // 当前不透明度
    D2D1_COLOR_F color = {1.0f, 1.0f, 1.0f, 1.0f};
    
    float life = 0.0f;          // 剩余寿命
    float max_life = 1.0f;      // 最大寿命
    
    bool active = false;
    bool is_falling = false;    // 是否受正向重力（如退格删除时下坠）
};

} // namespace DAFlash
