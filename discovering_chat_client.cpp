#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

void tcp_chat(SOCKET sock) {
    std::thread receiver([&]() {
        char buffer[1024];
        int bytes;
        while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytes] = '\0';
            std::cout << "[Serwer] " << buffer << std::endl;
        }
    });

    std::string msg;
    while (true) {
        std::getline(std::cin, msg);
        if (msg == "/exit") break;
        send(sock, msg.c_str(), msg.size(), 0);
    }

    closesocket(sock);
    receiver.detach();
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    BOOL broadcastEnable = TRUE;
    setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastEnable, sizeof(broadcastEnable));

    sockaddr_in bcastAddr{};
    bcastAddr.sin_family = AF_INET;
    bcastAddr.sin_port = htons(3331);
    bcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");

    char recvBuffer[1024];
    sockaddr_in fromAddr{};
    int fromLen = sizeof(fromAddr);

    std::cout << "[UDP] Szukanie serwera w sieci LAN...\n";

    while (true) {
        const char* msg = "hej-serwer";
        sendto(udpSock, msg, strlen(msg), 0, (SOCKADDR*)&bcastAddr, sizeof(bcastAddr));

        int bytes = recvfrom(udpSock, recvBuffer, sizeof(recvBuffer) - 1, 0, (SOCKADDR*)&fromAddr, &fromLen);
        if (bytes > 0) {
            recvBuffer[bytes] = '\0';
            if (strcmp(recvBuffer, "1234") == 0) {
                std::string serverIp = inet_ntoa(fromAddr.sin_addr);
                std::cout << "[UDP] Znaleziono serwer pod adresem " << serverIp << "\n";

                // Po³¹czenie TCP
                SOCKET tcpSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                sockaddr_in serverAddr{};
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(4444);
                serverAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());

                std::cout << "[TCP] £¹czenie do " << serverIp << ":4444...\n";
                if (connect(tcpSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == 0) {
                    std::cout << "[TCP] Po³¹czono z serwerem!\n";
                    tcp_chat(tcpSock);
                } else {
                    std::cerr << "[TCP] B³¹d po³¹czenia.\n";
                }
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    closesocket(udpSock);
    WSACleanup();
    return 0;
}

