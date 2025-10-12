#include <winsock2.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

void receive_messages(SOCKET clientSocket) {
    char buffer[1024];
    int bytesReceived;

    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        std::cout << "[Client] " << buffer << std::endl;
    }

    std::cout << "Po³¹czenie zakoñczone.\n";
    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    SOCKET serverSocket, clientSocket;
    sockaddr_in serverAddr{}, clientAddr{};
    int clientAddrLen = sizeof(clientAddr);

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "B³¹d tworzenia socketu.\n";
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(4444);

    bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 1);

    std::cout << "Oczekiwanie na po³¹czenie...\n";
    clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);
    std::cout << "Po³¹czono!\n";

    std::thread recvThread(receive_messages, clientSocket);

    std::string msg;
    while (true) {
        std::getline(std::cin, msg);
        if (msg == "/exit") break;
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }

    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}

