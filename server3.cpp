// server.cpp
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define BROADCAST_PORT 3331
#define TCP_PORT 5000
#define IDENT_MESSAGE "HELLO_SERVER"
#define RESPONSE_MESSAGE "1234"

void chat(SOCKET sock) {
    std::thread recvThread([&]() {
        char buffer[1024];
        while (true) {
            int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) break;
            buffer[bytes] = '\0';
            std::cout << "\r[Klient] " << buffer << "\n> ";
        }
    });

    std::string msg;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, msg);
        if (msg == "exit") break;
        send(sock, msg.c_str(), msg.size(), 0);
    }

    closesocket(sock);
    recvThread.join();
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // 1. UDP socket
    SOCKET udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in udpAddr{};
    udpAddr.sin_family = AF_INET;
    udpAddr.sin_port = htons(BROADCAST_PORT);
    udpAddr.sin_addr.s_addr = INADDR_ANY;
    bind(udpSock, (sockaddr*)&udpAddr, sizeof(udpAddr));

    // 2. Czekaj na wiadomoœæ
    char buf[512];
    sockaddr_in client{};
    int clientLen = sizeof(client);
    int bytes = recvfrom(udpSock, buf, sizeof(buf) - 1, 0,
                         (sockaddr*)&client, &clientLen);
    if (bytes <= 0) {
        std::cerr << "[UDP] B³¹d odbierania\n";
        return 1;
    }

    buf[bytes] = '\0';
    std::cout << "[UDP] Odebrano: " << buf << " od "
              << inet_ntoa(client.sin_addr) << "\n";

    if (std::string(buf) == IDENT_MESSAGE) {
        sendto(udpSock, RESPONSE_MESSAGE, strlen(RESPONSE_MESSAGE), 0,
               (sockaddr*)&client, sizeof(client));
        std::cout << "[UDP] OdpowiedŸ wys³ana\n";
    }

    // 3. TCP nas³uch
    SOCKET tcpSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(TCP_PORT);
    srv.sin_addr.s_addr = INADDR_ANY;
    bind(tcpSock, (sockaddr*)&srv, sizeof(srv));
    listen(tcpSock, 1);

    std::cout << "[TCP] Oczekiwanie na po³¹czenie...\n";
    SOCKET clientSock = accept(tcpSock, nullptr, nullptr);
    if (clientSock == INVALID_SOCKET) {
        std::cerr << "[TCP] B³¹d akceptowania\n";
        return 1;
    }

    std::cout << "[TCP] Klient po³¹czony\n";
    chat(clientSock);

    closesocket(udpSock);
    closesocket(tcpSock);
    WSACleanup();
    return 0;
}

