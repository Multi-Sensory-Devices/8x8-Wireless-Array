/*
MIT License

Copyright (c) [2023] [Ben Kazemi]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


Multithreaded UDP server by Ben Kazemi for Ryuji Hirayama. UCL 2023

This C++ program utilizes Windows Sockets (Winsock) to perform UDP broadcasting to all IP addresses in a specified subnet.


*/

#include <iostream>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <windows.h>
#include <thread>
#include <atomic>
#include <string>
#include <random>
#include <conio.h>
#include <fcntl.h>// For non-blocking mode

using namespace std;

#pragma comment(lib, "ws2_32.lib") //obj comment to link winsock to the executable 

#define USE_BROADCAST_ADDRESS       0 // use broadcast address in general unless client doesn't support it
#define FREQUENCY_DELAY             1  // delay  =  1 / Freq Delay
#define UPDATE_QUADPART_TICKS       QueryPerformanceCounter(&tick); // update the high-resolution counter
#define TICKS_PER_SEC               ticksPerSecond.QuadPart // retrieve the number of ticks per second
#define SOCKET_PORT                 54007   //this socket will be reserved for your communication, if you can't bind to it during runtime, then try a different socket
#define BIND_IP                     "192.168.0.173"   //this is your servers IP address, aka this computers IP (wherever runtime is)
#define MESSAGE_SIZE                1460  //maximum message size for esp32 (you can change this in the esp32 library but i don't know if there's a hardware limitation)

// Define the subnet range for UDP broadcasting
string subnet = "192.168.0"; // Subnet address without the last octet
int startIP = 202;                   // Starting IP address in the subnet
int endIP = 218;                   // Ending IP address in the subnet



atomic<int> totalPacketsReceived(0);

// Atomic variables for safe multithreading
atomic<unsigned int> frequencyDelay(FREQUENCY_DELAY); 
atomic<bool> broadcastLoopFlag(false);    // atomic flag to kill the broadcast loopers thread
atomic<bool> isAltering(false);   // stop the server loop while we're changing the frequency
atomic<bool> breakLoop(false);    // main loop flag



/*
Clean up winsock resources
*/
void cleanup()
{
    WSACleanup();
}

/*
Random string generator, used to create 'length' long random strings
*/
string generateRandomString(int length)
{
    static const string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<> dis(0, charset.length() - 1);

    string randomString;
    randomString.reserve(length);

    for (int i = 0; i < length; ++i)
    {
        randomString += charset[dis(gen)];
    }

    return randomString;
}

void receiverFunction()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    SOCKET receiverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // scope of whole thread
    if (receiverSocket == INVALID_SOCKET)
    {
        cerr << "Can't create a socket! Quitting" << endl;
        closesocket(receiverSocket);
        return;
    }
    //setsockopt(receiverSocket, IPPROTO_UDP, UDP_NOCHECKSUM, 0, 0);
    //setsockopt(receiverSocket, IPPROTO_TCP, TCP_NODELAY, 0, 0);
    sockaddr_in receiverAddr;
    int SenderAddrSize = sizeof(receiverAddr);
    receiverAddr.sin_family = AF_INET; // ipv4 family
    receiverAddr.sin_port = htons(60000); // listening on a different port
    inet_pton(AF_INET, BIND_IP, &(receiverAddr.sin_addr));

    //bind ip addresses once at the start of this loop, only send data to the bound ip addresses
    if (bind(receiverSocket, (sockaddr*)&receiverAddr, sizeof(receiverAddr)) == SOCKET_ERROR)
    {
        cout << "bind failed with " << BIND_IP << "; Error: " << (int)WSAGetLastError() << endl;
        broadcastLoopFlag.store(true); // Stop the broadcasting thread
        // breakLoop = true; // Exit the main loop
    }
    else
    {
        cout << "              Bound receiver socket to: " << BIND_IP << endl;
    }

    int totalBytesReceived = 0;
    auto startTime = chrono::high_resolution_clock::now();

    // This loop continues until whileFlag is set to true
    while (!broadcastLoopFlag.load())
    {
        char incomingBuffer[MESSAGE_SIZE];
        int bytes_received = recvfrom(receiverSocket, incomingBuffer, sizeof(incomingBuffer), 0 /* no flags*/, /*(SOCKADDR*)&receiverAddr*/0, /*&SenderAddrSize*/ 0);
        //incomingBuffer[MESSAGE_SIZE - 1] = '\0';
        //string stringBuffer = incomingBuffer;
        //      cout << "Received: " << incomingBuffer << "\n" << endl;
        totalBytesReceived += bytes_received;
        //totalPacketsReceived.fetch_sub(1);

        //Calculate elapsed time
        auto endTime = chrono::high_resolution_clock::now();
        auto elapsedTimeBench = chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count();

        //Calculate throughput over the elapsed time
        if (elapsedTimeBench >= 1000)  // Update throughput every 1 second
        {
            double throughput = (static_cast<double>(totalBytesReceived) / elapsedTimeBench) * 1000;  // Bytes per second
            cout << "                                             ReceiverThroughput: " << throughput << " bytes/s" << endl;
            // Reset counters and timer
            totalBytesReceived = 0;
           // totalPacketsReceived = 0;
            startTime = chrono::high_resolution_clock::now();
        }
        //int comparisonValue = stringBuffer.compare(randomString);
        //if (comparisonValue == 0) {
        //    // Strings are equal
        //    cout << "Strings are equal." << endl;
        //}
        //else
        //{
        //    //cout << "str1 is less than str2." << endl;
        //    //isAltering = true;
        //    //cout << "Exiting." << endl;
        //    //broadcastLoopFlag = true; // Stop the broadcasting thread
        //    //breakLoop = true; // Exit the main loop
        //}
    }
    closesocket(receiverSocket);
}


/*
 Broadcasting happens here in seperate thread.
 It continually sends UDP packets to all IP addresses in the subnet until `whileFlag` is set to `true`.
 The frequency of the broadcasting is controlled by `frequencyDelay` (1/n).
*/
void threadLoop()
{
   // SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    /*
    Create a UDP Socket

    First Param = socket family, style and type of address  (ipv4)
    AF_INET means IPv4
    Second Param = socket type, determine kind of packet it can receive
    SOCK_DGRAM means datagrams since udp deals with datagrams
    third param = protocol, related closely to socket type than family,
    since udp protocol must be used with datagram socket but either ipv4 or 6
    */
    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // scope of whole thread
    if (serverSocket == INVALID_SOCKET)
    {
        cerr << "Can't create a socket! Quitting" << endl;
        closesocket(serverSocket);
        return;
    }
    //setsockopt(serverSocket, IPPROTO_UDP, UDP_NOCHECKSUM, 0, 0);
    //setsockopt(serverSocket, IPPROTO_TCP, TCP_NODELAY, 0, 0);

    sockaddr_in serverAddr; // scope of whole thread
    serverAddr.sin_family = AF_INET; // ipv4 family
    serverAddr.sin_port = htons(SOCKET_PORT); //take host byte order (big endian) and return 16 bit network byte; ip port host to network byte                                                              
    //serverAddr.sin_addr.s_addr = INADDR_ANY;
    /*
    inet_pton: ip string to binary representation conversion
    AF_INET: ipv4
    2nd param: returns pointer of char array of string
    3rd param: result store in memory
    */
    inet_pton(AF_INET, BIND_IP, &(serverAddr.sin_addr));

    // Set socket to non-blocking mode
    //unsigned long nonBlockingMode = 1;
    //if (ioctlsocket(serverSocket, FIONBIO, &nonBlockingMode) != 0)
    //{
    //    cerr << "Failed to set socket to non-blocking mode." << endl;
    //    closesocket(serverSocket);
    //    WSACleanup();
    //    return;
    //}

    //bind ip addresses once at the start of this loop, only send data to the bound ip addresses
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cout << "Server bind failed with " << BIND_IP << "; Error: " << (int)WSAGetLastError() << endl;
        broadcastLoopFlag.store(true); // Stop the broadcasting thread
       // breakLoop = true; // Exit the main loop
    }
    else
    {
        cout << "Bound server to: " << BIND_IP << endl;
    }
    
    //string buf(MESSAGE_SIZE, '\0'); // Message to be sent; Initialize buf with null characters
    LARGE_INTEGER ticksPerSecond;
    LARGE_INTEGER tick; // a point in time
    long long lastEnteredTick = 0;
    QueryPerformanceFrequency(&ticksPerSecond);    // get the high-resolution counter's accuracy
    Sleep(500);

    int totalBytesReceived = 0;
    auto startTime = chrono::high_resolution_clock::now();
    string randomString;
    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < 128; j++)
        {
            randomString.append(to_string(j));
        }
    }
    //randomString[5] = 9;

    // This loop continues until whileFlag is set to true
    while (!broadcastLoopFlag.load())
    {

        UPDATE_QUADPART_TICKS; // update the current tick count
        long long elapsedTime = tick.QuadPart - lastEnteredTick;
        long long delay = TICKS_PER_SEC / frequencyDelay.load();
        if (!isAltering.load() && elapsedTime >= delay)
        {
  
            lastEnteredTick = tick.QuadPart; 

            //cout << "Broadcasting" << endl;
            // Iterate over each IP address in the subnet

            for (int ip = startIP; ip <= endIP; ++ip)
            {
                string ipAddress = subnet + "." + to_string(ip); // set the ip string with the final octet

                // Generate a random alphanumeric string for buf
                //string randomString = generateRandomString(MESSAGE_SIZE-1);
                

                //randomString[5] = 8;
                //copy(randomString.begin(), randomString.end(), buf.begin());
                /*
                inet_pton: ip string to binary representation conversion
                AF_INET: ipv4
                2nd param: returns pointer of char array of string
                3rd param: result store in memory
                */
                inet_pton(AF_INET, ipAddress.c_str(), &(serverAddr.sin_addr));
               // cout << "Sending UDP " << randomString + " packet to : " << ipAddress   << endl;
			  
                sendto(serverSocket, randomString.c_str(), randomString.length(), 0, (const sockaddr*)&serverAddr, sizeof(sockaddr_in));//send the UDP packet
                totalBytesReceived += randomString.length();
                //totalPacketsReceived.fetch_add(1);

                //the following is for benchmarking 
                auto endTime_sender_Ind = chrono::high_resolution_clock::now();
                auto elapsedTimeBench_Sender_Ind = chrono::duration_cast<chrono::milliseconds>(endTime_sender_Ind - startTime).count();
                if (elapsedTimeBench_Sender_Ind >= 1000)  // Update throughput every 1 second
                {
                    double throughput = (static_cast<double>(totalBytesReceived) / elapsedTimeBench_Sender_Ind);  // Bytes per second
                    cout << "Total (" << totalBytesReceived << ") Sender Throughput from Server : " << throughput / elapsedTimeBench_Sender_Ind << " MB/s" << endl;
                    //cout << "                  Packet count: " << totalPacketsReceived.load() << endl;
                    // Reset counters and timer
                    totalBytesReceived = 0;
                    //totalPacketsReceived = 0;
                    startTime = chrono::high_resolution_clock::now();
                }




                //cout << "\n" << endl;
            }
            //auto endTime_sender = chrono::high_resolution_clock::now();
            //auto elapsedTimeBench_Sender = chrono::duration_cast<chrono::milliseconds>(endTime_sender - startTime).count();
            //if (elapsedTimeBench_Sender >= 1000)  // Update throughput every 1 second
            //{
            //    double throughput = (static_cast<double>((totalBytesReceived/((endIP - startIP)+1))) / elapsedTimeBench_Sender);  // Bytes per second
            //    cout << "Sender Throughput to each client: " << throughput / elapsedTimeBench_Sender << " MB/s" << endl;
            //    //cout << "                  Packet count: " << totalPacketsReceived.load() << endl;
            //    // Reset counters and timer
            //    totalBytesReceived = 0;
            //    //totalPacketsReceived = 0;
            //    startTime = chrono::high_resolution_clock::now();
            //}

        }
    }
    closesocket(serverSocket);
    
}

/*
initializes Winsock, starts the broadcasting thread, and then enters a loop where it waits for user input to either change the broadcast frequency or exit the program.

*/
int main()
{
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
    // Initialize winsock
    WSADATA wsData; 
    WORD ver = MAKEWORD(2, 2);
    int wsOk = WSAStartup(ver, &wsData); //responsible for retrieving details of the winsock implementation that got linked into the executable
    // Check for winsock initialization failure
    if (wsOk != 0)
    {
        cerr << "Can't initialize winsock! Quitting program" << endl;
        return 1;
    }

    // use broadcast address in general unless client doesn't support it
    if (USE_BROADCAST_ADDRESS) 
    { 
        startIP = 255;
        endIP = 255;
    } 

    // Create and start the broadcasting thread
    thread myThread(threadLoop);//execute threadLoop function in new thread
   // thread receiverThread(receiverFunction);
    // Instructions for the user
    cout << "c: change frequency (1/n)\ne: exit program\n" << endl;

    // This loop waits for user commands
    while (!breakLoop.load())
    {
        char ch = _getch(); //retrieves the pressed key without requiring the Enter key
        //if (_kbhit()) // function checks if a key has been pressed
        //{
         //   cout << "kEY: " << ch << endl;
            
            switch (ch)
            {
            case 'c':
                isAltering.store(true); // exit the broadcast threads inner loop
                cout << "Enter your desired frequency (1/n): " << endl;
                unsigned int userInput; // store the user input for frequency delay
                cin >> userInput;
                frequencyDelay.store(userInput); // change the frequency here
                isAltering = false;
                break;
            case 'e':
                isAltering.store(true);
                cout << "Exiting." << endl;
                broadcastLoopFlag.store(true); // Stop the broadcasting thread
                breakLoop.store(true); // Exit the main loop
            }
        //}
    }
    myThread.join();  // Wait for the broadcasting thread to finish
    //receiverThread.join();
    
    // Properly clean up resources
    cleanup();

    return 0;
}