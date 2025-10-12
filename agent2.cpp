#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

int main() {
    WSADATA wsaData;
    // Inicjalizacja Winsock2
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    // Gniazdo UDP do wysy³ania broadcastu
    SOCKET udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSock == INVALID_SOCKET) {
        std::cerr << "Error creating UDP socket: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }
    // W³¹czenie opcji broadcast
    char broadcast = 1;
    if (setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        std::cerr << "Failed to set SO_BROADCAST\n";
        closesocket(udpSock);
        WSACleanup();
        return 1;
    }
    // Zwi¹zanie UDP do dowolnego wolnego portu Ÿród³owego
    sockaddr_in udpAddr;
    udpAddr.sin_family = AF_INET;
    udpAddr.sin_addr.s_addr = INADDR_ANY;
    udpAddr.sin_port = htons(0);
    if (bind(udpSock, (sockaddr*)&udpAddr, sizeof(udpAddr)) == SOCKET_ERROR) {
        std::cerr << "UDP bind failed: " << WSAGetLastError() << "\n";
        closesocket(udpSock);
        WSACleanup();
        return 1;
    }

    // Gniazdo TCP nas³uchuj¹ce
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "Error creating TCP socket: " << WSAGetLastError() << "\n";
        closesocket(udpSock);
        WSACleanup();
        return 1;
    }
    sockaddr_in tcpAddr;
    tcpAddr.sin_family = AF_INET;
    tcpAddr.sin_addr.s_addr = INADDR_ANY;
    tcpAddr.sin_port = htons(0); // 0 = dowolny wolny port
    if (bind(listenSock, (sockaddr*)&tcpAddr, sizeof(tcpAddr)) == SOCKET_ERROR) {
        std::cerr << "TCP bind failed: " << WSAGetLastError() << "\n";
        closesocket(udpSock);
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }
    // Pobranie numeru portu przydzielonego po bind
    int addrLen = sizeof(tcpAddr);
    if (getsockname(listenSock, (sockaddr*)&tcpAddr, &addrLen) == SOCKET_ERROR) {
        std::cerr << "getsockname failed: " << WSAGetLastError() << "\n";
        closesocket(udpSock);
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }
    int tcpPort = ntohs(tcpAddr.sin_port);
    std::cout << "TCP listening on port " << tcpPort << "\n";
    // Nas³uch TCP
    if (listen(listenSock, 1) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << "\n";
        closesocket(udpSock);
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    // Adres broadcastowy (255.255.255.255:3331)
    sockaddr_in broadcastAddr;
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcastAddr.sin_port = htons(3331);

    // Przygotowanie wiadomoœci broadcastowej z portem TCP
    std::string helloMsg = "HELLO " + std::to_string(tcpPort);
    std::cout << "Broadcasting discovery messages every 30 seconds...\n";

    // Ustawiamy timeout oczekiwania na recvfrom (30 sekund)
    int timeout = 30000;
    setsockopt(udpSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    // Pêtla wysy³ania broadcastów
    char recvBuf[512];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);
    while (true) {
        // Wyœlij pakiet broadcast
        int sent = sendto(udpSock, helloMsg.c_str(), (int)helloMsg.size(), 0,
                          (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
        if (sent == SOCKET_ERROR) {
            std::cerr << "sendto failed: " << WSAGetLastError() << "\n";
        } else {
            std::cout << "Sent broadcast: " << helloMsg << "\n";
        }

        // Czekaj na odpowiedŸ UDP
        int recvBytes = recvfrom(udpSock, recvBuf, sizeof(recvBuf)-1, 0,
                                 (sockaddr*)&fromAddr, &fromLen);
        if (recvBytes == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                std::cout << "No reply, will broadcast again...\n";
                continue;
            } else {
                std::cerr << "recvfrom failed: " << err << "\n";
                continue;
            }
        }
        recvBuf[recvBytes] = '\0';
        std::string reply(recvBuf);
        std::cout << "Received UDP from " << inet_ntoa(fromAddr.sin_addr)
                  << ":" << ntohs(fromAddr.sin_port) << " - \"" << reply << "\"\n";
        if (reply == "1234") {
            std::cout << "Discovery response received.\n";
            break;
        }
    }

    std::cout << "Waiting for TCP connection...\n";
    // Akceptuj po³¹czenie TCP od nadawcy broadcastu
    SOCKET clientSock = accept(listenSock, nullptr, nullptr);
    if (clientSock == INVALID_SOCKET) {
        std::cerr << "Accept failed: " << WSAGetLastError() << "\n";
        closesocket(udpSock);
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }
    std::cout << "TCP connection established.\n";

    // W¹tek odbieraj¹cy wiadomoœci TCP
    std::thread recvThread([&]() {
        char buf[512];
        while (true) {
            int bytes = recv(clientSock, buf, sizeof(buf)-1, 0);
            if (bytes <= 0) {
                std::cout << "Connection closed or error.\n";
                break;
            }
            buf[bytes] = '\0';
            std::cout << "Peer: " << buf << "\n";
        }
    });

    // Wysy³anie wiadomoœci wpisanych przez u¿ytkownika
    std::cout << "You can now chat. Type messages and press Enter.\n";
    std::string input;
    while (true) {
        std::getline(std::cin, input);
        if (input.empty()) continue;
        if (input == "exit") break;
        int sent = send(clientSock, input.c_str(), (int)input.size(), 0);
        if (sent == SOCKET_ERROR) {
            std::cerr << "Send failed: " << WSAGetLastError() << "\n";
            break;
        }
    }

    // Sprz¹tanie i zamykanie gniazd
    closesocket(clientSock);
    closesocket(listenSock);
    closesocket(udpSock);
    WSACleanup();
    recvThread.join();
    return 0;
}

