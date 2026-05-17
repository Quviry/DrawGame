#include "client/client.h"
#include <iostream>
#include <cstring>
#include <cmath>

#ifdef _WIN32
static void initWSA2() {
    static bool done = false;
    if (!done) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        done = true;
    }
}
#endif

DrawClient::DrawClient() {
#ifdef _WIN32
    initWSA2();
#endif
}

DrawClient::~DrawClient() {
    stop();
}

bool DrawClient::startLocal(uint16_t port, bool open_port) {
    mode_ = Mode::LOCAL;

    server_ = new DrawServer();

    // Коллбэк для штрихов от удалённых клиентов
    server_->setStrokeCallback([this](const NProtocol::Color& c, uint8_t bs,
                                       const std::vector<NProtocol::StrokePoint>& pts) {
        // Применяем на локальный холст
        std::lock_guard<std::mutex> lock(local_canvas_.mutex());
        local_canvas_.drawStroke(c, bs, pts);
    });

    server_->setClearCallback([this]() {
        std::lock_guard<std::mutex> lock(local_canvas_.mutex());
        local_canvas_.clear();
    });

    if (!server_->start(port, !open_port)) {
        delete server_;
        server_ = nullptr;
        return false;
    }

    // Копируем холст сервера (может быть загружен из файла)
    {
        std::lock_guard<std::mutex> lock1(server_->canvas().mutex());
        std::lock_guard<std::mutex> lock2(local_canvas_.mutex());
        memcpy(local_canvas_.getPixelsMut(), server_->canvas().getPixels(),
               sizeof(NProtocol::Color) * NProtocol::CANVAS_WIDTH * NProtocol::CANVAS_HEIGHT);
    }

    connected_.store(true);
    running_.store(true);

    std::string title = "DrawGame [Local Server, port " + std::to_string(port) + "]";
    if (!renderer_.init(NProtocol::CANVAS_WIDTH, NProtocol::CANVAS_HEIGHT, title)) {
        return false;
    }

    buildPaletteEntries();
    return true;
}

bool DrawClient::connectRemote(const std::string& host, uint16_t port) {
    mode_ = Mode::REMOTE;

    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCK) {
        std::cerr << "[Client] Socket creation failed\n";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "[Client] Invalid address: " << host << "\n";
        closeSocket();
        return false;
    }

    if (connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[Client] Connection failed to " << host << ":" << port << "\n";
        closeSocket();
        return false;
    }

    std::cout << "[Client] Connected to " << host << ":" << port << "\n";

    connected_.store(true);
    running_.store(true);

    std::string title = "DrawGame [Connected to " + host + ":" + std::to_string(port) + "]";
    if (!renderer_.init(NProtocol::CANVAS_WIDTH, NProtocol::CANVAS_HEIGHT, title)) {
        return false;
    }

    buildPaletteEntries();

    // Запускаем поток приёма
    recv_thread_ = std::thread(&DrawClient::networkRecvLoop, this);

    return true;
}

void DrawClient::run() {
    while (running_.load() && !glfwWindowShouldClose(renderer_.window())) {
        glfwPollEvents();

        handleInput();
        processNetworkEvents();

        // Обновляем текстуру
        {
            std::lock_guard<std::mutex> lock(local_canvas_.mutex());
            renderer_.updateTexture(local_canvas_.getPixels());
        }

        int cc = 0;
        if (server_) cc = server_->clientCount();

        renderer_.render(current_color_, brush_size_, connected_.load(), cc);
    }

    stop();
}

void DrawClient::stop() {
    if (!running_.exchange(false)) return;

    if (mode_ == Mode::REMOTE) {
        if (sock_ != INVALID_SOCK) {
            auto bye = NProtocol::serializeHeader(NProtocol::PacketType::CLIENT_BYE, 0);
            sendAll(bye.data(), bye.size());
            closeSocket();
        }
        if (recv_thread_.joinable()) {
            recv_thread_.join();
        }
    }

    if (server_) {
        server_->stop("canvas_save.bmp");
        delete server_;
        server_ = nullptr;
    }

    renderer_.shutdown();
}

void DrawClient::handleInput() {
    GLFWwindow* win = renderer_.window();

    // Клавиши
    // C - очистить
    if (glfwGetKey(win, GLFW_KEY_C) == GLFW_PRESS &&
        glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        if (mode_ == Mode::LOCAL && server_) {
            server_->localClear();
            std::lock_guard<std::mutex> lock(local_canvas_.mutex());
            local_canvas_.clear();
        } else {
            sendClearToServer();
        }
    }

    // +/- размер кисти
    static bool plus_was = false, minus_was = false;
    bool plus_now = glfwGetKey(win, GLFW_KEY_EQUAL) == GLFW_PRESS ||
                    glfwGetKey(win, GLFW_KEY_KP_ADD) == GLFW_PRESS;
    bool minus_now = glfwGetKey(win, GLFW_KEY_MINUS) == GLFW_PRESS ||
                     glfwGetKey(win, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;

    if (plus_now && !plus_was) {
        brush_size_ = std::min(50, (int)brush_size_ + 2);
    }
    if (minus_now && !minus_was) {
        brush_size_ = std::max(1, (int)brush_size_ - 2);
    }
    plus_was = plus_now;
    minus_was = minus_now;

    // Scroll для размера кисти
    // (Обработка scroll через callback была бы лучше, но для простоты используем клавиши)

    // Escape - выход
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
    }

    // S - сохранить
    static bool s_was = false;
    bool s_now = glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS &&
                 glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    if (s_now && !s_was) {
        std::lock_guard<std::mutex> lock(local_canvas_.mutex());
        if (local_canvas_.saveBMP("drawing_export.bmp")) {
            std::cout << "[Client] Exported to drawing_export.bmp\n";
        }
    }
    s_was = s_now;

    // Мышь
    double mx, my;
    glfwGetCursorPos(win, &mx, &my);

    int mouse_state = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT);

    if (mouse_state == GLFW_PRESS) {
        // Проверяем клик по палитре
        if (!drawing_) {
            if (handlePaletteClick(mx, my)) {
                return;
            }
        }

        int cx, cy;
        bool on_canvas = renderer_.getCanvasMousePos(cx, cy);

        if (on_canvas) {
            if (!drawing_) {
                drawing_ = true;
                current_stroke_.clear();
                last_x_ = cx;
                last_y_ = cy;
            }

            if (cx != last_x_ || cy != last_y_ || current_stroke_.empty()) {
                NProtocol::StrokePoint pt;
                pt.x = static_cast<int16_t>(cx);
                pt.y = static_cast<int16_t>(cy);
                current_stroke_.push_back(pt);

                // Рисуем на локальном холсте немедленно
                if (current_stroke_.size() >= 2) {
                    auto& p0 = current_stroke_[current_stroke_.size() - 2];
                    auto& p1 = current_stroke_[current_stroke_.size() - 1];
                    std::lock_guard<std::mutex> lock(local_canvas_.mutex());
                    local_canvas_.drawLine(p0.x, p0.y, p1.x, p1.y,
                                           current_color_, brush_size_);
                } else {
                    std::lock_guard<std::mutex> lock(local_canvas_.mutex());
                    int radius = std::max(0, (brush_size_ - 1) / 2);
                    local_canvas_.drawCircle(cx, cy, radius, current_color_);
                }

                last_x_ = cx;
                last_y_ = cy;
            }
        }
    } else {
        if (drawing_) {
            drawing_ = false;

            if (!current_stroke_.empty()) {
                // Отправляем штрих серверу
                if (mode_ == Mode::LOCAL && server_) {
                    server_->localStroke(current_color_, brush_size_, current_stroke_);
                } else {
                    sendStrokeToServer(current_color_, brush_size_, current_stroke_);
                }
                current_stroke_.clear();
            }
        }
    }

    // Правая кнопка мыши - пипетка (взять цвет)
    if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        int cx, cy;
        if (renderer_.getCanvasMousePos(cx, cy)) {
            std::lock_guard<std::mutex> lock(local_canvas_.mutex());
            const NProtocol::Color* px = local_canvas_.getPixels();
            current_color_ = px[cy * local_canvas_.width() + cx];
        }
    }
}

void DrawClient::processNetworkEvents() {
    std::lock_guard<std::mutex> lock(net_events_mutex_);
    while (!net_events_.empty()) {
        auto& ev = net_events_.front();
        switch (ev.type) {
            case NetEvent::STROKE: {
                std::lock_guard<std::mutex> lk(local_canvas_.mutex());
                local_canvas_.drawStroke(ev.color, ev.brush_size, ev.points);
                break;
            }
            case NetEvent::CLEAR: {
                std::lock_guard<std::mutex> lk(local_canvas_.mutex());
                local_canvas_.clear();
                break;
            }
            case NetEvent::FULL_CANVAS: {
                std::lock_guard<std::mutex> lk(local_canvas_.mutex());
                if (ev.canvas_data.size() == (size_t)(local_canvas_.width() * local_canvas_.height())) {
                    memcpy(local_canvas_.getPixelsMut(), ev.canvas_data.data(),
                           ev.canvas_data.size() * sizeof(NProtocol::Color));
                }
                break;
            }
            case NetEvent::DISCONNECT: {
                connected_.store(false);
                std::cout << "[Client] Disconnected from server\n";
                break;
            }
        }
        net_events_.pop();
    }
}

void DrawClient::networkRecvLoop() {
    uint8_t header_buf[NProtocol::HEADER_SIZE];

    while (running_.load() && connected_.load()) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock_, &fds);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int sel = select(static_cast<int>(sock_) + 1, &fds, nullptr, nullptr, &tv);
        if (sel < 0) break;
        if (sel == 0) continue;

        if (!recvAll(header_buf, NProtocol::HEADER_SIZE)) {
            NetEvent ev;
            ev.type = NetEvent::DISCONNECT;
            std::lock_guard<std::mutex> lock(net_events_mutex_);
            net_events_.push(std::move(ev));
            break;
        }

        auto header = NProtocol::deserializeHeader(header_buf);

        std::vector<uint8_t> payload(header.payload_size);
        if (header.payload_size > 0) {
            if (!recvAll(payload.data(), header.payload_size)) {
                NetEvent ev;
                ev.type = NetEvent::DISCONNECT;
                std::lock_guard<std::mutex> lock(net_events_mutex_);
                net_events_.push(std::move(ev));
                break;
            }
        }

        switch (header.type) {
            case NProtocol::PacketType::STROKE_BROADCAST: {
                NetEvent ev;
                ev.type = NetEvent::STROKE;
                NProtocol::deserializeStroke(payload.data(), ev.color, ev.brush_size, ev.points);
                std::lock_guard<std::mutex> lock(net_events_mutex_);
                net_events_.push(std::move(ev));
                break;
            }

            case NProtocol::PacketType::CLEAR_BROADCAST: {
                NetEvent ev;
                ev.type = NetEvent::CLEAR;
                std::lock_guard<std::mutex> lock(net_events_mutex_);
                net_events_.push(std::move(ev));
                break;
            }

            case NProtocol::PacketType::FULL_CANVAS: {
                NetEvent ev;
                ev.type = NetEvent::FULL_CANVAS;
                ev.canvas_data.resize(NProtocol::CANVAS_WIDTH * NProtocol::CANVAS_HEIGHT);
                NProtocol::decompressCanvas(payload.data(), payload.size(),
                                            ev.canvas_data.data(),
                                            NProtocol::CANVAS_WIDTH, NProtocol::CANVAS_HEIGHT);
                std::lock_guard<std::mutex> lock(net_events_mutex_);
                net_events_.push(std::move(ev));
                break;
            }

            case NProtocol::PacketType::SERVER_SHUTDOWN: {
                NetEvent ev;
                ev.type = NetEvent::DISCONNECT;
                std::lock_guard<std::mutex> lock(net_events_mutex_);
                net_events_.push(std::move(ev));
                connected_.store(false);
                break;
            }

            case NProtocol::PacketType::PONG:
                break;

            default:
                break;
        }
    }
}

void DrawClient::sendStrokeToServer(const NProtocol::Color& color, uint8_t brush_size,
                                     const std::vector<NProtocol::StrokePoint>& points) {
    if (!connected_.load() || sock_ == INVALID_SOCK) return;
    auto packet = NProtocol::serializeStroke(color, brush_size, points);
    sendAll(packet.data(), packet.size());
}

void DrawClient::sendClearToServer() {
    if (!connected_.load() || sock_ == INVALID_SOCK) return;
    auto packet = NProtocol::serializeHeader(NProtocol::PacketType::CLEAR_CANVAS, 0);
    sendAll(packet.data(), packet.size());
}

bool DrawClient::sendAll(const uint8_t* data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
        int ret = send(sock_, reinterpret_cast<const char*>(data + sent),
                       static_cast<int>(size - sent), 0);
        if (ret <= 0) return false;
        sent += ret;
    }
    return true;
}

bool DrawClient::recvAll(uint8_t* data, size_t size) {
    size_t received = 0;
    while (received < size) {
        int ret = recv(sock_, reinterpret_cast<char*>(data + received),
                       static_cast<int>(size - received), 0);
        if (ret <= 0) return false;
        received += ret;
    }
    return true;
}

void DrawClient::closeSocket() {
    if (sock_ == INVALID_SOCK) return;
#ifdef _WIN32
    closesocket(sock_);
#else
    close(sock_);
#endif
    sock_ = INVALID_SOCK;
}

bool DrawClient::handlePaletteClick(double mx, double my) {
    for (auto& pe : palette_entries_) {
        if (mx >= pe.x_min && mx <= pe.x_max && my >= pe.y_min && my <= pe.y_max) {
            current_color_ = pe.color;
            return true;
        }
    }
    return false;
}

void DrawClient::buildPaletteEntries() {
    palette_entries_.clear();

    int window_w = NProtocol::CANVAS_WIDTH;
    int window_h = NProtocol::CANVAS_HEIGHT + Renderer::UI_PANEL_HEIGHT;

    NProtocol::Color colors[] = {
        {0,0,0,255}, {255,255,255,255}, {255,0,0,255}, {0,255,0,255},
        {0,0,255,255}, {255,255,0,255}, {255,0,255,255}, {0,255,255,255},
        {255,127,0,255}, {127,0,255,255}, {127,127,127,255}, {178,76,25,255}
    };
    int count = sizeof(colors) / sizeof(colors[0]);

    float box_px = 40.0f;
    float start_x = 10.0f + box_px / 2.0f;
    float center_y = NProtocol::CANVAS_HEIGHT + Renderer::UI_PANEL_HEIGHT / 2.0f;
    float half = box_px / 2.0f * 0.8f;

    for (int i = 0; i < count; i++) {
        PaletteEntry pe;
        pe.color = colors[i];
        float cx = start_x + i * box_px;
        pe.x_min = cx - half;
        pe.x_max = cx + half;
        pe.y_min = center_y - half;
        pe.y_max = center_y + half;
        palette_entries_.push_back(pe);
    }
}
