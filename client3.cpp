// client.cpp
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define BROADCAST_PORT 3331
#define BROADCAST_IP "192.168.1.255" // <-- Dopasuj do swojej podsieci!
#define IDENT_MESSAGE "HELLO_SERVER"
#define RESPONSE_MESSAGE "1234"

void chat(SOCKET sock) {
    std::thread recvThread([&]() {
        char buffer[1024];
        while (true) {
            int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) break;
            buffer[bytes] = '\0';
            std::cout << "\r[Serwer] " << buffer << "\n> ";
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

    // 1. UDP socket do broadcastu
    SOCKET udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    BOOL opt = TRUE;
    setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt));

    sockaddr_in bcastAddr{};
    bcastAddr.sin_family = AF_INET;
    bcastAddr.sin_port = htons(BROADCAST_PORT);
    bcastAddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    // 2. Bind na UDP, by odbieraæ odpowiedzi
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(BROADCAST_PORT);
    local.sin_addr.s_addr = INADDR_ANY;
    bind(udpSock, (sockaddr*)&local, sizeof(local));

    // 3. Broadcastuj
    sendto(udpSock, IDENT_MESSAGE, strlen(IDENT_MESSAGE), 0,
           (sockaddr*)&bcastAddr, sizeof(bcastAddr));
    std::cout << "[UDP] Broadcast wys³any\n";

    // 4. Czekaj na odpowiedŸ
    char buf[512];
    sockaddr_in sender{};
    int senderLen = sizeof(sender);
    int bytes = recvfrom(udpSock, buf, sizeof(buf) - 1, 0,
                         (sockaddr*)&sender, &senderLen);
    if (bytes <= 0) {
        std::cerr << "[UDP] Nie otrzymano odpowiedzi\n";
        return 1;
    }

    buf[bytes] = '\0';
    std::cout << "[UDP] Odebrano: " << buf << " od "
              << inet_ntoa(sender.sin_addr) << "\n";

    if (std::string(buf) != RESPONSE_MESSAGE) {
        std::cerr << "[UDP] Nieprawid³owa odpowiedŸ\n";
        return 1;
    }

    // 5. TCP po³¹czenie
    SOCKET tcpSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sender.sin_port = htons(5000); // Port TCP serwera

    if (connect(tcpSock, (sockaddr*)&sender, sizeof(sender)) == SOCKET_ERROR) {
        std::cerr << "[TCP] B³¹d po³¹czenia\n";
        return 1;
    }

    std::cout << "[TCP] Po³¹czono z serwerem\n";
    chat(tcpSock);

    closesocket(udpSock);
    WSACleanup();
    return 0;
}

