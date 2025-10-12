// Plik: ProgramB.cpp
// Kompilacja w Visual Studio: Pamiêtaj o dodaniu `ws2_32.lib` do zale¿noœci linkera.

#define _WIN32_WINNT 0x0600 // WA¯NE: Definicja dla nowszych funkcji Winsock

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

// Port, na którym nas³uchujemy na broadcast UDP
const int DISCOVERY_PORT = 54321;
// Wiadomoœæ, której szukamy w broadcastach
const std::string DISCOVERY_MESSAGE_HEADER = "CHAT_DISCOVERY;PORT=";

std::atomic<bool> g_chatActive(false);

void handle_error(const char* message) {
    std::cerr << message << " B³¹d: " << WSAGetLastError() << std::endl;
}

// W¹tek do odbierania wiadomoœci czatu
void receive_messages(SOCKET serverSocket) {
    char buffer[4096];
    while (g_chatActive) {
        int bytesReceived = recv(serverSocket, buffer, 4096, 0);
        if (bytesReceived > 0) {
            std::cout << "\rKolega/Kole¿anka: " << std::string(buffer, 0, bytesReceived) << std::endl << "Ty: " << std::flush;
        }
        else if (bytesReceived == 0) {
            std::cout << "\r[INFO] Po³¹czenie zamkniête przez serwer." << std::endl;
            g_chatActive = false;
            break;
        }
        else {
             if (WSAGetLastError() != WSAEWOULDBLOCK) {
                std::cout << "\r[B£¥D] B³¹d recv(). Koñczenie czatu." << std::endl;
                g_chatActive = false;
                break;
            }
        }
    }
}

// W¹tek do wysy³ania wiadomoœci czatu
void send_messages(SOCKET serverSocket) {
    std::string userInput;
    while (g_chatActive) {
        std::cout << "Ty: ";
        std::getline(std::cin, userInput);

        if (!g_chatActive) break;

        if (!userInput.empty()) {
            if (userInput == "exit") {
                g_chatActive = false;
            }

            int sendResult = send(serverSocket, userInput.c_str(), userInput.size(), 0);
            if (sendResult == SOCKET_ERROR) {
                std::cout << "[B£¥D] Nie uda³o siê wys³aæ wiadomoœci." << std::endl;
                g_chatActive = false;
                break;
            }
        }
    }
}

int main() {
	SetConsoleOutputCP(CP_UTF8);
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        handle_error("Inicjalizacja Winsock nie powiod³a siê.");
        return 1;
    }

    // 1. Utwórz gniazdo UDP do nas³uchiwania na broadcasty
    SOCKET listenSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (listenSocket == INVALID_SOCKET) {
        handle_error("Nie uda³o siê utworzyæ gniazda UDP.");
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DISCOVERY_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        handle_error("Nie uda³o siê zbindowaæ gniazda UDP.");
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "[INFO] Nas³uchujê na broadcasty na porcie UDP " << DISCOVERY_PORT << "..." << std::endl;

    char buffer[1024];
    sockaddr_in senderAddr;
    int senderAddrSize = sizeof(senderAddr);
    int serverTcpPort = 0;
    std::string serverIpAddress;

    // 2. Czekaj na w³aœciwy broadcast
    while (true) {
        int bytesReceived = recvfrom(listenSocket, buffer, 1024, 0, (SOCKADDR*)&senderAddr, &senderAddrSize);
        if (bytesReceived > 0) {
            std::string receivedMsg(buffer, bytesReceived);
            
            // SprawdŸ, czy to nasza wiadomoœæ
            if (receivedMsg.rfind(DISCOVERY_MESSAGE_HEADER, 0) == 0) {
                // Wyodrêbnij port
                std::string portStr = receivedMsg.substr(DISCOVERY_MESSAGE_HEADER.length());
                try {
                    serverTcpPort = std::stoi(portStr);
                    
                    // Wyodrêbnij adres IP
                    char ipStr[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &senderAddr.sin_addr, ipStr, INET_ADDRSTRLEN); // Teraz powinno dzia³aæ
                    serverIpAddress = ipStr;
                    
                    std::cout << "[INFO] Odkryto serwer czatu pod adresem " << serverIpAddress << " na porcie TCP " << serverTcpPort << std::endl;
                    break; // Mamy serwer, wychodzimy z pêtli
                }
                catch (const std::invalid_argument& e) {
                    std::cerr << "[OSTRZE¯ENIE] Otrzymano nieprawid³owy format portu w wiadomoœci: " << receivedMsg << std::endl;
                }
            }
        }
    }
    
    closesocket(listenSocket); // Gniazdo nas³uchuj¹ce UDP nie jest ju¿ potrzebne

    // 3. Nawi¹¿ po³¹czenie TCP z serwerem
    SOCKET connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectSocket == INVALID_SOCKET) {
        handle_error("Nie uda³o siê utworzyæ gniazda TCP do po³¹czenia.");
        WSACleanup();
        return 1;
    }

    sockaddr_in connectAddr;
    connectAddr.sin_family = AF_INET;
    connectAddr.sin_port = htons(serverTcpPort);
    inet_pton(AF_INET, serverIpAddress.c_str(), &connectAddr.sin_addr); // Teraz powinno dzia³aæ

    std::cout << "[INFO] Próbujê po³¹czyæ siê z serwerem..." << std::endl;
    if (connect(connectSocket, (SOCKADDR*)&connectAddr, sizeof(connectAddr)) == SOCKET_ERROR) {
        handle_error("Po³¹czenie z serwerem nie powiod³o siê.");
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    // 4. Po³¹czenie nawi¹zane, przejdŸ do czatu
    std::cout << "[INFO] Po³¹czono! Mo¿na rozpocz¹æ czat." << std::endl;
    std::cout << "Wpisz 'exit', aby zakoñczyæ." << std::endl;
    std::cout << "------------------------------------------" << std::endl;

    g_chatActive = true;
    std::thread receiver(receive_messages, connectSocket);
    std::thread sender(send_messages, connectSocket);

    receiver.join();
    sender.join();

    // 5. Sprz¹tanie
    closesocket(connectSocket);
    WSACleanup();
    std::cout << "[INFO] Program zakoñczony." << std::endl;

    return 0;
}
