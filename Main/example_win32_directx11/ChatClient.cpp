#include "ChatClient.h"

ChatClient::ChatClient() : sock(INVALID_SOCKET), running(false) {}
ChatClient::~ChatClient() { stop(); }

bool ChatClient::start(const char* ip, int port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Connect failed: " << WSAGetLastError() << "\n";
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }

    running = true;
    recvThread = std::thread(&ChatClient::recvLoop, this);
    return true;
}

void ChatClient::stop() {
    running = false;
    if (sock != INVALID_SOCKET) {
        shutdown(sock, SD_BOTH);
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    if (recvThread.joinable())
        recvThread.join();
    WSACleanup();
}

void ChatClient::sendMsg(const std::string& msg) {
        std::string send_msg = msg;
        send(sock, send_msg.c_str(), (int)send_msg.size(), 0);
}

std::vector<std::string> ChatClient::getMessages() { // only collect message for once
    std::lock_guard<std::mutex> lock(mutex);

    std::vector<std::string> out = std::move(chatHistory);
    chatHistory.clear();

    return out;
}

void ChatClient::recvLoop() {
    char buffer[1024];
    std::string partial; // get the partial message when TCP spilt the message into several packets

    while (running) {
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        partial += buffer;

        size_t pos;
        while ((pos = partial.find('\n')) != std::string::npos) {
            std::string line = partial.substr(0, pos);
            partial.erase(0, pos + 1);

            if (!line.empty()) { // no empty line
                std::lock_guard<std::mutex> lock(mutex);
                chatHistory.push_back(line);
            }
        }
    }

    running = false;
}

