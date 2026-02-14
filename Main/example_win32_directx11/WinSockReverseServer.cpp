#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>

#pragma comment(lib, "Ws2_32.lib")

constexpr int PORT = 65432;
constexpr int BUF_SIZE = 1024;

// client line
std::vector<SOCKET> clients;
std::mutex clients_mutex;

// boadcast message to all clients except sender
void broadcast(const std::string& msg, SOCKET sender) {
    std::lock_guard<std::mutex> lock(clients_mutex);

    for (SOCKET client : clients) {
        if (client != sender) {
            send(client, msg.c_str(), static_cast<int>(msg.size()), 0);
        }
    }
}

// each client handler
void handle_client(SOCKET client_socket) {
    char buffer[BUF_SIZE];

    while (true) {
        int bytes = recv(client_socket, buffer, BUF_SIZE - 1, 0);

        if (bytes <= 0) {
            break;
        }

        buffer[bytes] = '\0';
        std::string msg(buffer);

        std::cout << "[Client " << client_socket << "] " << msg << std::endl;

        if (msg == "!bye") {
            break;
        }

        broadcast(msg, client_socket);
    }

    // remove client from list
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(
            std::remove(clients.begin(), clients.end(), client_socket),
            clients.end()
        );
    }

    closesocket(client_socket);
    std::cout << "[Client disconnected]\n";
}

// server main
int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Chat server listening on port " << PORT << "...\n";

    while (true) {
        SOCKET client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed\n";
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(client_socket);
        }

        std::cout << "[New client connected]\n";
        std::thread(handle_client, client_socket).detach();
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}
