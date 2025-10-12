// agent.cpp
#define _WIN32_WINNT 0x0601
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <conio.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

// --- Konfiguracja ---
const int UDP_PORT = 54321;
const int TCP_PORT = 54322;
const char* BROADCAST_MSG = "CZY_KTOKOLWIEK_MNIE_SLYSZY_AGENT_V9_RELIABLE";
const char* SERVER_RESPONSE_MSG = "TAK_SLYSZE_CIE_SERVER_V9_RELIABLE";
const int BUFFER_SIZE = 1024;
// --- Koniec Konfiguracji ---

// Zmienne globalne do komunikacji miêdzy w¹tkami
std::atomic<bool> g_connectionActive(false);
std::atomic<bool> g_serverFound(false);
std::string g_serverIp;
std::mutex g_coutMutex;
std::mutex g_serverIpMutex;

// --- Funkcje pomocnicze i czatu (bez zmian) ---
void print_message(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_coutMutex);
    std::cout << msg << std::endl;
}
// --- Pe³na implementacja funkcji pomocniczych (wklej je do pliku) ---

std::vector<std::string> get_broadcast_addresses() {
    std::vector<std::string> addresses; ULONG bufferSize = 15000; PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize); if (pAddresses == NULL) return addresses;
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &bufferSize) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses != NULL; pCurrAddresses = pCurrAddresses->Next) {
            if (pCurrAddresses->OperStatus != IfOperStatusUp) continue;
            for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != NULL; pUnicast = pUnicast->Next) {
                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* sa_in = (sockaddr_in*)pUnicast->Address.lpSockaddr; ULONG ip_addr = sa_in->sin_addr.S_un.S_addr; ULONG subnet_mask = 0;
                    ConvertLengthToIpv4Mask(pUnicast->OnLinkPrefixLength, &subnet_mask); ULONG broadcast_addr = ip_addr | ~subnet_mask;
                    in_addr broadcast_in_addr; broadcast_in_addr.S_un.S_addr = broadcast_addr; addresses.push_back(inet_ntoa(broadcast_in_addr));
                }
            }
        }
    }
    if (pAddresses) free(pAddresses); addresses.push_back("255.255.255.255"); return addresses;
}
void receive_handler(SOCKET tcpSocket) {
    char buffer[BUFFER_SIZE]; while (g_connectionActive) {
        int bytesReceived = recv(tcpSocket, buffer, BUFFER_SIZE, 0); if (bytesReceived > 0) { std::lock_guard<std::mutex> lock(g_coutMutex); std::cout << "\rSerwer: " << std::string(buffer, bytesReceived) << std::endl << "Ty: "; }
        else { if (g_connectionActive) { print_message("\r[INFO] Po³¹czenie z serwerem zosta³o zerwane."); g_connectionActive = false; } break; }
    }
}
void chat_session(SOCKET tcpSocket) {
    g_connectionActive = true; print_message("[INFO] Po³¹czono z serwerem. Mo¿esz zacz¹æ czatowaæ. Napisz 'exit' aby zakoñczyæ."); std::thread receiver(receive_handler, tcpSocket);
    std::string current_line; { std::lock_guard<std::mutex> lock(g_coutMutex); std::cout << "Ty: "; }
    while (g_connectionActive) {
        if (_kbhit()) {
            char c = _getch(); if (c == '\r') { if (!current_line.empty()) { if (current_line == "exit") { g_connectionActive = false; } if (send(tcpSocket, current_line.c_str(), current_line.length(), 0) == SOCKET_ERROR) { print_message("\r[B£¥D] Wysy³anie nie powiod³o siê."); g_connectionActive = false; } std::lock_guard<std::mutex> lock(g_coutMutex); std::cout << std::endl << "Ty: "; current_line.clear(); } }
            else if (c == '\b') { if (!current_line.empty()) { current_line.pop_back(); std::lock_guard<std::mutex> lock(g_coutMutex); std::cout << "\b \b"; } }
            else { current_line += c; std::lock_guard<std::mutex> lock(g_coutMutex); std::cout << c; }
        }
        Sleep(50);
    }
    if (receiver.joinable()) { receiver.join(); } closesocket(tcpSocket); print_message("\n[INFO] Sesja czatu zakoñczona.");
}

// =========================================================================
// == NOWA FUNKCJA - DEDYKOWANY W¥TEK NAS£UCHUJ¥CY UDP                     ==
// =========================================================================
void udp_listen_thread(SOCKET udpSocket) {
    char buffer[BUFFER_SIZE];
    sockaddr_in serverAddrFrom;
    int serverAddrSize = sizeof(serverAddrFrom);

    // Ten w¹tek bêdzie dzia³a³ dopóki nie znajdziemy serwera
    while (!g_serverFound) {
        int bytesReceived = recvfrom(udpSocket, buffer, BUFFER_SIZE, 0, (SOCKADDR*)&serverAddrFrom, &serverAddrSize);

        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            if (strcmp(buffer, SERVER_RESPONSE_MSG) == 0) {
                // U¿ywamy mutexa do bezpiecznego zapisu adresu IP
                std::lock_guard<std::mutex> lock(g_serverIpMutex);
                g_serverIp = inet_ntoa(serverAddrFrom.sin_addr);
                
                // Ustawiamy flagê, która zakoñczy pêtlê w main() i w tym w¹tku
                g_serverFound = true; 
                
                print_message("[SUKCES] Otrzymano odpowiedŸ od serwera: " + g_serverIp);
                break; // Koñczymy pracê tego w¹tku
            }
        } else {
             // Jeœli gniazdo zostanie zamkniête przez g³ówny w¹tek, recvfrom zwróci b³¹d.
             // Wtedy po prostu koñczymy ten w¹tek.
            if (!g_serverFound) {
                // Mo¿na zignorowaæ b³êdy timeout, jeœli by³yby ustawione
            }
            break;
        }
    }
}

// =========================================================================
// == PROSTA I NIEZAWODNA WERSJA FUNKCJI `main`                           ==
// =========================================================================
int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    while (true) {
        // Resetowanie flag przed ka¿d¹ prób¹
        g_serverFound = false;

        print_message("\n[INFO] Rozpoczynam szukanie serwera w sieci...");
        SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        
        char broadcast_opt = '1';
        setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &broadcast_opt, sizeof(broadcast_opt));
        char reuse_addr_opt = '1';
        setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_opt, sizeof(reuse_addr_opt));
        
        sockaddr_in agentAddr;
        agentAddr.sin_family = AF_INET;
        agentAddr.sin_port = htons(UDP_PORT);
        agentAddr.sin_addr.s_addr = INADDR_ANY;
        
        if (bind(udpSocket, (SOCKADDR*)&agentAddr, sizeof(agentAddr)) == SOCKET_ERROR) {
            print_message("[B£¥D] Nie uda³o siê zbindowaæ gniazda UDP agenta: " + std::to_string(WSAGetLastError()));
            closesocket(udpSocket);
            Sleep(5000);
            continue;
        }

        // Uruchomienie dedykowanego w¹tku do nas³uchiwania na odpowiedŸ
        std::thread listener(udp_listen_thread, udpSocket);

        // G³ówny w¹tek teraz tylko wysy³a broadcast co 3 sekundy, dopóki serwer nie zostanie znaleziony
        while (!g_serverFound) {
            print_message("[INFO] Wysy³am broadcast...");
            std::vector<std::string> bcast_addrs = get_broadcast_addresses();
            for (const auto& addr : bcast_addrs) {
                 sockaddr_in broadcastAddr;
                 broadcastAddr.sin_family = AF_INET;
                 broadcastAddr.sin_port = htons(UDP_PORT);
                 broadcastAddr.sin_addr.s_addr = inet_addr(addr.c_str());
                 sendto(udpSocket, BROADCAST_MSG, strlen(BROADCAST_MSG), 0, (SOCKADDR*)&broadcastAddr, sizeof(broadcastAddr));
            }
            Sleep(3000); // Czekaj 3 sekundy przed kolejnym broadcastem
        }

        // Gdy serwer zostanie znaleziony, zamykamy gniazdo UDP.
        // To spowoduje, ¿e recvfrom w w¹tku nas³uchuj¹cym zwróci b³¹d i w¹tek siê zakoñczy.
        closesocket(udpSocket);
        // Czekamy, a¿ w¹tek nas³uchuj¹cy na pewno zakoñczy swoj¹ pracê
        if (listener.joinable()) {
            listener.join();
        }

        // Faza TCP (bez zmian)
        print_message("[INFO] Próbujê po³¹czyæ siê z serwerem przez TCP...");
        SOCKET tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in serverTcpAddr;
        serverTcpAddr.sin_family = AF_INET;
        serverTcpAddr.sin_port = htons(TCP_PORT);
        serverTcpAddr.sin_addr.s_addr = inet_addr(g_serverIp.c_str());

        if (connect(tcpSocket, (SOCKADDR*)&serverTcpAddr, sizeof(serverTcpAddr)) == SOCKET_ERROR) {
            std::cerr << "[B£¥D] Nie uda³o siê po³¹czyæ z serwerem TCP: " << WSAGetLastError() << std::endl;
            closesocket(tcpSocket);
            print_message("[INFO] Czekam 5 sekund przed ponownym wyszukaniem...");
            Sleep(5000);
            continue;
        }

        chat_session(tcpSocket);
        print_message("[INFO] Powrót do trybu odkrywania serwera.");
    }

    WSACleanup();
    return 0;
}


