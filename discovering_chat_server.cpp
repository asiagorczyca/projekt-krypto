#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

void tcp_chat_server(SOCKET clientSocket) {
    std::thread receiver([&]() {
        char buffer[1024];
        int bytes;
        while ((bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytes] = '\0';
            std::cout << "[Klient] " << buffer << std::endl;
        }
    });

    std::string msg;
    while (true) {
        std::getline(std::cin, msg);
        if (msg == "/exit") break;
        send(clientSocket, msg.c_str(), msg.size(), 0);
    }

    closesocket(clientSocket);
    receiver.detach();
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // 1. UDP broadcast nas³uchiwanie
    SOCKET udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in udpAddr{};
    udpAddr.sin_family = AF_INET;
    udpAddr.sin_port = htons(3331);
    udpAddr.sin_addr.s_addr = INADDR_ANY;
    bind(udpSock, (SOCKADDR*)&udpAddr, sizeof(udpAddr));
    std::cout << "[UDP] Serwer nas³uchuje na porcie 3331 (broadcast)...\n";

    char buffer[1024];
    sockaddr_in senderAddr{};
    int senderLen = sizeof(senderAddr);

    while (true) {
        int bytes = recvfrom(udpSock, buffer, sizeof(buffer) - 1, 0, (SOCKADDR*)&senderAddr, &senderLen);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            std::cout << "[UDP] Odebrano od " << inet_ntoa(senderAddr.sin_addr) << ": " << buffer << std::endl;

            if (strcmp(buffer, "hej-serwer") == 0) {
                const char* reply = "1234";
                sendto(udpSock, reply, strlen(reply), 0, (SOCKADDR*)&senderAddr, senderLen);
                std::cout << "[UDP] Wys³ano potwierdzenie do " << inet_ntoa(senderAddr.sin_addr) << "\n";

                // 2. TCP serwer
                SOCKET tcpServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                sockaddr_in tcpAddr{};
                tcpAddr.sin_family = AF_INET;
                tcpAddr.sin_port = htons(4444);
                tcpAddr.sin_addr.s_addr = INADDR_ANY;
                bind(tcpServer, (SOCKADDR*)&tcpAddr, sizeof(tcpAddr));
                listen(tcpServer, 1);
                std::cout << "[TCP] Oczekiwanie na po³¹czenie...\n";

                SOCKET clientSock = accept(tcpServer, nullptr, nullptr);
                std::cout << "[TCP] Po³¹czono z klientem!\n";

                tcp_chat_server(clientSock);
                closesocket(tcpServer);
                break;
            }
        }
    }

    closesocket(udpSock);
    WSACleanup();
    return 0;
}

