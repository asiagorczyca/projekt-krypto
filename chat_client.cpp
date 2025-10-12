#include <winsock2.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

void receive_messages(SOCKET sock) {
    char buffer[1024];
    int bytesReceived;

    while ((bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        std::cout << "[Server] " << buffer << std::endl;
    }

    std::cout << "Po³¹czenie przerwane.\n";
    closesocket(sock);
}

int main() {
    WSADATA wsaData;
    SOCKET clientSocket;
    sockaddr_in serverAddr{};

    std::string serverIp;
    int port = 4444;

    std::cout << "Podaj IP serwera: ";
    std::cin >> serverIp;
    std::cin.ignore(); // czyœcimy bufor

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "B³¹d tworzenia socketu.\n";
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());

    std::cout << "£¹czenie do " << serverIp << ":" << port << "...\n";
    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "B³¹d po³¹czenia.\n";
        return 1;
    }

    std::cout << "Po³¹czono z serwerem!\n";
    std::thread recvThread(receive_messages, clientSocket);

    std::string msg;
    while (true) {
        std::getline(std::cin, msg);
        if (msg == "/exit") break;
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}

