#include "ChatClient.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "GamesEngineeringBase.h" // 新增：音频头文件
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <windows.h>
#include <unordered_map>
#include <ctime>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "d3d11.lib")

// D3D globals
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// 新增：全局音频管理器
GamesEngineeringBase::SoundManager* g_pSoundMgr = nullptr;

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL level;
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &level, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pd3dDeviceContext) g_pd3dDeviceContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// 生成私聊房间ID（和服务端完全一致）
int MakePrivateRoomId(const std::string& a, const std::string& b) {
    std::string key = (a < b) ? (a + "#" + b) : (b + "#" + a);
    return 10000 + (int)(std::hash<std::string>{}(key) % 1000000);
}

std::string GetCurrentTimeString() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

// 新增：初始化音频和音效
bool InitAudio() {
    // 创建音频管理器
    g_pSoundMgr = new GamesEngineeringBase::SoundManager();
    if (!g_pSoundMgr) {
        std::cerr << "create vedio falled" << std::endl;
        return false;
    }

    // 加载音效文件（确保wav文件在exe同目录）
    std::cout << "load chat.wav" << std::endl;
    g_pSoundMgr->load("chat.wav"); // 公聊消息音效
    std::cout << "load call.wav" << std::endl;
    g_pSoundMgr->load("call.wav"); // 私聊消息音效

    std::cout << "vedio load successed" << std::endl;
    return true;
}

// 新增：播放对应音效
void PlayMessageSound(int roomId) {
    if (!g_pSoundMgr) return;

    // 判断是私聊（ID≥10000）还是公聊
    if (roomId >= 10000) {
        // 私聊：播放call.wav
        g_pSoundMgr->play("call.wav");
        std::cout << "call.wav" << std::endl;
    }
    else {
        // 公聊：播放chat.wav
        g_pSoundMgr->play("chat.wav");
        std::cout << "chat.wav" << std::endl;
    }
}

// ====================== main ======================
int main() {
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    // 新增：初始化COM库（XAudio2必须）
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "COM初始化失败: " << hr << std::endl;
        return -1;
    }

    // 新增：初始化音频
    if (!InitAudio()) {
        return -1;
    }

    std::string my_name;

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
        hInstance, nullptr, nullptr, nullptr, nullptr,
        L"ChatClientUI", nullptr };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow(wc.lpszClassName, L"ImGui Chat Client",
        WS_OVERLAPPEDWINDOW, 100, 100, 800, 600,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ChatClient client;
    if (!client.start("127.0.0.1", 65432)) {
        return 1;
    }

    // ========== chat UI state ==========
    char chat_input[256] = "";
    bool scroll_to_bottom = true;
    bool focus_input_next_frame = true;

    // username state
    char username[32] = "";
    bool username_set = false;
    bool show_username_modal = true;

    // 房间状态（公聊）
    std::vector<int> room_order;
    std::unordered_map<int, std::string> room_names;
    std::unordered_map<int, std::vector<std::string>> chat_history; // 公聊+私聊的消息历史
    int current_room = 0;

    room_names[0] = "General";
    room_names[1] = "Study";
    room_names[2] = "Relex";
    room_names[3] = "Game";
    room_order = { 0, 1, 2, 3 };

    // 私聊窗口状态（key=私聊房间ID）
    struct PrivateChatWindow {
        bool open = true;
        std::string other_user;
        std::vector<std::string> messages;
        char input[256] = "";
    };
    std::unordered_map<int, PrivateChatWindow> private_chats;

    MSG msg{};
    const float clear[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

    while (msg.message != WM_QUIT) {
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // new frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ====================
        // 渲染所有私聊窗口
        //====================
        for (auto it = private_chats.begin(); it != private_chats.end(); ) {
            int room_id = it->first;
            PrivateChatWindow& win = it->second;

            if (!win.open) {
                it = private_chats.erase(it);
                continue;
            }

            std::string title = "Private: " + win.other_user + " [ID=" + std::to_string(room_id) + "]";
            ImGui::Begin(title.c_str(), &win.open, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::BeginChild("Messages", ImVec2(400, 200), true);
            for (const std::string& msg : win.messages)
                ImGui::TextWrapped("%s", msg.c_str());
            ImGui::EndChild();

            bool send = ImGui::InputText("##input", win.input, IM_ARRAYSIZE(win.input),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

            ImGui::SameLine();
            send |= ImGui::Button("Send");

            // 发送私聊消息（复用CHAT协议）
            if (send && win.input[0] != '\0' && username_set) {
                std::string time = GetCurrentTimeString();
                std::string full_msg = "[" + time + "] " + std::string(username) + ": " + win.input;

                // 按CHAT协议发送，房间ID是私聊专属ID
                std::string packet = "CHAT|" + std::to_string(room_id) + "|" + full_msg + "\n";
                client.sendMsg(packet);

                // 本地显示
                win.messages.push_back(full_msg);
                chat_history[room_id].push_back(full_msg); // 同步到全局历史
                win.input[0] = '\0';
            }

            ImGui::End();
            ++it;
        }

        // 用户名模态框
        if (show_username_modal) {
            ImGui::OpenPopup("Enter Your Name");
            show_username_modal = false;
        }

        if (ImGui::BeginPopupModal("Enter Your Name", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Please enter your display name:");
            ImGui::InputText("##username", username, IM_ARRAYSIZE(username));

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                if (strlen(username) > 0) {
                    my_name = username;
                    username_set = true;
                    ImGui::CloseCurrentPopup();

                    // 发送用户名到服务端
                    std::string packet = "USERNAME|" + std::string(username) + "\n";
                    client.sendMsg(packet);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                strcpy(username, "Me");
                my_name = "Me";
                username_set = true;
                ImGui::CloseCurrentPopup();

                // 发送默认用户名
                std::string packet = "USERNAME|Me\n";
                client.sendMsg(packet);
            }

            ImGui::EndPopup();
        }

        // 未设置用户名时只渲染模态框
        if (!username_set) {
            ImGui::Render();
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            g_pSwapChain->Present(1, 0);
            continue;
        }

        // 主聊天窗口
        ImGui::SetNextWindowSize(ImVec2(560, 440), ImGuiCond_FirstUseEver);
        ImGui::Begin("Chatroom");

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float left_width = 220.0f;

        // 左侧房间列表
        ImGui::BeginChild("LeftPanel", ImVec2(left_width, avail.y), true);
        ImGui::Text("Chat Rooms");
        ImGui::Separator();

        for (int room_id : room_order) {
            const std::string& name = room_names[room_id];
            if (ImGui::Selectable(name.c_str(), current_room == room_id)) {
                current_room = room_id;
                chat_history[current_room].clear();
                scroll_to_bottom = true;
                // 加入公聊房间
                client.sendMsg("JOIN|" + std::to_string(room_id) + "\n");
            }
        }

        ImGui::EndChild();
        ImGui::SameLine();

        // 右侧聊天区域
        ImGui::BeginChild("RightPanel", ImVec2(0, avail.y), true);
        float input_height = ImGui::GetFrameHeightWithSpacing() * 2;

        // 聊天历史
        ImGui::BeginChild("ChatHistory", ImVec2(0, avail.y - input_height), false);

        // 解析服务端消息（仅CHAT协议，无PM）
        auto new_messages = client.getMessages();
        for (const auto& line : new_messages) {
            std::stringstream ss(line);
            std::string type;
            if (!std::getline(ss, type, '|')) continue;

            if (type == "CHAT") {
                std::string room_str, text;
                if (!std::getline(ss, room_str, '|')) continue;
                if (!std::getline(ss, text)) continue;

                int room = std::stoi(room_str);
                if (!text.empty()) {
                    chat_history[room].push_back(text);

                    // 新增：播放对应音效（公聊/私聊）
                    PlayMessageSound(room);

                    // 如果是私聊消息，同步到对应私聊窗口
                    auto it = private_chats.find(room);
                    if (it != private_chats.end()) {
                        it->second.messages.push_back(text);
                    }
                }
            }
        }

        // 显示当前房间的消息
        if (chat_history.find(current_room) != chat_history.end()) {
            const std::vector<std::string>& messages = chat_history[current_room];
            for (size_t i = 0; i < messages.size(); ++i) {
                const std::string& msg = messages[i];

                size_t lb = msg.find('[');
                size_t rb = msg.find(']');
                size_t colon = msg.find(':', rb + 1);

                if (lb == std::string::npos || rb == std::string::npos || colon == std::string::npos ||
                    rb + 2 > msg.size() || colon < rb + 2) {
                    ImGui::TextWrapped("%s", msg.c_str());
                    continue;
                }

                std::string time = msg.substr(lb + 1, rb - lb - 1);
                std::string name = msg.substr(rb + 2, colon - (rb + 2));
                std::string text = msg.substr(colon + 1);

                ImGui::PushID((int)i);

                // 灰色显示时间
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[%s]", time.c_str());
                ImGui::SameLine();

                // 用户名可右键打开私聊
                ImGui::SameLine();
                if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {}
                if (ImGui::BeginPopupContextItem("name_popup")) {
                    if (name != username) {
                        if (ImGui::MenuItem("Private Chat")) {
                            // 生成私聊房间ID
                            int room_id = MakePrivateRoomId(my_name, name);
                            std::cout << "[DEBUG] 打开私聊窗口，房间ID=" << room_id << "\n";

                            // 加入私聊房间
                            client.sendMsg("JOIN|" + std::to_string(room_id) + "\n");

                            // 创建/打开私聊窗口
                            if (!private_chats.count(room_id)) {
                                PrivateChatWindow win;
                                win.open = true;
                                win.other_user = name;
                                win.messages = chat_history[room_id]; // 同步历史消息
                                private_chats[room_id] = win;
                            }
                        }
                    }
                    ImGui::EndPopup();
                }

                ImGui::SameLine();
                ImGui::TextWrapped(":%s", text.c_str());

                ImGui::PopID();
            }
        }

        if (scroll_to_bottom) {
            ImGui::SetScrollHereY(1.0f);
            scroll_to_bottom = false;
        }
        ImGui::EndChild();

        // 回到底部按钮
        ImVec2 history_pos = ImGui::GetItemRectMin();
        ImVec2 history_size = ImGui::GetItemRectSize();
        ImGui::SetCursorScreenPos(ImVec2(history_pos.x + history_size.x - 80.0f,
            history_pos.y + history_size.y - 40.0f));

        ImGui::BeginChild("BackToBottomOverlay", ImVec2(120, 30), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
        if (ImGui::Button("TO END")) scroll_to_bottom = true;
        ImGui::EndChild();

        ImGui::Separator();

        // 公聊输入框
        bool send = false;
        send |= ImGui::InputText("##chatinput", chat_input, IM_ARRAYSIZE(chat_input),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (focus_input_next_frame) {
            ImGui::SetKeyboardFocusHere(-1);
            focus_input_next_frame = false;
        }

        ImGui::SameLine();
        send |= ImGui::Button("Send");

        if (send && chat_input[0] != '\0') {
            std::string time = GetCurrentTimeString();
            std::string full_msg = "[" + time + "] " + std::string(username) + ": " + chat_input;

            // 发送公聊消息（CHAT协议）
            std::string packet = "CHAT|" + std::to_string(current_room) + "|" + full_msg + "\n";
            client.sendMsg(packet);

            // 本地显示
            chat_history[current_room].push_back(full_msg);
            chat_input[0] = '\0';
            scroll_to_bottom = true;
            focus_input_next_frame = true;
        }

        ImGui::EndChild(); // RightPanel
        ImGui::End();      // Chatroom

        // 渲染
        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    // 新增：清理音频资源
    if (g_pSoundMgr) {
        delete g_pSoundMgr;
        g_pSoundMgr = nullptr;
    }

    // 清理资源
    client.stop();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}
