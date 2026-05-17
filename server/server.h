#pragma once

#include "common/canvas.h"
#include "common/protocol.h"

#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
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
    #include <fcntl.h>
    typedef int SocketHandle;
    #define INVALID_SOCK (-1)
#endif

class DrawServer {
public:
    using StrokeCallback = std::function<void(const NProtocol::Color&, uint8_t,
                                               const std::vector<NProtocol::StrokePoint>&)>;
    using ClearCallback = std::function<void()>;

    DrawServer();
    ~DrawServer();

    // Запуск сервера
    bool start(uint16_t port, bool local_only = true);

    // Остановка сервера с сохранением
    void stop(const std::string& save_file = "canvas_save.bmp");

    // Обработка штриха от локального клиента
    void localStroke(const NProtocol::Color& color, uint8_t brush_size,
                     const std::vector<NProtocol::StrokePoint>& points);

    void localClear();

    // Получить указатель на канвас
    Canvas& canvas() { return canvas_; }

    bool isRunning() const { return running_.load(); }

    uint16_t port() const { return port_; }
    int clientCount() const;

    // Коллбэки для локального клиента (получение данных от удалённых клиентов)
    void setStrokeCallback(StrokeCallback cb) { stroke_callback_ = cb; }
    void setClearCallback(ClearCallback cb) { clear_callback_ = cb; }

private:
    void acceptLoop();
    void clientLoop(SocketHandle sock, int client_id);
    void broadcastStroke(const NProtocol::Color& color, uint8_t brush_size,
                         const std::vector<NProtocol::StrokePoint>& points,
                         int exclude_client = -1);
    void broadcastClear(int exclude_client = -1);
    void sendFullCanvas(SocketHandle sock);
    bool sendAll(SocketHandle sock, const uint8_t* data, size_t size);
    bool recvAll(SocketHandle sock, uint8_t* data, size_t size);
    void removeClient(int client_id);
    void closeSocket(SocketHandle sock);

    Canvas canvas_;
    std::atomic<bool> running_{false};
    uint16_t port_ = 0;

    SocketHandle listen_sock_ = INVALID_SOCK;
    std::thread accept_thread_;

    struct ClientInfo {
        SocketHandle sock;
        int id;
        std::thread thread;
    };

    std::vector<ClientInfo*> clients_;
    std::mutex clients_mutex_;
    int next_client_id_ = 1;

    StrokeCallback stroke_callback_;
    ClearCallback clear_callback_;
};
