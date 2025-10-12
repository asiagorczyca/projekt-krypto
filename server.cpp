// server.cpp
#define _WIN32_WINNT 0x0601
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <conio.h>

#pragma comment(lib, "ws2_32.lib")

// --- Konfiguracja ---
const int UDP_PORT = 54321;
const int TCP_PORT = 54322;
const char* BROADCAST_MSG = "CZY_KTOKOLWIEK_MNIE_SLYSZY_AGENT_V9_RELIABLE";
const char* SERVER_RESPONSE_MSG = "TAK_SLYSZE_CIE_SERVER_V9_RELIABLE";
const int BUFFER_SIZE = 1024;
// --- Koniec Konfiguracji ---

std::atomic<bool> g_connectionActive(false);
std::mutex g_coutMutex;

void print_message(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_coutMutex);
    std::cout << msg << std::endl;
}

void receive_handler(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    while (g_connectionActive) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived > 0) {
            std::lock_guard<std::mutex> lock(g_coutMutex);
            std::cout << "\rAgent: " << std::string(buffer, bytesReceived) << std::endl << "Ty: ";
        } else {
            if (g_connectionActive) {
                print_message("\r[INFO] Po≥πczenie z agentem zosta≥o zerwane.");
                g_connectionActive = false;
            }
            break;
        }
    }
}

void chat_session(SOCKET clientSocket) {
    g_connectionActive = true;
    print_message("[INFO] Po≥πczono z agentem. Moøesz zaczπÊ czatowaÊ. Napisz 'exit' aby zakoÒczyÊ.");
    std::thread receiver(receive_handler, clientSocket);
    std::string current_line;
    {
        std::lock_guard<std::mutex> lock(g_coutMutex);
        std::cout << "Ty: ";
    }
    while (g_connectionActive) {
        if (_kbhit()) {
            char c = _getch();
            if (c == '\r') {
                if (!current_line.empty()) {
                    if (current_line == "exit") g_connectionActive = false;
                    if (send(clientSocket, current_line.c_str(), current_line.length(), 0) == SOCKET_ERROR) {
                        print_message("\r[B£•D] Nie uda≥o siÍ wys≥aÊ wiadomoúci.");
                        g_connectionActive = false;
                    }
                    std::lock_guard<std::mutex> lock(g_coutMutex);
                    std::cout << std::endl << "Ty: ";
                    current_line.clear();
                }
            } else if (c == '\b') {
                if (!current_line.empty()) {
                    current_line.pop_back();
                    std::lock_guard<std::mutex> lock(g_coutMutex);
                    std::cout << "\b \b";
                }
            } else {
                current_line += c;
                std::lock_guard<std::mutex> lock(g_coutMutex);
                std::cout << c;
            }
        }
        Sleep(50);
    }
    if (receiver.joinable()) receiver.join();
    closesocket(clientSocket);
    print_message("\n[INFO] Sesja czatu zakoÒczona.");
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    while (true) {
        print_message("\n[INFO] Nas≥uchujÍ na porcie UDP " + std::to_string(UDP_PORT) + "...");
        SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        char reuse_addr_opt = '1';
        setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_opt, sizeof(reuse_addr_opt));
        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(UDP_PORT);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(udpSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "[KRYTYCZNY B£•D] B≥πd bindowania gniazda UDP: " << WSAGetLastError() << std::endl;
            closesocket(udpSocket); Sleep(5000); continue;
        }

        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        char buffer[BUFFER_SIZE];

        while(true) {
            int bytesReceived = recvfrom(udpSocket, buffer, BUFFER_SIZE, 0, (SOCKADDR*)&clientAddr, &clientAddrSize);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                if (strcmp(buffer, BROADCAST_MSG) == 0) {
                    std::string clientIp = inet_ntoa(clientAddr.sin_addr);
                    print_message("[INFO] Otrzymano broadcast od: " + clientIp);
                    print_message("[INFO] Wysy≥am odpowiedü zwrotnπ...");
                    sendto(udpSocket, SERVER_RESPONSE_MSG, strlen(SERVER_RESPONSE_MSG), 0, (SOCKADDR*)&clientAddr, clientAddrSize);
                    break;
                }
            } else {
                 std::cerr << "[B£•D] B≥πd recvfrom: " << WSAGetLastError() << std::endl;
            }
        }
        closesocket(udpSocket);

        print_message("[INFO] Czekam na po≥πczenie TCP od agenta na porcie " + std::to_string(TCP_PORT) + "...");
        SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in tcpServerAddr;
        tcpServerAddr.sin_family = AF_INET;
        tcpServerAddr.sin_port = htons(TCP_PORT);
        tcpServerAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(listenSocket, (SOCKADDR*)&tcpServerAddr, sizeof(tcpServerAddr)) == SOCKET_ERROR) {
            std::cerr << "[KRYTYCZNY B£•D] B≥πd bindowania gniazda TCP: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket); continue;
        }

        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
             std::cerr << "B≥πd nas≥uchiwania na gnieüdzie TCP: " << WSAGetLastError() << std::endl;
             closesocket(listenSocket); continue;
        }

        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        closesocket(listenSocket);

        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "B≥πd akceptowania po≥πczenia TCP: " << WSAGetLastError() << std::endl;
            continue;
        }

        chat_session(clientSocket);
        print_message("[INFO] PowrÛt do trybu nas≥uchiwania na broadcast.");
    }
    WSACleanup();
    return 0;
}
