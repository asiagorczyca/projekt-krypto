#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

#define BROADCAST_PORT 33331
#define TCP_START_PORT 10007
#define TCP_END_PORT 10017

void udp_listener() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket == INVALID_SOCKET) {
        std::cerr << "[UDP] Socket error.\n";
        return;
    }

    sockaddr_in recv_addr;
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(BROADCAST_PORT);
    recv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_socket, (sockaddr*)&recv_addr, sizeof(recv_addr)) == SOCKET_ERROR) {
        std::cerr << "[UDP] Bind failed.\n";
        closesocket(udp_socket);
        return;
    }

    std::cout << "[UDP] Nas³uchiwanie na porcie " << BROADCAST_PORT << "...\n";

    while (true) {
        char buffer[1024] = { 0 };
        sockaddr_in sender;
        int sender_len = sizeof(sender);

        int len = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&sender, &sender_len);
        if (len > 0) {
            buffer[len] = '\0';
            std::cout << "[UDP] Otrzymano: " << buffer << "\n";
            if (std::string(buffer) == "HELLO_CONTROLLER") {
                const char* reply = "CONTROLLER_ACK";
                sendto(udp_socket, reply, strlen(reply), 0, (sockaddr*)&sender, sender_len);
                std::cout << "[UDP] Wys³ano CONTROLLER_ACK do " << inet_ntoa(sender.sin_addr) << "\n";
            }
        }
    }

    closesocket(udp_socket);
    WSACleanup();
}

void handle_client(SOCKET client_socket, sockaddr_in client_addr) {
    char buffer[1024];
    std::cout << "[TCP] Po³¹czenie od " << inet_ntoa(client_addr.sin_addr) << "\n";

    while (true) {
        int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        std::cout << "[TCP] Otrzymano: " << buffer << "\n";

        const char* reply = "ACK_FROM_CONTROLLER";
        send(client_socket, reply, strlen(reply), 0);
    }

    closesocket(client_socket);
    std::cout << "[TCP] Roz³¹czono.\n";
}

void tcp_listener(int port) {
    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        std::cerr << "[TCP] Socket error na porcie " << port << "\n";
        return;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[TCP] Bind error na porcie " << port << "\n";
        closesocket(listen_socket);
        return;
    }

    listen(listen_socket, SOMAXCONN);
    std::cout << "[TCP] Nas³uchiwanie na porcie " << port << "...\n";

    while (true) {
        sockaddr_in client;
        int client_size = sizeof(client);
        SOCKET client_socket = accept(listen_socket, (sockaddr*)&client, &client_size);
        if (client_socket != INVALID_SOCKET) {
            std::thread(handle_client, client_socket, client).detach();
        }
    }

    closesocket(listen_socket);
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    std::thread udp_thread(udp_listener);

    std::vector<std::thread> tcp_threads;
    for (int port = TCP_START_PORT; port <= TCP_END_PORT; ++port) {
        tcp_threads.emplace_back(tcp_listener, port);
    }

    std::cout << "[INFO] Kontroler uruchomiony.\n";

    udp_thread.join();
    for (auto& t : tcp_threads) {
        t.join();
    }

    WSACleanup();
    return 0;
}

