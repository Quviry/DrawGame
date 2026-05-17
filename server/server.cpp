#include "server/server.h"
#include <iostream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
static bool wsa_initialized = false;
static void initWSA() {
    if (!wsa_initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        wsa_initialized = true;
    }
}
#endif

DrawServer::DrawServer() {
#ifdef _WIN32
    initWSA();
#endif
}

DrawServer::~DrawServer() {
    if (running_.load()) {
        stop();
    }
}

void DrawServer::closeSocket(SocketHandle sock) {
    if (sock == INVALID_SOCK) return;
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

bool DrawServer::sendAll(SocketHandle sock, const uint8_t* data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
        int ret = send(sock, reinterpret_cast<const char*>(data + sent),
                       static_cast<int>(size - sent), 0);
        if (ret <= 0) return false;
        sent += ret;
    }
    return true;
}

bool DrawServer::recvAll(SocketHandle sock, uint8_t* data, size_t size) {
    size_t received = 0;
    while (received < size) {
        int ret = recv(sock, reinterpret_cast<char*>(data + received),
                       static_cast<int>(size - received), 0);
        if (ret <= 0) return false;
        received += ret;
    }
    return true;
}

bool DrawServer::start(uint16_t port, bool local_only) {
    // Попытка загрузки сохранения
    canvas_.loadBMP("canvas_save.bmp");

    listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock_ == INVALID_SOCK) {
        std::cerr << "[Server] Failed to create socket\n";
        return false;
    }

    int opt = 1;
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = local_only ? htonl(INADDR_LOOPBACK) : htonl(INADDR_ANY);

    if (bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[Server] Bind failed on port " << port << "\n";
        closeSocket(listen_sock_);
        listen_sock_ = INVALID_SOCK;
        return false;
    }

    if (listen(listen_sock_, 10) < 0) {
        std::cerr << "[Server] Listen failed\n";
        closeSocket(listen_sock_);
        listen_sock_ = INVALID_SOCK;
        return false;
    }

    port_ = port;
    running_.store(true);

    accept_thread_ = std::thread(&DrawServer::acceptLoop, this);

    std::cout << "[Server] Started on port " << port;
    if (local_only) std::cout << " (local only)";
    std::cout << "\n";

    return true;
}

void DrawServer::stop(const std::string& save_file) {
    if (!running_.load()) return;

    std::cout << "[Server] Stopping...\n";

    running_.store(false);

    // Сохранение холста
    {
        std::lock_guard<std::mutex> lock(canvas_.mutex());
        if (canvas_.saveBMP(save_file)) {
            std::cout << "[Server] Canvas saved to " << save_file << "\n";
        }
    }

    // Уведомление клиентов
    {
        auto header = NProtocol::serializeHeader(NProtocol::PacketType::SERVER_SHUTDOWN, 0);
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto* ci : clients_) {
            sendAll(ci->sock, header.data(), header.size());
            closeSocket(ci->sock);
        }
    }

    // Закрытие слушающего сокета
    closeSocket(listen_sock_);
    listen_sock_ = INVALID_SOCK;

    // Ожидание завершения потоков
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto* ci : clients_) {
            if (ci->thread.joinable()) {
                ci->thread.join();
            }
            delete ci;
        }
        clients_.clear();
    }

    std::cout << "[Server] Stopped\n";
}

void DrawServer::acceptLoop() {
    while (running_.load()) {
        // Устанавливаем таймаут для accept через select
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listen_sock_, &fds);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000; // 200ms

        int sel = select(static_cast<int>(listen_sock_) + 1, &fds, nullptr, nullptr, &tv);
        if (sel <= 0) continue;

        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        SocketHandle client_sock = accept(listen_sock_,
                                          reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

        if (client_sock == INVALID_SOCK) continue;

        int cid;
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            cid = next_client_id_++;
            auto* ci = new ClientInfo();
            ci->sock = client_sock;
            ci->id = cid;
            ci->thread = std::thread(&DrawServer::clientLoop, this, client_sock, cid);
            clients_.push_back(ci);
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        std::cout << "[Server] Client #" << cid << " connected from "
                  << ip_str << ":" << ntohs(client_addr.sin_port) << "\n";
    }
}

void DrawServer::clientLoop(SocketHandle sock, int client_id) {
    // Отправляем полный холст при подключении
    sendFullCanvas(sock);

    uint8_t header_buf[NProtocol::HEADER_SIZE];

    while (running_.load()) {
        // select с таймаутом
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int sel = select(static_cast<int>(sock) + 1, &fds, nullptr, nullptr, &tv);
        if (sel < 0) break;
        if (sel == 0) continue;

        if (!recvAll(sock, header_buf, NProtocol::HEADER_SIZE)) break;

        auto header = NProtocol::deserializeHeader(header_buf);

        std::vector<uint8_t> payload(header.payload_size);
        if (header.payload_size > 0) {
            if (!recvAll(sock, payload.data(), header.payload_size)) break;
        }

        switch (header.type) {
            case NProtocol::PacketType::DRAW_STROKE: {
                NProtocol::Color color;
                uint8_t brush_size;
                std::vector<NProtocol::StrokePoint> points;
                NProtocol::deserializeStroke(payload.data(), color, brush_size, points);

                {
                    std::lock_guard<std::mutex> lock(canvas_.mutex());
                    canvas_.drawStroke(color, brush_size, points);
                }

                // Рассылка другим клиентам
                broadcastStroke(color, brush_size, points, client_id);

                // Уведомление локального клиента
                if (stroke_callback_) {
                    stroke_callback_(color, brush_size, points);
                }
                break;
            }

            case NProtocol::PacketType::CLEAR_CANVAS: {
                {
                    std::lock_guard<std::mutex> lock(canvas_.mutex());
                    canvas_.clear();
                }
                broadcastClear(client_id);
                if (clear_callback_) {
                    clear_callback_();
                }
                break;
            }

            case NProtocol::PacketType::REQUEST_SYNC: {
                sendFullCanvas(sock);
                break;
            }

            case NProtocol::PacketType::CLIENT_BYE: {
                goto disconnect;
            }

            case NProtocol::PacketType::PING: {
                auto pong = NProtocol::serializeHeader(NProtocol::PacketType::PONG, 0);
                sendAll(sock, pong.data(), pong.size());
                break;
            }

            default:
                break;
        }
    }

disconnect:
    std::cout << "[Server] Client #" << client_id << " disconnected\n";
    closeSocket(sock);
    removeClient(client_id);
}

void DrawServer::broadcastStroke(const NProtocol::Color& color, uint8_t brush_size,
                                  const std::vector<NProtocol::StrokePoint>& points,
                                  int exclude_client) {
    auto packet = NProtocol::serializeStroke(color, brush_size, points);
    // Меняем тип на STROKE_BROADCAST
    packet[0] = static_cast<uint8_t>(NProtocol::PacketType::STROKE_BROADCAST);

    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto* ci : clients_) {
        if (ci->id != exclude_client) {
            sendAll(ci->sock, packet.data(), packet.size());
        }
    }
}

void DrawServer::broadcastClear(int exclude_client) {
    auto packet = NProtocol::serializeHeader(NProtocol::PacketType::CLEAR_BROADCAST, 0);

    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto* ci : clients_) {
        if (ci->id != exclude_client) {
            sendAll(ci->sock, packet.data(), packet.size());
        }
    }
}

void DrawServer::sendFullCanvas(SocketHandle sock) {
    std::vector<uint8_t> compressed;
    {
        std::lock_guard<std::mutex> lock(canvas_.mutex());
        compressed = NProtocol::compressCanvas(canvas_.getPixels(),
                                               canvas_.width(), canvas_.height());
    }

    auto header = NProtocol::serializeHeader(NProtocol::PacketType::FULL_CANVAS,
                                             static_cast<uint32_t>(compressed.size()));
    sendAll(sock, header.data(), header.size());
    sendAll(sock, compressed.data(), compressed.size());
}

void DrawServer::localStroke(const NProtocol::Color& color, uint8_t brush_size,
                              const std::vector<NProtocol::StrokePoint>& points) {
    {
        std::lock_guard<std::mutex> lock(canvas_.mutex());
        canvas_.drawStroke(color, brush_size, points);
    }

    // Рассылка всем удалённым клиентам
    broadcastStroke(color, brush_size, points, -1);
}

void DrawServer::localClear() {
    {
        std::lock_guard<std::mutex> lock(canvas_.mutex());
        canvas_.clear();
    }
    broadcastClear(-1);
}

void DrawServer::removeClient(int client_id) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [client_id](ClientInfo* ci) {
                if (ci->id == client_id) {
                    if (ci->thread.joinable()) {
                        ci->thread.detach();
                    }
                    delete ci;
                    return true;
                }
                return false;
            }),
        clients_.end()
    );
}

int DrawServer::clientCount() const {
    // const_cast для мьютекса (в реальном коде использовать mutable)
    auto& m = const_cast<std::mutex&>(clients_mutex_);
    std::lock_guard<std::mutex> lock(m);
    return static_cast<int>(clients_.size());
}
