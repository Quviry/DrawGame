#pragma once

#include "common/protocol.h"
#include <vector>
#include <mutex>
#include <string>

class Canvas {
public:
    Canvas(int width = NProtocol::CANVAS_WIDTH, int height = NProtocol::CANVAS_HEIGHT);

    void clear(const NProtocol::Color& color = NProtocol::Color(255, 255, 255, 255));

    // Рисование штриха (серия точек связанных линиями)
    void drawStroke(const NProtocol::Color& color, uint8_t brush_size,
                    const std::vector<NProtocol::StrokePoint>& points);

    // Рисование отрезка линии кистью
    void drawLine(int x0, int y0, int x1, int y1,
                  const NProtocol::Color& color, uint8_t brush_size);

    // Рисование круглой точки
    void drawCircle(int cx, int cy, int radius, const NProtocol::Color& color);

    // Доступ к пикселям
    const NProtocol::Color* getPixels() const { return pixels_.data(); }
    NProtocol::Color* getPixelsMut() { return pixels_.data(); }

    int width() const { return width_; }
    int height() const { return height_; }

    // Сохранение в BMP
    bool saveBMP(const std::string& filename) const;

    // Загрузка из BMP
    bool loadBMP(const std::string& filename);

    std::mutex& mutex() { return mutex_; }

private:
    void setPixel(int x, int y, const NProtocol::Color& color);

    int width_, height_;
    std::vector<NProtocol::Color> pixels_;
    std::mutex mutex_;
};
