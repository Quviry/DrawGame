#pragma once

#include "common/protocol.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(int canvas_w, int canvas_h, const std::string& title = "DrawGame");
    void shutdown();

    // Обновление текстуры холста
    void updateTexture(const NProtocol::Color* pixels);

    // Отрисовка кадра (холст + UI)
    void render(const NProtocol::Color& current_color, uint8_t brush_size,
                bool is_connected, int client_count);

    GLFWwindow* window() const { return window_; }

    // Координаты мыши в пространстве холста
    bool getCanvasMousePos(int& x, int& y) const;

    // Размеры UI
    static constexpr int UI_PANEL_HEIGHT = 60;
    static constexpr int WINDOW_EXTRA_W = 0;

    private:
    void renderQuad();
    void renderUI(const NProtocol::Color& current_color, uint8_t brush_size,
        bool is_connected, int client_count);
    void updateScaleFactor();

    GLFWwindow* window_ = nullptr;
    GLuint texture_ = 0;
    int canvas_w_ = 0, canvas_h_ = 0;
    int window_w_ = 0, window_h_ = 0;

    int fb_width_ = 0, fb_height_ = 0;     // Размер фреймбуфера в пикселях
    float scale_x_ = 1.0f, scale_y_ = 1.0f; // Масштаб: fb / window
};
