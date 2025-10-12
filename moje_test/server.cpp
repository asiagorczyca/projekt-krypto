#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <vector>

#pragma comment (lib, "Ws2_32.lib")

#define IP_ADDRESS "192.168.0.153"
#define DEFAULT_PORT "3504"
#define DEFAULT_BUFLEN 512

struct client_type
{
    int id;
    SOCKET socket;
};

const char OPTION_VALUE = 1;

const int MAX_CLIENTS = 4; 


int process_client(client_type &new_client, std::vector<client_type> &client_array, std::thread &thread);
int main();

int process_client(client_type &new_client, std::vector<client_type> &client_array, std::thread &thread)
{
    std::string msg = "";
    char tempmsg[DEFAULT_BUFLEN] = "";

    
    while (1)
    {
        memset(tempmsg, 0, DEFAULT_BUFLEN);

        if (new_client.socket != 0)
        {
            int iResult = recv(new_client.socket, tempmsg, DEFAULT_BUFLEN, 0);

            if (iResult > 0) 
            {
                if (strcmp("", tempmsg))
                    msg = "Client #" + std::to_string(new_client.id) + ": " + tempmsg;

                std::cout << msg.c_str() << std::endl;

                
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (client_array[i].socket != INVALID_SOCKET)
                        if (new_client.id != i)
                            iResult = send(client_array[i].socket, msg.c_str(), (int)msg.length(), 0);
                }
            }
            else 
            {
                msg = "Client #" + std::to_string(new_client.id) + " Disconnected";

                std::cout << msg << std::endl;

                closesocket(new_client.socket);
                client_array[new_client.id].socket = INVALID_SOCKET;

                
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (client_array[i].socket != INVALID_SOCKET)
                        iResult = send(client_array[i].socket, msg.c_str(), (int)msg.length(), 0);
                }

                break;
            }
        }
    } 

    thread.detach();

    return 0;
}

int main()
{
    WSADATA wsaData;
    struct addrinfo hints;
    struct addrinfo *server = NULL;
    SOCKET server_socket = INVALID_SOCKET;
    std::string msg = "";
    std::vector<client_type> client(MAX_CLIENTS);
    int num_clients = 0;
    int temp_id = -1;
    std::thread my_thread[MAX_CLIENTS];

    
    std::cout << "Intializing Winsock..." << std::endl;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

  
    std::cout << "Setting up server..." << std::endl;
    getaddrinfo(IP_ADDRESS, DEFAULT_PORT, &hints, &server);

   
    std::cout << "Creating server socket..." << std::endl;
    server_socket = socket(server->ai_family, server->ai_socktype, server->ai_protocol);

    
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &OPTION_VALUE, sizeof(int)); 
    setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, &OPTION_VALUE, sizeof(int)); 
    std::cout << "Binding socket..." << std::endl;
    bind(server_socket, server->ai_addr, (int)server->ai_addrlen);

    
    std::cout << "Listening..." << std::endl;
    listen(server_socket, SOMAXCONN);

    
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client[i] = { -1, INVALID_SOCKET };
    }

    while (1)
    {
        SOCKET incoming = INVALID_SOCKET;
        incoming = accept(server_socket, NULL, NULL);

        if (incoming == INVALID_SOCKET) continue;

        
        num_clients = 0; 

     
        temp_id = -1;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client[i].socket == INVALID_SOCKET && temp_id == -1)
            {
                client[i].socket = incoming;
                client[i].id = i;
                temp_id = i;
            }

            if (client[i].socket != INVALID_SOCKET)
                num_clients++;
        }

        if (temp_id != -1)
        {
            
            std::cout << "Client #" << client[temp_id].id << " Accepted" << std::endl;
            msg = std::to_string(client[temp_id].id);
            send(client[temp_id].socket, msg.c_str(), (int)msg.length(), 0);

            
            my_thread[temp_id] = std::thread(process_client, std::ref(client[temp_id]), std::ref(client), std::ref(my_thread[temp_id]));
        }
        else
        {
            msg = "Server is full";
            send(incoming, msg.c_st(), (int)msg.length(), 0);
            std::cout << msg << std::endl;
            closesocket(incoming);
        }
    } 


    
    closesocket(server_socket);

    
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if(my_thread[i].joinable())
            my_thread[i].detach();
        if(client[i].socket != INVALID_SOCKET)
            closesocket(client[i].socket);
    }

    
    WSACleanup();
    std::cout << "Program has ended successfully" << std::endl;

    system("pause");
    return 0;
}
