#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>

#pragma comment(lib, "ws2_32.lib")

void recv_tcp_messages(SOCKET tcp_sock) {
    char buf[1024];
    while (true) {
        int received = recv(tcp_sock, buf, sizeof(buf) - 1, 0);
        if (received <= 0) {
            std::cout << "Connection closed by peer or error.\n";
            break;
        }
        buf[received] = '\0';
        std::cout << "Peer: " << buf << std::endl;
    }
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock == INVALID_SOCKET) {
        std::cerr << "UDP socket creation failed.\n";
        return 1;
    }

    int reuse = 1;
    setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    sockaddr_in udp_addr{};
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(3331);
    udp_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_sock, (sockaddr*)&udp_addr, sizeof(udp_addr)) == SOCKET_ERROR) {
        std::cerr << "UDP bind failed.\n";
        closesocket(udp_sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Listening for broadcast on UDP port 3331...\n";

    char buf[1024];
    sockaddr_in sender_addr{};
    int sender_len = sizeof(sender_addr);
    int tcp_port = -1;
    std::string sender_ip;

    while (true) {
        int bytes = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0,
                             (sockaddr*)&sender_addr, &sender_len);
        if (bytes <= 0) continue;

        buf[bytes] = '\0';
        std::string msg(buf);
        sender_ip = inet_ntoa(sender_addr.sin_addr);
        std::cout << "Received broadcast from " << sender_ip << ":" << ntohs(sender_addr.sin_port)
                  << " - '" << msg << "'\n";

        if (msg.rfind("HELLO", 0) == 0) {
            size_t pos = msg.find(' ');
            if (pos != std::string::npos) {
                try {
                    tcp_port = std::stoi(msg.substr(pos + 1));
                    std::string reply = "1234";
                    sendto(udp_sock, reply.c_str(), reply.length(), 0,
                           (sockaddr*)&sender_addr, sender_len);
                    std::cout << "Sent reply '1234' to " << sender_ip << ":" << ntohs(sender_addr.sin_port) << "\n";
                    break;
                } catch (...) {
                    std::cerr << "Invalid port in HELLO message.\n";
                }
            }
        }
    }

    // Po³¹czenie TCP
    SOCKET tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in tcp_addr{};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(tcp_port);
    inet_pton(AF_INET, sender_ip.c_str(), &tcp_addr.sin_addr);

    std::cout << "Connecting to " << sender_ip << ":" << tcp_port << " via TCP...\n";

    if (connect(tcp_sock, (sockaddr*)&tcp_addr, sizeof(tcp_addr)) == SOCKET_ERROR) {
        std::cerr << "TCP connection failed.\n";
        closesocket(udp_sock);
        closesocket(tcp_sock);
        WSACleanup();
        return 1;
    }

    std::cout << "TCP connection established.\n";

    std::thread recv_thread(recv_tcp_messages, tcp_sock);
    recv_thread.detach();

    std::cout << "You can now chat. Type messages and press Enter.\n";

    std::string line;
    while (true) {
        std::getline(std::cin, line);
        if (!line.empty()) {
            send(tcp_sock, line.c_str(), line.length(), 0);
        }
        if (line == "exit") break;
    }

    closesocket(tcp_sock);
    closesocket(udp_sock);
    WSACleanup();
    std::cout << "Chat ended.\n";
    return 0;
}

