#include "client/renderer.h"
#include <iostream>
#include <cstdio>
#include <cmath>

Renderer::Renderer() {}

Renderer::~Renderer() {
    shutdown();
}

// === НОВАЯ ФУНКЦИЯ: определяем масштаб ===
void Renderer::updateScaleFactor() {
    int win_w, win_h;
    glfwGetWindowSize(window_, &win_w, &win_h);
    glfwGetFramebufferSize(window_, &fb_width_, &fb_height_);

    if (win_w > 0 && win_h > 0) {
        scale_x_ = static_cast<float>(fb_width_) / static_cast<float>(win_w);
        scale_y_ = static_cast<float>(fb_height_) / static_cast<float>(win_h);
    } else {
        scale_x_ = 1.0f;
        scale_y_ = 1.0f;
    }
}

bool Renderer::init(int canvas_w, int canvas_h, const std::string& title) {
    canvas_w_ = canvas_w;
    canvas_h_ = canvas_h;
    window_w_ = canvas_w + WINDOW_EXTRA_W;
    window_h_ = canvas_h + UI_PANEL_HEIGHT;

    if (!glfwInit()) {
        std::cerr << "[Renderer] GLFW init failed\n";
        return false;
    }

    // === ВАЖНО для macOS: НЕ запрашиваем конкретную версию OpenGL, ===
    // === либо запрашиваем совместимый с Legacy OpenGL профиль      ===
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // === macOS: Можно отключить Retina, но лучше корректно обработать ===
    // glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE); // раскомментировать для отключения Retina

    window_ = glfwCreateWindow(window_w_, window_h_, title.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "[Renderer] Window creation failed\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "[Renderer] GLAD init failed\n";
        return false;
    }

    // === Определяем масштаб сразу после создания контекста ===
    updateScaleFactor();

    std::cout << "[Renderer] Window: " << window_w_ << "x" << window_h_
              << ", Framebuffer: " << fb_width_ << "x" << fb_height_
              << ", Scale: " << scale_x_ << "x" << scale_y_ << "\n";

    // Создание текстуры холста
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, canvas_w, canvas_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    return true;
}

void Renderer::shutdown() {
    if (texture_) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

void Renderer::updateTexture(const NProtocol::Color* pixels) {
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, canvas_w_, canvas_h_,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

// === ИСПРАВЛЕНО: учитываем масштаб для координат мыши ===
bool Renderer::getCanvasMousePos(int& x, int& y) const {
    double mx, my;
    glfwGetCursorPos(window_, &mx, &my);

    // glfwGetCursorPos возвращает координаты в screen coordinates (логических).
    // Холст занимает область [0..canvas_w_) x [0..canvas_h_) в логических координатах.
    // НЕ нужно умножать на scale — холст определён в логических единицах.
    x = static_cast<int>(mx);
    y = static_cast<int>(my);

    return (x >= 0 && x < canvas_w_ && y >= 0 && y < canvas_h_);
}

void Renderer::renderQuad() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_);

    // Холст — верхняя часть окна
    // В NDC: window_h_ логических единиц -> [-1, 1]
    // canvas_h_ логических единиц от верха
    float top = 1.0f;
    float bottom = 1.0f - 2.0f * static_cast<float>(canvas_h_) / static_cast<float>(window_h_);

    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, top);
    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, top);
    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f, bottom);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, bottom);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

void Renderer::renderUI(const NProtocol::Color& current_color, uint8_t brush_size,
                         bool is_connected, int client_count) {
    float panel_top = 1.0f - 2.0f * static_cast<float>(canvas_h_) / static_cast<float>(window_h_);
    float panel_bottom = -1.0f;

    // Фон панели
    glBegin(GL_QUADS);
    glColor3f(0.2f, 0.2f, 0.2f);
    glVertex2f(-1.0f, panel_top);
    glVertex2f( 1.0f, panel_top);
    glVertex2f( 1.0f, panel_bottom);
    glVertex2f(-1.0f, panel_bottom);
    glEnd();

    // Разделительная линия
    glBegin(GL_LINES);
    glColor3f(0.5f, 0.5f, 0.5f);
    glVertex2f(-1.0f, panel_top);
    glVertex2f( 1.0f, panel_top);
    glEnd();

    // Палитра цветов
    struct PaletteColor { float r, g, b; };
    PaletteColor palette[] = {
        {0,0,0}, {1,1,1}, {1,0,0}, {0,1,0}, {0,0,1},
        {1,1,0}, {1,0,1}, {0,1,1}, {1,0.5f,0}, {0.5f,0,1},
        {0.5f,0.5f,0.5f}, {0.7f,0.3f,0.1f}
    };
    int palette_count = sizeof(palette) / sizeof(palette[0]);

    // Все координаты в логических единицах, конвертируем в NDC
    float box_size_px = 40.0f;
    float start_x_px = 10.0f + box_size_px / 2.0f;
    float center_y_px = static_cast<float>(canvas_h_) + static_cast<float>(UI_PANEL_HEIGHT) / 2.0f;

    auto px_to_ndc_x = [&](float px) -> float {
        return -1.0f + 2.0f * px / static_cast<float>(window_w_);
    };
    auto px_to_ndc_y = [&](float py) -> float {
        return 1.0f - 2.0f * py / static_cast<float>(window_h_);
    };

    float half_px = box_size_px / 2.0f * 0.8f;

    for (int i = 0; i < palette_count; i++) {
        float cx_px = start_x_px + i * box_size_px;
        float x0 = px_to_ndc_x(cx_px - half_px);
        float x1 = px_to_ndc_x(cx_px + half_px);
        float y0 = px_to_ndc_y(center_y_px - half_px);
        float y1 = px_to_ndc_y(center_y_px + half_px);

        glBegin(GL_QUADS);
        glColor3f(palette[i].r, palette[i].g, palette[i].b);
        glVertex2f(x0, y0);
        glVertex2f(x1, y0);
        glVertex2f(x1, y1);
        glVertex2f(x0, y1);
        glEnd();

        // Рамка вокруг выбранного цвета
        uint8_t pr = static_cast<uint8_t>(palette[i].r * 255);
        uint8_t pg = static_cast<uint8_t>(palette[i].g * 255);
        uint8_t pb = static_cast<uint8_t>(palette[i].b * 255);
        if (pr == current_color.r && pg == current_color.g && pb == current_color.b) {
            float bx0 = px_to_ndc_x(cx_px - half_px - 3);
            float bx1 = px_to_ndc_x(cx_px + half_px + 3);
            float by0 = px_to_ndc_y(center_y_px - half_px - 3);
            float by1 = px_to_ndc_y(center_y_px + half_px + 3);
            glLineWidth(2.0f);
            glBegin(GL_LINE_LOOP);
            glColor3f(1, 1, 0);
            glVertex2f(bx0, by0);
            glVertex2f(bx1, by0);
            glVertex2f(bx1, by1);
            glVertex2f(bx0, by1);
            glEnd();
            glLineWidth(1.0f);
        }
    }

    // Индикатор размера кисти
    float brush_cx_px = start_x_px + palette_count * box_size_px + box_size_px;
    float brush_cx = px_to_ndc_x(brush_cx_px);
    float brush_cy = px_to_ndc_y(center_y_px);
    float brush_r_px = brush_size / 2.0f;
    float brush_r_ndc_x = 2.0f * brush_r_px / static_cast<float>(window_w_);
    float brush_r_ndc_y = 2.0f * brush_r_px / static_cast<float>(window_h_);

    int segments = 32;
    glBegin(GL_TRIANGLE_FAN);
    glColor3ub(current_color.r, current_color.g, current_color.b);
    glVertex2f(brush_cx, brush_cy);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * 3.14159f * i / segments;
        glVertex2f(brush_cx + cosf(angle) * brush_r_ndc_x,
                    brush_cy + sinf(angle) * brush_r_ndc_y);
    }
    glEnd();

    // Индикатор подключения
    float ind_cx_px = static_cast<float>(window_w_) - 30.0f;
    float ind_cx = px_to_ndc_x(ind_cx_px);
    float ind_cy = px_to_ndc_y(center_y_px);
    float ind_r_x = 2.0f * 6.0f / static_cast<float>(window_w_);
    float ind_r_y = 2.0f * 6.0f / static_cast<float>(window_h_);

    glBegin(GL_TRIANGLE_FAN);
    if (is_connected) glColor3f(0, 1, 0);
    else glColor3f(1, 0, 0);
    glVertex2f(ind_cx, ind_cy);
    for (int i = 0; i <= 16; i++) {
        float angle = 2.0f * 3.14159f * i / 16;
        glVertex2f(ind_cx + cosf(angle) * ind_r_x,
                    ind_cy + sinf(angle) * ind_r_y);
    }
    glEnd();

    glColor3f(1, 1, 1);
}

void Renderer::render(const NProtocol::Color& current_color, uint8_t brush_size,
                      bool is_connected, int client_count) {
    // === КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: обновляем масштаб каждый кадр ===
    updateScaleFactor();

    // === КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: glViewport использует размер ФРЕЙМБУФЕРА ===
    glViewport(0, 0, fb_width_, fb_height_);

    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(1, 1, 1);
    renderQuad();
    renderUI(current_color, brush_size, is_connected, client_count);

    glfwSwapBuffers(window_);
}
