#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

namespace NProtocol {

// Размер холста
constexpr int CANVAS_WIDTH = 800;
constexpr int CANVAS_HEIGHT = 600;
constexpr uint16_t DEFAULT_PORT = 7777;

// Типы пакетов
enum class PacketType : uint8_t {
    // Клиент -> Сервер
    DRAW_STROKE     = 0x01,  // Штрих кистью
    CLEAR_CANVAS    = 0x02,  // Очистить холст
    REQUEST_SYNC    = 0x03,  // Запрос синхронизации холста
    CLIENT_HELLO    = 0x04,  // Клиент подключился
    CLIENT_BYE      = 0x05,  // Клиент отключается

    // Сервер -> Клиент
    FULL_CANVAS     = 0x10,  // Полный холст (синхронизация)
    STROKE_BROADCAST= 0x11,  // Широковещательный штрих
    CLEAR_BROADCAST = 0x12,  // Широковещательная очистка
    SERVER_SHUTDOWN  = 0x1F,  // Сервер выключается

    // Общие
    PING            = 0xFE,
    PONG            = 0xFF
};

// Цвет пикселя
struct Color {
    uint8_t r, g, b, a;

    Color() : r(255), g(255), b(255), a(255) {}
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}

    bool operator==(const Color& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
};

// Точка штриха
struct StrokePoint {
    int16_t x, y;
};

// Заголовок пакета
struct PacketHeader {
    PacketType type;
    uint32_t payload_size;
};

constexpr size_t HEADER_SIZE = sizeof(uint8_t) + sizeof(uint32_t);

// Пакет штриха
struct StrokePacket {
    Color color;
    uint8_t brush_size;
    uint16_t point_count;
    // За ним следует массив StrokePoint
};

// Сериализация пакета
inline std::vector<uint8_t> serializeHeader(PacketType type, uint32_t payload_size) {
    std::vector<uint8_t> data(HEADER_SIZE);
    data[0] = static_cast<uint8_t>(type);
    memcpy(&data[1], &payload_size, sizeof(uint32_t));
    return data;
}

inline PacketHeader deserializeHeader(const uint8_t* data) {
    PacketHeader h;
    h.type = static_cast<PacketType>(data[0]);
    memcpy(&h.payload_size, &data[1], sizeof(uint32_t));
    return h;
}

// Сериализация штриха
inline std::vector<uint8_t> serializeStroke(const Color& color, uint8_t brush_size,
                                             const std::vector<StrokePoint>& points) {
    size_t payload_sz = sizeof(Color) + sizeof(uint8_t) + sizeof(uint16_t)
                       + points.size() * sizeof(StrokePoint);

    std::vector<uint8_t> packet;
    auto header = serializeHeader(PacketType::DRAW_STROKE, static_cast<uint32_t>(payload_sz));
    packet.insert(packet.end(), header.begin(), header.end());

    // Color
    size_t off = packet.size();
    packet.resize(packet.size() + payload_sz);
    uint8_t* p = &packet[off];

    memcpy(p, &color, sizeof(Color)); p += sizeof(Color);
    *p = brush_size; p += 1;
    uint16_t cnt = static_cast<uint16_t>(points.size());
    memcpy(p, &cnt, sizeof(uint16_t)); p += sizeof(uint16_t);
    memcpy(p, points.data(), points.size() * sizeof(StrokePoint));

    return packet;
}

inline void deserializeStroke(const uint8_t* payload, Color& color,
                               uint8_t& brush_size, std::vector<StrokePoint>& points) {
    const uint8_t* p = payload;
    memcpy(&color, p, sizeof(Color)); p += sizeof(Color);
    brush_size = *p; p += 1;
    uint16_t cnt;
    memcpy(&cnt, p, sizeof(uint16_t)); p += sizeof(uint16_t);
    points.resize(cnt);
    memcpy(points.data(), p, cnt * sizeof(StrokePoint));
}

// Сериализация полного холста (сжатие RLE)
inline std::vector<uint8_t> compressCanvas(const Color* pixels, int w, int h) {
    std::vector<uint8_t> compressed;
    int total = w * h;
    int i = 0;
    while (i < total) {
        Color c = pixels[i];
        uint16_t run = 1;
        while (i + run < total && run < 65535 && pixels[i + run] == c) {
            run++;
        }
        // Записываем: color(4 bytes) + run(2 bytes)
        compressed.push_back(c.r);
        compressed.push_back(c.g);
        compressed.push_back(c.b);
        compressed.push_back(c.a);
        compressed.push_back(static_cast<uint8_t>(run & 0xFF));
        compressed.push_back(static_cast<uint8_t>((run >> 8) & 0xFF));
        i += run;
    }
    return compressed;
}

inline void decompressCanvas(const uint8_t* data, size_t size, Color* pixels, int w, int h) {
    int total = w * h;
    int pixel_idx = 0;
    size_t off = 0;
    while (off + 6 <= size && pixel_idx < total) {
        Color c;
        c.r = data[off]; c.g = data[off+1]; c.b = data[off+2]; c.a = data[off+3];
        uint16_t run = static_cast<uint16_t>(data[off+4]) | (static_cast<uint16_t>(data[off+5]) << 8);
        off += 6;
        for (uint16_t j = 0; j < run && pixel_idx < total; j++) {
            pixels[pixel_idx++] = c;
        }
    }
}

} // namespace NProtocol
