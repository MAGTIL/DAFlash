#include "D2DRenderer.h"

namespace DAFlash {

D2DRenderer::D2DRenderer() = default;

D2DRenderer::~D2DRenderer() {
    Cleanup();
}

void D2DRenderer::Cleanup() {
    if (pBrush_) { pBrush_->Release(); pBrush_ = nullptr; }
    if (pTextFormat_) { pTextFormat_->Release(); pTextFormat_ = nullptr; }
    if (pRenderTarget_) { pRenderTarget_->Release(); pRenderTarget_ = nullptr; }
    if (pDWriteFactory_) { pDWriteFactory_->Release(); pDWriteFactory_ = nullptr; }
    if (pD2DFactory_) { pD2DFactory_->Release(); pD2DFactory_ = nullptr; }
}

bool D2DRenderer::Initialize(HWND hWnd, int width, int height) {
    hWnd_ = hWnd;
    width_ = width;
    height_ = height;

    LogApp("D2DRenderer::Initialize: hWnd=" + std::to_string((uintptr_t)hWnd) + " w=" + std::to_string(width) + " h=" + std::to_string(height));

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory_);
    LogApp("D2D1CreateFactory hr=0x" + std::to_string(hr));
    if (FAILED(hr) || !pD2DFactory_) return false;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&pDWriteFactory_)
    );
    LogApp("DWriteCreateFactory hr=0x" + std::to_string(hr));
    if (FAILED(hr) || !pDWriteFactory_) return false;

    // 创建中文高质量字形排版格式
    hr = pDWriteFactory_->CreateTextFormat(
        L"Microsoft YaHei UI",
        nullptr,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        22.0f,
        L"zh-cn",
        &pTextFormat_
    );
    LogApp("CreateTextFormat hr=0x" + std::to_string(hr));
    if (FAILED(hr) || !pTextFormat_) return false;

    pTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    pTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // 建立硬件渲染目标 (标准 HWND 颜色键透明模式)
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)
    );

    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
        hWnd_,
        D2D1::SizeU(width_, height_),
        D2D1_PRESENT_OPTIONS_IMMEDIATELY
    );

    LogApp("Calling CreateHwndRenderTarget...");
    hr = pD2DFactory_->CreateHwndRenderTarget(rtProps, hwndProps, &pRenderTarget_);
    LogApp("CreateHwndRenderTarget returned hr=0x" + std::to_string(hr));
    if (FAILED(hr) || !pRenderTarget_) return false;

    // 锁定 96 DPI，确保 Direct2D 坐标系与 Windows 全局物理像素 1:1 精确对应，不受跨屏缩放失真影响
    pRenderTarget_->SetDpi(96.0f, 96.0f);
    LogApp("SetDpi completed");

    // 预分配全局画刷，严禁在渲染循环中重复 new 画刷
    hr = pRenderTarget_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &pBrush_);
    LogApp("CreateSolidColorBrush hr=0x" + std::to_string(hr));
    return SUCCEEDED(hr);
}

void D2DRenderer::Resize(int width, int height) {
    width_ = width;
    height_ = height;
    if (pRenderTarget_) {
        pRenderTarget_->Resize(D2D1::SizeU(width_, height_));
        pRenderTarget_->SetDpi(96.0f, 96.0f);
    }
}

void D2DRenderer::BeginDraw() {
    if (pRenderTarget_) {
        pRenderTarget_->BeginDraw();
    }
}

void D2DRenderer::Clear() {
    if (pRenderTarget_) {
        // 纯黑清屏，配合 SetLayeredWindowAttributes 的 LWA_COLORKEY 成为 100% 透明
        pRenderTarget_->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));
    }
}

void D2DRenderer::DrawParticles(const std::vector<Particle>& particles, float shakeOffsetX, float shakeOffsetY) {
    if (!pRenderTarget_ || !pBrush_) return;

    for (const auto& p : particles) {
        if (!p.active) continue;

        float x = p.x + shakeOffsetX;
        float y = p.y + shakeOffsetY;

        float r = (std::max)(p.color.r, 0.06f);
        float g = (std::max)(p.color.g, 0.06f);
        float b = (std::max)(p.color.b, 0.06f);

        if (p.type == ParticleType::Spark) {
            // 火花 / 气泡粒子
            pBrush_->SetColor(D2D1::ColorF(r, g, b, p.alpha));
            D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(x, y), p.size * p.scale, p.size * p.scale);
            pRenderTarget_->FillEllipse(ellipse, pBrush_);
        } else if (p.type == ParticleType::TextGlyph && p.character != L'\0') {
            // 真实反重力汉字字符粒子
            WCHAR str[2] = { p.character, L'\0' };
            D2D1_MATRIX_3X2_F oldTransform;
            pRenderTarget_->GetTransform(&oldTransform);

            // 应用旋转与缩放矩阵变换
            D2D1_MATRIX_3X2_F rot = D2D1::Matrix3x2F::Rotation(p.rotation * (180.0f / 3.14159265f), D2D1::Point2F(x, y));
            D2D1_MATRIX_3X2_F scale = D2D1::Matrix3x2F::Scale(p.scale, p.scale, D2D1::Point2F(x, y));
            pRenderTarget_->SetTransform(rot * scale * oldTransform);

            // 1. 文字微光外发光/阴影
            pBrush_->SetColor(D2D1::ColorF(0.15f, 0.15f, 0.25f, p.alpha));
            D2D1_RECT_F shadowRect = D2D1::RectF(x - 20.0f + 1.5f, y - 20.0f + 1.5f, x + 20.0f + 1.5f, y + 20.0f + 1.5f);
            pRenderTarget_->DrawText(str, 1, pTextFormat_, shadowRect, pBrush_);

            // 2. 主体汉字绘制
            pBrush_->SetColor(D2D1::ColorF(r, g, b, p.alpha));
            D2D1_RECT_F textRect = D2D1::RectF(x - 20.0f, y - 20.0f, x + 20.0f, y + 20.0f);
            pRenderTarget_->DrawText(str, 1, pTextFormat_, textRect, pBrush_);

            pRenderTarget_->SetTransform(oldTransform);
        }
    }
}

bool D2DRenderer::EndDraw() {
    if (!pRenderTarget_) return false;
    HRESULT hr = pRenderTarget_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        Cleanup();
        Initialize(hWnd_, width_, height_);
        return false;
    }
    return SUCCEEDED(hr);
}

} // namespace DAFlash

