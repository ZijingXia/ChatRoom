#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <atomic>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

class ChatClient {
public:
    ChatClient();
    ~ChatClient();

    bool start(const char* ip = "127.0.0.1", int port = 65432);

    void stop();

    void sendMsg(const std::string& msg);

    std::vector<std::string> getMessages();

private:
    void recvLoop();

    SOCKET sock;
    std::atomic<bool> running;
    std::thread recvThread;
    std::mutex mutex;
    std::vector<std::string> chatHistory;
};
