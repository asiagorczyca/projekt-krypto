// Plik: ProgramA.cpp
// Wersja zmergowana: Watchdog i Worker w jednym pliku.
// POPRAWIONA - usuniêto polskie znaki z kodu Ÿród³owego, aby unikn¹æ b³êdów kompilacji.

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <windows.h> 

#pragma comment(lib, "ws2_32.lib")

// --- Sta³e i zmienne globalne dla trybu WORKER ---
const int DISCOVERY_PORT = 54321;
const int BASE_TCP_PORT = 12345;
const std::string DISCOVERY_MESSAGE_HEADER = "CHAT_DISCOVERY;PORT=";
std::atomic<bool> g_stopBroadcasting(false);
std::atomic<bool> g_chatActive(false);

// --- Funkcje pomocnicze dla trybu WORKER ---

void handle_worker_error(const char* message, bool continue_running = false) {
    std::cerr << "[WORKER] " << message << " Blad: " << WSAGetLastError() << std::endl;
    if (!continue_running) {
        WSACleanup();
        exit(1);
    }
}

void broadcast_discovery(int tcp_port) {
    SOCKET broadcastSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (broadcastSocket == INVALID_SOCKET) {
        handle_worker_error("Nie udalo sie utworzyc gniazda broadcastowego.", true);
        return;
    }
    char broadcast = '1';
    setsockopt(broadcastSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    
    sockaddr_in broadcastAddr;
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(DISCOVERY_PORT);
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
    std::string message = DISCOVERY_MESSAGE_HEADER + std::to_string(tcp_port);

    std::cout << "[WORKER] Rozpoczynam rozglaszanie na porcie UDP " << DISCOVERY_PORT << std::endl;
    while (!g_stopBroadcasting) {
        sendto(broadcastSocket, message.c_str(), message.length(), 0, (SOCKADDR*)&broadcastAddr, sizeof(broadcastAddr));
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    std::cout << "[WORKER] Zakonczono rozglaszanie." << std::endl;
    closesocket(broadcastSocket);
}

void receive_messages(SOCKET clientSocket) {
    char buffer[4096];
    while (g_chatActive) {
        int bytesReceived = recv(clientSocket, buffer, 4096, 0);
        if (bytesReceived > 0) {
            std::cout << "\rKolega/Kolezanka: " << std::string(buffer, 0, bytesReceived) << std::endl << "Ty: " << std::flush;
        } else {
            std::cout << "\r[WORKER] Polaczenie zostalo zerwane." << std::endl;
            g_chatActive = false;
            break;
        }
    }
}

void send_messages(SOCKET clientSocket) {
    std::string userInput;
    while (g_chatActive) {
        std::cout << "Ty: ";
        std::getline(std::cin, userInput);
        if (!g_chatActive) break;
        if (!userInput.empty()) {
            if (userInput == "exit") {
                g_chatActive = false;
            }
            if (send(clientSocket, userInput.c_str(), userInput.size(), 0) == SOCKET_ERROR) {
                std::cout << "[WORKER] BLAD: Nie udalo sie wyslac wiadomosci." << std::endl;
                break;
            }
        }
    }
}

void runWorkerLogic() {
    std::cout << "--- URUCHOMIONO W TRYBIE WORKER ---" << std::endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        handle_worker_error("Inicjalizacja Winsock nie powiodla sie.");
        return;
    }

    while (true) {
        SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) {
             handle_worker_error("Nie udalo sie utworzyc gniazda TCP.", true);
             std::this_thread::sleep_for(std::chrono::seconds(10));
             continue;
        }

        int currentTcpPort = BASE_TCP_PORT;
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        while (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) != 0) {
            if (WSAGetLastError() == WSAEADDRINUSE) {
                currentTcpPort++;
                serverAddr.sin_port = htons(currentTcpPort);
            } else {
                 handle_worker_error("Krytyczny blad bindowania.", true);
                 closesocket(listenSocket);
                 std::this_thread::sleep_for(std::chrono::seconds(30));
                 continue;
            }
        }
        std::cout << "[WORKER] Serwer TCP zwiazany z portem: " << currentTcpPort << std::endl;
        
        listen(listenSocket, SOMAXCONN);

        g_stopBroadcasting = false;
        std::thread discoveryThread(broadcast_discovery, currentTcpPort);

        std::cout << "[WORKER] Czekam na polaczenie od klienta..." << std::endl;
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);

        g_stopBroadcasting = true;
        discoveryThread.join();
        closesocket(listenSocket);

        if (clientSocket != INVALID_SOCKET) {
            std::cout << "[WORKER] Klient polaczony! Mozna rozpoczac czat." << std::endl;
            std::cout << "Wpisz 'exit', aby zakonczyc." << std::endl;
            std::cout << "------------------------------------------" << std::endl;

            g_chatActive = true;
            std::thread receiver(receive_messages, clientSocket);
            std::thread sender(send_messages, clientSocket);
            receiver.join();
            sender.join();

            closesocket(clientSocket);
            std::cout << "[WORKER] Sesja czatu zakonczona." << std::endl;
        }

        std::cout << "[WORKER] Rozpocznie ponowne nasluchiwanie i rozglaszanie za 30 sekund..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }

    WSACleanup();
}

void runWatchdogLogic() {
    // Ustawienie strony kodowej konsoli na UTF-8, aby poprawnie wyœwietlaæ œcie¿ki z nietypowymi znakami
    // Ta funkcja jest bezpieczna nawet jeœli polskie znaki zostan¹ usuniête z litera³ów.
    SetConsoleOutputCP(CP_UTF8);

    std::wcout << L"--- URUCHOMIONO W TRYBIE WATCHDOG ---" << std::endl;
    std::wcout << L"Bede pilnowal, aby proces potomny (worker) byl zawsze aktywny." << std::endl;
    std::wcout << L"Aby zatrzymac wszystko, zamknij to okno (watchdoga)." << std::endl;

    wchar_t ownPath[MAX_PATH];
    GetModuleFileNameW(NULL, ownPath, MAX_PATH);

    std::wstring commandLine = L"\"";
    commandLine += ownPath;
    commandLine += L"\" --worker";

    while (true) {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::wcout << L"\n[WATCHDOG] Uruchamiam proces workera..." << std::endl;

        if (!CreateProcessW(NULL, (LPWSTR)commandLine.c_str(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
            std::wcerr << L"[WATCHDOG] Blad! Nie udalo sie uruchomic procesu workera. Kod bledu: " << GetLastError() << std::endl;
            Sleep(10000);
            continue;
        }

        std::wcout << L"[WATCHDOG] Proces workera uruchomiony. Czekam na jego zakonczenie." << std::endl;
        WaitForSingleObject(pi.hProcess, INFINITE);
        std::wcout << L"[WATCHDOG] Wykryto zamkniecie workera. Restartuje za 3 sekundy..." << std::endl;

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        Sleep(3000);
    }
}

int main(int argc, char* argv[]) {
	SetConsoleOutputCP(CP_UTF8);
    if (argc > 1 && std::string(argv[1]) == "--worker") {
        runWorkerLogic();
    } else {
        runWatchdogLogic();
    }
    return 0;
}
