#include "common/canvas.h"
#include <cmath>
#include <fstream>
#include <algorithm>

Canvas::Canvas(int width, int height)
    : width_(width), height_(height), pixels_(width * height)
{
    clear();
}

void Canvas::clear(const NProtocol::Color& color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

void Canvas::setPixel(int x, int y, const NProtocol::Color& color) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        // Альфа-смешивание
        NProtocol::Color& dst = pixels_[y * width_ + x];
        if (color.a == 255) {
            dst = color;
        } else if (color.a > 0) {
            float sa = color.a / 255.0f;
            float da = 1.0f - sa;
            dst.r = static_cast<uint8_t>(color.r * sa + dst.r * da);
            dst.g = static_cast<uint8_t>(color.g * sa + dst.g * da);
            dst.b = static_cast<uint8_t>(color.b * sa + dst.b * da);
            dst.a = 255;
        }
    }
}

void Canvas::drawCircle(int cx, int cy, int radius, const NProtocol::Color& color) {
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) {
                setPixel(cx + dx, cy + dy, color);
            }
        }
    }
}

void Canvas::drawLine(int x0, int y0, int x1, int y1,
                      const NProtocol::Color& color, uint8_t brush_size) {
    int radius = std::max(0, (brush_size - 1) / 2);

    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (true) {
        drawCircle(x0, y0, radius, color);

        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void Canvas::drawStroke(const NProtocol::Color& color, uint8_t brush_size,
                        const std::vector<NProtocol::StrokePoint>& points) {
    if (points.empty()) return;

    if (points.size() == 1) {
        int radius = std::max(0, (brush_size - 1) / 2);
        drawCircle(points[0].x, points[0].y, radius, color);
        return;
    }

    for (size_t i = 1; i < points.size(); i++) {
        drawLine(points[i-1].x, points[i-1].y,
                 points[i].x, points[i].y, color, brush_size);
    }
}

bool Canvas::saveBMP(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;

    int row_size = ((width_ * 3 + 3) / 4) * 4;
    int image_size = row_size * height_;
    int file_size = 54 + image_size;

    // BMP Header
    uint8_t header[54] = {};
    header[0] = 'B'; header[1] = 'M';
    memcpy(&header[2], &file_size, 4);
    int offset = 54;
    memcpy(&header[10], &offset, 4);
    int info_size = 40;
    memcpy(&header[14], &info_size, 4);
    memcpy(&header[18], &width_, 4);
    memcpy(&header[22], &height_, 4);
    int16_t planes = 1;
    memcpy(&header[26], &planes, 2);
    int16_t bpp = 24;
    memcpy(&header[28], &bpp, 2);
    memcpy(&header[34], &image_size, 4);

    file.write(reinterpret_cast<char*>(header), 54);

    // Пиксели (BMP хранит снизу вверх, BGR)
    std::vector<uint8_t> row(row_size, 0);
    for (int y = height_ - 1; y >= 0; y--) {
        for (int x = 0; x < width_; x++) {
            const auto& c = pixels_[y * width_ + x];
            row[x * 3 + 0] = c.b;
            row[x * 3 + 1] = c.g;
            row[x * 3 + 2] = c.r;
        }
        file.write(reinterpret_cast<char*>(row.data()), row_size);
    }

    return file.good();
}

bool Canvas::loadBMP(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return false;

    uint8_t header[54];
    file.read(reinterpret_cast<char*>(header), 54);
    if (header[0] != 'B' || header[1] != 'M') return false;

    int w, h;
    int16_t bpp;
    memcpy(&w, &header[18], 4);
    memcpy(&h, &header[22], 4);
    memcpy(&bpp, &header[28], 2);

    if (w != width_ || h != height_ || bpp != 24) return false;

    int row_size = ((w * 3 + 3) / 4) * 4;
    std::vector<uint8_t> row(row_size);

    for (int y = h - 1; y >= 0; y--) {
        file.read(reinterpret_cast<char*>(row.data()), row_size);
        for (int x = 0; x < w; x++) {
            auto& c = pixels_[y * w + x];
            c.b = row[x * 3 + 0];
            c.g = row[x * 3 + 1];
            c.r = row[x * 3 + 2];
            c.a = 255;
        }
    }

    return file.good();
}
