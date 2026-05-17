#pragma once

#include "common/protocol.h"
#include "common/canvas.h"
#include "client/renderer.h"
#include "server/server.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET SocketHandle;
    #define INVALID_SOCK INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int SocketHandle;
    #define INVALID_SOCK (-1)
#endif

class DrawClient {
public:
    DrawClient();
    ~DrawClient();

    // Запуск в локальном режиме (встроенный сервер)
    bool startLocal(uint16_t port = NProtocol::DEFAULT_PORT, bool open_port = false);

    // Подключение к удалённому серверу
    bool connectRemote(const std::string& host, uint16_t port);

    // Главный цикл
    void run();

    // Остановка
    void stop();

private:
    void handleInput();
    void processNetworkEvents();
    void networkRecvLoop();

    // Отправка пакета серверу (при удалённом подключении)
    void sendStrokeToServer(const NProtocol::Color& color, uint8_t brush_size,
                             const std::vector<NProtocol::StrokePoint>& points);
    void sendClearToServer();

    bool sendAll(const uint8_t* data, size_t size);
    bool recvAll(uint8_t* data, size_t size);
    void closeSocket();

    // Выбор цвета из палитры по клику
    bool handlePaletteClick(double mx, double my);

    // Режим работы
    enum class Mode { LOCAL, REMOTE };
    Mode mode_ = Mode::LOCAL;

    // Компоненты
    Renderer renderer_;
    Canvas local_canvas_; // Локальная копия холста для отрисовки
    DrawServer* server_ = nullptr; // Только в LOCAL режиме

    // Сетевые данные (REMOTE режим)
    SocketHandle sock_ = INVALID_SOCK;
    std::thread recv_thread_;
    std::atomic<bool> connected_{false};

    // Рисование
    NProtocol::Color current_color_{0, 0, 0, 255};
    uint8_t brush_size_ = 4;
    bool drawing_ = false;
    std::vector<NProtocol::StrokePoint> current_stroke_;
    int last_x_ = -1, last_y_ = -1;

    // Очередь событий от сети
    struct NetEvent {
        enum Type { STROKE, CLEAR, FULL_CANVAS, DISCONNECT };
        Type type;
        NProtocol::Color color;
        uint8_t brush_size;
        std::vector<NProtocol::StrokePoint> points;
        std::vector<NProtocol::Color> canvas_data;
    };
    std::queue<NetEvent> net_events_;
    std::mutex net_events_mutex_;

    std::atomic<bool> running_{false};

    // Палитра
    struct PaletteEntry {
        NProtocol::Color color;
        float x_min, x_max, y_min, y_max; // в пикселях окна
    };
    std::vector<PaletteEntry> palette_entries_;
    void buildPaletteEntries();
};
