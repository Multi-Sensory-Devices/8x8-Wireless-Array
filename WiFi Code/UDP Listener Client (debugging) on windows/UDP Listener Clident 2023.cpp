#include <iostream>
#include <chrono>  // For time measurement
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <io.h>
#include <fcntl.h>// For non-blocking mode
#include <thread>
#include <string>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define SERVER_IP "192.168.0.5" // Replace with the IP address of your server
#define SERVER_PORT 54000
#define MESSAGE_SIZE 1460
#define PATTERN_REPEATS 5

SOCKET clientSocket;

atomic<int> totalBytesReceived(0);
atomic<int> totalPacketsReceived(0);
auto startTime = std::chrono::high_resolution_clock::now();

atomic<bool> breakLoop(false);
string expectedPattern;

bool verifyNumberString(string inputString)
{
    //cout << "expected Pattern: \n" << expectedPattern << endl;
    //cout << "Input String: \n" << inputString << endl;

    if (inputString.size() < expectedPattern.size()) {
        return false;  // Input string is too short
    }

    //inputString[5] = 9;
    for (int i = 0; i < PATTERN_REPEATS; i++)
    {
        for (int j = 0; j < 64; j++)
        {
            if (inputString[(i * 128) + j] != expectedPattern[(i * 128) + j])
            {
                cout << "no match at pointer: " << (i * 128) + j << " value: " << inputString[(i * 128) + j] << endl;
                return false;
            }
        }
    }
    return true;  // Pattern is not valid
}

void receiverFunction()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    // Create a UDP socket
    clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET)
    {
        std::cerr << "Failed to create socket." << std::endl;
        WSACleanup();
        breakLoop.store(true);
        return;
    }

    // Set socket to non-blocking mode
    //unsigned long nonBlockingMode = 1;
    //if (ioctlsocket(clientSocket, FIONBIO, &nonBlockingMode) != 0)
    //{
    //    std::cerr << "Failed to set socket to non-blocking mode." << std::endl;
    //    closesocket(clientSocket);
    //    WSACleanup();
    //    breakLoop = true;
    //    return;
    //}

    // Bind the socket to any available port on the client side
    sockaddr_in clientAddress;
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_addr.s_addr = INADDR_ANY;
    clientAddress.sin_port = htons(54000);

    if (bind(clientSocket, (sockaddr*)&clientAddress, sizeof(clientAddress)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to bind socket." << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        breakLoop.store(true);
        return;
    }

    // Set up the server address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(60000);
    inet_pton(AF_INET, SERVER_IP, &(serverAddress.sin_addr));
    int serverAddrSize = sizeof(serverAddress);

    // Receive and display packets from the server
    char buffer[MESSAGE_SIZE];
    for (int i = 0; i < MESSAGE_SIZE; i++)
    {
        buffer[i] = '\0';
    }
    while (true)
    {
        int bytesReceived = recvfrom(clientSocket, buffer, sizeof(buffer), 0, /*(SOCKADDR*)&clientAddress, &serverAddrSize*/0,0);

        if (bytesReceived > 0)
        {
            totalBytesReceived.fetch_add(bytesReceived);
            //totalPacketsReceived.fetch_add(1);

            if (verifyNumberString(buffer)) {
                //printf("Match\n");
            }
            else {
                std::cout << "The string does not follow the required pattern." << std::endl;
                breakLoop.store(true);
                break;
            }

            //std::cout << "Received: \n" << buffer << std::endl;
            //std::cout << "Received: "<< std::endl;
            //sendto(clientSocket, buffer, sizeof(buffer), 0, (const sockaddr*)&serverAddress, serverAddrSize);
        }
        else
        {
            std::cout << "receied bytes count: "<< bytesReceived << std::endl;
        }


 
    }

    // Cleanup
    closesocket(clientSocket);
    WSACleanup();
}

int main()
{
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
    // Initialize Winsock
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0)
    {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return 1;
    }
    for (int i = 0; i < PATTERN_REPEATS; i++)
    {
        for (int j = 0; j < 128; j++)
        {
            expectedPattern.append(to_string(j));
        }
    }
    std::thread readerThread(receiverFunction);
 

    // Throughput measurement variables


    // Receive and display packets from the server
    //char buffer[897];
    while (!breakLoop.load())
    {


        //Calculate elapsed time
        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        //Calculate throughput over the elapsed time
        if (elapsedTime >= 1000)  // Update throughput every 1 second
        {
            double throughput = (static_cast<double>(totalBytesReceived.load()) / elapsedTime);  // Bytes per second
            std::cout << "                                              Throughput: " << throughput /1000<< " MB/s" << std::endl;

            // Reset counters and timer
            totalBytesReceived.store(0);
            //totalPacketsReceived = 0;
            startTime = std::chrono::high_resolution_clock::now();
        }
    }

    // Cleanup
    readerThread.join();
    WSACleanup();

    return 0;
}
