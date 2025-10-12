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

#pragma comment(lib, "ws2_32.lib")

// --- Konfiguracja ---
const int UDP_PORT = 54321;
const int TCP_PORT = 54322;
const char* BROADCAST_MSG = "CZY_KTOKOLWIEK_MNIE_SLYSZY_AGENT_V4_FIX";
const char* SERVER_RESPONSE_MSG = "TAK_SLYSZE_CIE_SERVER_V4_FIX";
const int BUFFER_SIZE = 1024;
// --- Koniec Konfiguracji ---

// ... reszta kodu pomocniczego i czatu jest bez zmian ...
// ... wklejam dla kompletnoœci ...

std::atomic<bool> g_connectionActive(false);
std::mutex g_coutMutex;

void print_message(const std::string& msg) { /* ... */
    std::lock_guard<std::mutex> lock(g_coutMutex);
    std::cout << msg << std::endl;
}
void receive_handler(SOCKET clientSocket) { /* ... */
    char buffer[BUFFER_SIZE];
    while (g_connectionActive) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived > 0) {
            print_message("Agent: " + std::string(buffer, bytesReceived));
        } else {
            if (g_connectionActive) {
                print_message("[INFO] Po³¹czenie z agentem zosta³o zerwane.");
                g_connectionActive = false;
            }
            break;
        }
    }
}
void chat_session(SOCKET clientSocket) { /* ... */
    g_connectionActive = true;
    print_message("[INFO] Po³¹czono z agentem. Mo¿esz zacz¹æ czatowaæ. Napisz 'exit' aby zakoñczyæ.");
    std::thread receiver(receive_handler, clientSocket);
    std::string line;
    while (g_connectionActive) {
        std::getline(std::cin, line);
        if (!g_connectionActive) break;
        if (line == "exit") g_connectionActive = false;
        if (send(clientSocket, line.c_str(), line.length(), 0) == SOCKET_ERROR) {
            print_message("[B£¥D] Nie uda³o siê wys³aæ wiadomoœci: " + std::to_string(WSAGetLastError()));
            g_connectionActive = false;
        }
    }
    if (receiver.joinable()) receiver.join();
    closesocket(clientSocket);
    print_message("[INFO] Sesja czatu zakoñczona.");
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    while (true) {
        print_message("\n[INFO] Nas³uchujê na porcie UDP " + std::to_string(UDP_PORT) + "...");
        SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        
        // NOWOŒÆ: Pozwól na wspó³dzielenie portu
        char reuse_addr_opt = '1';
        if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_opt, sizeof(reuse_addr_opt)) == SOCKET_ERROR) {
            print_message("[OSTRZE¯ENIE] Nie uda³o siê ustawiæ SO_REUSEADDR.");
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(UDP_PORT);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(udpSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "[KRYTYCZNY B£¥D] B³¹d bindowania gniazda UDP: " << WSAGetLastError() << std::endl;
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
                    print_message("[INFO] Wysy³am odpowiedŸ zwrotn¹...");
                    
                    // Serwer odpowiada na adres, z którego przyszed³ broadcast
                    sendto(udpSocket, SERVER_RESPONSE_MSG, strlen(SERVER_RESPONSE_MSG), 0, (SOCKADDR*)&clientAddr, clientAddrSize);
                    break;
                }
            } else {
                 std::cerr << "[B£¥D] B³¹d recvfrom: " << WSAGetLastError() << std::endl;
            }
        }
        closesocket(udpSocket);

        // Faza TCP (bez zmian)
        print_message("[INFO] Czekam na po³¹czenie TCP od agenta na porcie " + std::to_string(TCP_PORT) + "...");
        SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in tcpServerAddr;
        tcpServerAddr.sin_family = AF_INET;
        tcpServerAddr.sin_port = htons(TCP_PORT);
        tcpServerAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(listenSocket, (SOCKADDR*)&tcpServerAddr, sizeof(tcpServerAddr)) == SOCKET_ERROR) {
            std::cerr << "[KRYTYCZNY B£¥D] B³¹d bindowania gniazda TCP: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket); continue;
        }

        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
             std::cerr << "B³¹d nas³uchiwania na gnieŸdzie TCP: " << WSAGetLastError() << std::endl;
             closesocket(listenSocket); continue;
        }

        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        closesocket(listenSocket);

        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "B³¹d akceptowania po³¹czenia TCP: " << WSAGetLastError() << std::endl;
            continue;
        }

        chat_session(clientSocket);
        print_message("[INFO] Powrót do trybu nas³uchiwania na broadcast.");
    }

    WSACleanup();
    return 0;
}
