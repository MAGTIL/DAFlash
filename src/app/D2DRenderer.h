#pragma once

#include "Common.h"
#include "Particle.h"
#include <vector>

namespace DAFlash {

class D2DRenderer {
public:
    D2DRenderer();
    ~D2DRenderer();

    bool Initialize(HWND hWnd, int width, int height);
    void Resize(int width, int height);
    void Cleanup();

    void BeginDraw();
    void Clear();
    void DrawParticles(const std::vector<Particle>& particles, float shakeOffsetX = 0.0f, float shakeOffsetY = 0.0f);
    bool EndDraw();

private:
    ID2D1Factory* pD2DFactory_ = nullptr;
    ID2D1HwndRenderTarget* pRenderTarget_ = nullptr;
    IDWriteFactory* pDWriteFactory_ = nullptr;
    IDWriteTextFormat* pTextFormat_ = nullptr;
    ID2D1SolidColorBrush* pBrush_ = nullptr;

    HWND hWnd_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

} // namespace DAFlash

