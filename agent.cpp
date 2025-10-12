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

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

// --- Konfiguracja ---
const int UDP_PORT = 54321;
const int TCP_PORT = 54322;
const char* BROADCAST_MSG = "CZY_KTOKOLWIEK_MNIE_SLYSZY_AGENT_V4_FIX";
const char* SERVER_RESPONSE_MSG = "TAK_SLYSZE_CIE_SERVER_V4_FIX";
const int BUFFER_SIZE = 1024;
// --- Koniec Konfiguracji ---

// ... reszta kodu pomocniczego (print_message, get_broadcast_addresses, chat) jest bez zmian ...
// ... wklejam dla kompletnoœci ...

std::atomic<bool> g_connectionActive(false);
std::mutex g_coutMutex;

void print_message(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_coutMutex);
    std::cout << msg << std::endl;
}

std::vector<std::string> get_broadcast_addresses() {
    std::vector<std::string> addresses;
    ULONG bufferSize = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
    if (pAddresses == NULL) return addresses;

    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &bufferSize) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses != NULL; pCurrAddresses = pCurrAddresses->Next) {
            if (pCurrAddresses->IfType == IF_TYPE_SOFTWARE_LOOPBACK || pCurrAddresses->OperStatus != IfOperStatusUp) continue;
            for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != NULL; pUnicast = pUnicast->Next) {
                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* sa_in = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                    ULONG ip_addr = sa_in->sin_addr.S_un.S_addr;
                    ULONG subnet_mask = 0;
                    ConvertLengthToIpv4Mask(pUnicast->OnLinkPrefixLength, &subnet_mask); 
                    ULONG broadcast_addr = ip_addr | ~subnet_mask;
                    in_addr broadcast_in_addr;
                    broadcast_in_addr.S_un.S_addr = broadcast_addr;
                    addresses.push_back(inet_ntoa(broadcast_in_addr));
                }
            }
        }
    }
    if (pAddresses) free(pAddresses);
    addresses.push_back("255.255.255.255");
    return addresses;
}

void receive_handler(SOCKET tcpSocket) { /* bez zmian */
    char buffer[BUFFER_SIZE];
    while (g_connectionActive) {
        int bytesReceived = recv(tcpSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived > 0) {
            print_message("Serwer: " + std::string(buffer, bytesReceived));
        } else {
            if (g_connectionActive) {
                print_message("[INFO] Po³¹czenie z serwerem zosta³o zerwane.");
                g_connectionActive = false;
            }
            break;
        }
    }
}
void chat_session(SOCKET tcpSocket) { /* bez zmian */
    g_connectionActive = true;
    print_message("[INFO] Po³¹czono z serwerem. Mo¿esz zacz¹æ czatowaæ. Napisz 'exit' aby zakoñczyæ.");
    std::thread receiver(receive_handler, tcpSocket);
    std::string line;
    while (g_connectionActive) {
        std::getline(std::cin, line);
        if (!g_connectionActive) break;
        if (line == "exit") g_connectionActive = false;
        if (send(tcpSocket, line.c_str(), line.length(), 0) == SOCKET_ERROR) {
            print_message("[B£¥D] Nie uda³o siê wys³aæ wiadomoœci: " + std::to_string(WSAGetLastError()));
            g_connectionActive = false;
        }
    }
    if (receiver.joinable()) receiver.join();
    closesocket(tcpSocket);
    print_message("[INFO] Sesja czatu zakoñczona.");
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    while (true) {
        print_message("\n[INFO] Rozpoczynam szukanie serwera w sieci...");
        SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        
        char broadcast_opt = '1';
        setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &broadcast_opt, sizeof(broadcast_opt));
        
        // NOWOŒÆ: Pozwól na wspó³dzielenie portu (wa¿ne dla broadcastu)
        char reuse_addr_opt = '1';
        if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_opt, sizeof(reuse_addr_opt)) == SOCKET_ERROR) {
            print_message("[OSTRZE¯ENIE] Nie uda³o siê ustawiæ SO_REUSEADDR.");
        }
        
        // NOWOŒÆ: Bindowanie gniazda agenta do tego samego portu, na którym nas³uchuje serwer
        sockaddr_in agentAddr;
        agentAddr.sin_family = AF_INET;
        agentAddr.sin_port = htons(UDP_PORT);
        agentAddr.sin_addr.s_addr = INADDR_ANY; // Nas³uchuj na wszystkich interfejsach
        
        if (bind(udpSocket, (SOCKADDR*)&agentAddr, sizeof(agentAddr)) == SOCKET_ERROR) {
            print_message("[B£¥D] Nie uda³o siê zbindowaæ gniazda UDP agenta: " + std::to_string(WSAGetLastError()));
            closesocket(udpSocket);
            Sleep(5000);
            continue;
        }

        DWORD timeout = 5000;
        setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

        std::vector<std::string> bcast_addrs = get_broadcast_addresses();
        
        sockaddr_in serverAddrFrom;
        int serverAddrSize = sizeof(serverAddrFrom);
        char buffer[BUFFER_SIZE];
        std::string serverIp;

        while (true) {
            print_message("[INFO] Wysy³am broadcast...");
            for (const auto& addr : bcast_addrs) {
                 sockaddr_in broadcastAddr;
                 broadcastAddr.sin_family = AF_INET;
                 broadcastAddr.sin_port = htons(UDP_PORT);
                 broadcastAddr.sin_addr.s_addr = inet_addr(addr.c_str());
                 sendto(udpSocket, BROADCAST_MSG, strlen(BROADCAST_MSG), 0, (SOCKADDR*)&broadcastAddr, sizeof(broadcastAddr));
            }
            
            // Teraz recvfrom powinno zadzia³aæ, bo gniazdo jest zbindowane i firewall wie, czego siê spodziewaæ
            int bytesReceived = recvfrom(udpSocket, buffer, BUFFER_SIZE, 0, (SOCKADDR*)&serverAddrFrom, &serverAddrSize);

            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                if (strcmp(buffer, SERVER_RESPONSE_MSG) == 0) {
                    serverIp = inet_ntoa(serverAddrFrom.sin_addr);
                    print_message("[SUKCES] Otrzymano odpowiedŸ od serwera: " + serverIp);
                    break;
                }
            } else {
                if (WSAGetLastError() == WSAETIMEDOUT) {
                    print_message("[INFO] Brak odpowiedzi, ponawiam za 10 sekund...");
                    Sleep(10000);
                } else {
                    std::cerr << "[B£¥D] B³¹d recvfrom: " << WSAGetLastError() << std::endl;
                    Sleep(10000);
                }
            }
            Sleep(1000);
        }
        closesocket(udpSocket);

        // Faza TCP (bez zmian)
        print_message("[INFO] Próbujê po³¹czyæ siê z serwerem przez TCP...");
        SOCKET tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        
        sockaddr_in serverTcpAddr;
        serverTcpAddr.sin_family = AF_INET;
        serverTcpAddr.sin_port = htons(TCP_PORT);
        serverTcpAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());

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
