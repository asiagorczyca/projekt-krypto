#include <winsock2.h> // Podstawowe funkcje Winsock
#include <ws2tcpip.h> // Funkcje do obs³ugi adresów IP (getaddrinfo)
#include <iostream>   // Do obs³ugi wejœcia/wyjœcia (cin, cout)
#include <string>     // Do u¿ycia std::string
#include <thread>     // Do obs³ugi w¹tków
#include <vector>     // Do potencjalnych list klientów (tutaj nie u¿yte, ale dobre w sieci)
#include <chrono>     // Do timingu, jeœli potrzebny

// Linkowanie biblioteki Winsock2. Jest to odpowiednik -lws2_32 w g++.
#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512            // Domyœlna d³ugoœæ bufora dla wiadomoœci
#define IP_ADDRESS "192.168.0.153"    // Adres IP serwera, z którym klient próbuje siê po³¹czyæ
#define DEFAULT_PORT "3504"           // Port serwera

// Struktura przechowuj¹ca informacje o kliencie
struct client_type
{
    SOCKET socket; // Deskryptor gniazda dla tego klienta
    int id;        // ID klienta nadane przez serwer
    char received_message[DEFAULT_BUFLEN]; // Bufor na odbierane wiadomoœci
};

// Deklaracja funkcji, która bêdzie dzia³aæ w osobnym w¹tku,
// odpowiedzialna za odbieranie wiadomoœci od serwera.
int process_client(client_type &new_client);

// Funkcja main (g³ówna) programu klienckiego
int main()
{
    // --- Inicjalizacja Winsock ---
    WSAData wsa_data; // Struktura do przechowywania informacji o implementacji Winsock
    // U¿ywamy MAKEWORD(2,2) do ¿¹dania wersji Winsock 2.2
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (iResult != 0) {
        std::cerr << "WSAStartup() failed with error: " << iResult << std::endl; // U¿yj cerr dla b³êdów
        return 1; // Zakoñcz program z b³êdem
    }

    // --- Konfiguracja adresu serwera ---
    struct addrinfo *result = NULL, // WskaŸnik do listy struktur addrinfo
                    *ptr = NULL,    // Pomocniczy wskaŸnik do iteracji po liœcie
                    hints;          // Struktura do okreœlenia kryteriów dla getaddrinfo

    ZeroMemory(&hints, sizeof(hints)); // Wyzeruj strukturê hints
    hints.ai_family = AF_UNSPEC;       // Pozwól na IPv4 lub IPv6
    hints.ai_socktype = SOCK_STREAM;   // Typ gniazda: strumieniowe (TCP)
    hints.ai_protocol = IPPROTO_TCP;   // Protokó³: TCP

    std::cout << "Starting Client...\n";
    std::cout << "Connecting to " << IP_ADDRESS << ":" << DEFAULT_PORT << "...\n";

    // Rozwi¹¿ adres IP serwera i port
    iResult = getaddrinfo(IP_ADDRESS, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo() failed with error: " << iResult << std::endl;
        WSACleanup(); // Zwolnij zasoby Winsock
        system("pause"); // Wstrzymaj program, aby u¿ytkownik móg³ zobaczyæ b³¹d
        return 1;
    }

    // --- Tworzenie gniazda i ³¹czenie z serwerem ---
    client_type client = { INVALID_SOCKET, -1, "" }; // Inicjalizacja struktury klienta

    // Próbuj po³¹czyæ siê z ka¿dym adresem zwróconym przez getaddrinfo
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        // Utwórz gniazdo dla po³¹czenia z serwerem
        client.socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (client.socket == INVALID_SOCKET) {
            std::cerr << "socket() failed with error: " << WSAGetLastError() << std::endl;
            WSACleanup();
            system("pause");
            return 1;
        }

        // Po³¹cz siê z serwerem.
        iResult = connect(client.socket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(client.socket); // Zamknij gniazdo, jeœli po³¹czenie siê nie powiod³o
            client.socket = INVALID_SOCKET;
            std::cout << "Connection attempt failed for " << IP_ADDRESS << ". Trying next address (if any).\n";
            continue; // Spróbuj kolejnego adresu
        }
        break; // Po³¹czenie udane, wyjdŸ z pêtli
    }

    freeaddrinfo(result); // Zwolnij pamiêæ zaalokowan¹ przez getaddrinfo

    if (client.socket == INVALID_SOCKET) {
        std::cerr << "Unable to connect to server! No address succeeded.\n";
        WSACleanup();
        system("pause");
        return 1;
    }

    std::cout << "Successfully Connected to " << IP_ADDRESS << ":" << DEFAULT_PORT << std::endl;

    // --- Odbierz ID klienta od serwera ---
    // Serwer powinien wys³aæ ID klienta zaraz po po³¹czeniu
    iResult = recv(client.socket, client.received_message, DEFAULT_BUFLEN, 0);
    if (iResult > 0) {
        client.received_message[iResult] = '\0'; // Upewnij siê, ¿e string jest zakoñczony zerem
        std::string message_from_server = client.received_message;

        if (message_from_server != "Server is full")
        {
            client.id = atoi(client.received_message); // Konwertuj odebrane ID na int
            std::cout << "Received client ID: " << client.id << std::endl;

            // --- Uruchom w¹tek do odbierania wiadomoœci ---
            // Tworzymy nowy w¹tek, który bêdzie nas³uchiwa³ wiadomoœci od serwera
            std::thread my_thread(process_client, std::ref(client)); // std::ref jest wa¿ne dla przekazywania referencji do w¹tku

            // --- Pêtla wysy³ania wiadomoœci ---
            std::string sent_message_str; // Zmieniono nazwê, aby unikn¹æ kolizji z globalnym sent_message

            while (true) // Pêtla nieskoñczona, dopóki u¿ytkownik nie zdecyduje siê wyjœæ
            {
                sent_message_str = ""; // Wyzeruj wiadomoœæ przed pobraniem nowej
                std::cout << "JA: "; // <--- Znak zachêty dla u¿ytkownika
                std::getline(std::cin, sent_message_str); // Pobierz ca³¹ liniê od u¿ytkownika

                // Sprawdzenie, czy u¿ytkownik chce wyjœæ
                if (sent_message_str == "exit" || sent_message_str == "quit") {
                    std::cout << "Exiting chat...\n";
                    break; // WyjdŸ z pêtli wysy³ania
                }

                // Dodatkowa walidacja: nie wysy³aj pustych wiadomoœci
                if (sent_message_str.empty()) {
                    continue; // Pomiñ wysy³anie, jeœli wiadomoœæ jest pusta
                }

                // Wysy³anie wiadomoœci do serwera
                iResult = send(client.socket, sent_message_str.c_str(), (int)sent_message_str.length(), 0);

                if (iResult == SOCKET_ERROR)
                {
                    std::cerr << "send() failed: " << WSAGetLastError() << std::endl;
                    break; // WyjdŸ z pêtli wysy³ania w przypadku b³êdu
                }
                 if (iResult == 0) // Polaczenie zamkniete przez serwer
                {
                    std::cout << "Server closed the connection." << std::endl;
                    break;
                }
            }

            // Od³¹cz w¹tek odbieraj¹cy, aby dzia³a³ niezale¿nie lub poczekaj na niego.
            // join() by³oby lepsze, ale wymaga³oby jakiegoœ mechanizmu do sygnalizowania zakoñczenia.
            // detach() powoduje, ¿e w¹tek bêdzie dzia³a³ w tle, a¿ siê zakoñczy lub program siê zamknie.
            if (my_thread.joinable()) {
                // Aby bezpiecznie zakoñczyæ w¹tek odbiorczy, najlepiej wys³aæ jakiœ sygna³
                // lub zamkn¹æ gniazdo. Proste detach() mo¿e spowodowaæ, ¿e w¹tek bêdzie próbowa³
                // czytaæ z zamkniêtego gniazda.
                // Na razie pozostawimy detach dla prostoty, ale pamiêtaj o tym w produkcyjnym kodzie.
                my_thread.detach();
            }

        }
        else // Serwer pe³ny
        {
            std::cout << message_from_server << std::endl;
        }
    } else if (iResult == 0) {
        std::cerr << "Server closed connection while waiting for ID.\n";
    } else {
        std::cerr << "recv() failed while waiting for ID: " << WSAGetLastError() << std::endl;
    }


    // --- Zamykanie po³¹czenia i czyszczenie zasobów ---
    std::cout << "Shutting down socket..." << std::endl;
    // Wyœlij sygna³, ¿e nie bêdziemy ju¿ wysy³aæ danych
    iResult = shutdown(client.socket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "shutdown() failed with error: " << WSAGetLastError() << std::endl;
    }

    closesocket(client.socket); // Zamknij gniazdo
    WSACleanup();               // Zwolnij zasoby Winsock
    system("pause");            // Wstrzymaj program przed zamkniêciem konsoli
    return 0;                   // Zakoñcz program pomyœlnie
}

// Implementacja funkcji odbieraj¹cej wiadomoœci w osobnym w¹tku
int process_client(client_type &new_client)
{
    while (true) // Pêtla nieskoñczona do odbierania wiadomoœci
    {
        memset(new_client.received_message, 0, DEFAULT_BUFLEN); // Wyzeruj bufor

        if (new_client.socket == INVALID_SOCKET) {
             // W¹tek mo¿e próbowaæ dzia³aæ, gdy gniazdo jest ju¿ zamkniête/nieprawid³owe
             // Dobrze by³oby mieæ tu mechanizm do sygnalizowania zakoñczenia w¹tku.
            break;
        }

        int iResult = recv(new_client.socket, new_client.received_message, DEFAULT_BUFLEN - 1, 0); // Odbierz wiadomoœæ
        // Odeœlij n-1 bajtów, aby zostawiæ miejsce na '\0'

        if (iResult > 0) // Wiadomoœæ odebrana pomyœlnie
        {
            new_client.received_message[iResult] = '\0'; // Upewnij siê, ¿e string jest zakoñczony zerem
            std::cout << "\nSerwer: " << new_client.received_message << std::endl; // Wypisz wiadomoœæ
            std::cout << "JA: " << std::flush; // Ponownie wypisz zachêtê, aby u¿ytkownik wiedzia³, ¿e mo¿e pisaæ
                                              // flush() wymusza natychmiastowe wyœwietlenie
        }
        else if (iResult == 0) // Serwer zamkn¹³ po³¹czenie
        {
            std::cout << "\nSerwer zamknal polaczenie." << std::endl;
            break; // WyjdŸ z pêtli odbierania
        }
        else // B³¹d odbierania
        {
            int error_code = WSAGetLastError();
            if (error_code == WSAECONNRESET) { // Serwer zresetowa³ po³¹czenie
                std::cout << "\nPolaczenie zostalo zresetowane przez serwer." << std::endl;
            } else {
                std::cerr << "\nrecv() failed in thread: " << error_code << std::endl;
            }
            break; // WyjdŸ z pêtli odbierania
        }
    }

    return 0;
}
