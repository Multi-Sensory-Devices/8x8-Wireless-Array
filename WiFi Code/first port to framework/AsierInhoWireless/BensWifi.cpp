#include <BensWifi.h>

using namespace std;

/*
initializes Winsock, starts the broadcasting thread, and then enters a loop where it waits for user input to either change the broadcast frequency or exit the program.
*/
int BensWifi::begin()
{
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
    bindServer();
}

/*
Clean up winsock resources
*/
void BensWifi::cleanup()
{
	WSACleanup();
}

void BensWifi::error() {
    // DO ERROR STUFF HERE
}

void BensWifi::receiverFunction()
{
    //SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    //SOCKET receiverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // scope of whole thread
    //if (receiverSocket == INVALID_SOCKET)
    //{
    //    cerr << "Can't create a socket! Quitting" << endl;
    //    closesocket(receiverSocket);
    //    return;
    //}
    ////setsockopt(receiverSocket, IPPROTO_UDP, UDP_NOCHECKSUM, 0, 0);
    ////setsockopt(receiverSocket, IPPROTO_TCP, TCP_NODELAY, 0, 0);
    //sockaddr_in receiverAddr;
    //int SenderAddrSize = sizeof(receiverAddr);
    //receiverAddr.sin_family = AF_INET; // ipv4 family
    //receiverAddr.sin_port = htons(60000);
    //inet_pton(AF_INET, BIND_IP, &(receiverAddr.sin_addr));

    ////bind ip addresses once at the start of this loop, only send data to the bound ip addresses
    //if (bind(receiverSocket, (sockaddr*)&receiverAddr, sizeof(receiverAddr)) == SOCKET_ERROR)
    //{
    //    cout << "bind failed with " << BIND_IP << "; Error: " << (int)WSAGetLastError() << endl;
    //    //broadcastLoopFlag.store(true); // Stop the broadcasting thread
    //    error();
    //}
    //else
    //{
    //    cout << "              Bound receiver socket to: " << BIND_IP << endl;
    //}

    //int totalBytesReceived = 0;
    //auto startTime = chrono::high_resolution_clock::now();

    //// This loop continues until whileFlag is set to true
    //while (!broadcastLoopFlag.load())
    //{
    //    char incomingBuffer[MESSAGE_SIZE];
    //    int bytes_received = recvfrom(receiverSocket, incomingBuffer, sizeof(incomingBuffer), 0 /* no flags*/, /*(SOCKADDR*)&receiverAddr*/0, /*&SenderAddrSize*/ 0);
    //    //incomingBuffer[MESSAGE_SIZE - 1] = '\0';
    //    //string stringBuffer = incomingBuffer;
    //    //      cout << "Received: " << incomingBuffer << "\n" << endl;
    //    totalBytesReceived += bytes_received;
    //    //totalPacketsReceived.fetch_sub(1);

    //    //Calculate elapsed time
    //    auto endTime = chrono::high_resolution_clock::now();
    //    auto elapsedTimeBench = chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count();

    //    //Calculate throughput over the elapsed time
    //    if (elapsedTimeBench >= 1000)  // Update throughput every 1 second
    //    {
    //        double throughput = (static_cast<double>(totalBytesReceived) / elapsedTimeBench) * 1000;  // Bytes per second
    //        cout << "                                             ReceiverThroughput: " << throughput << " bytes/s" << endl;
    //        // Reset counters and timer
    //        totalBytesReceived = 0;
    //        // totalPacketsReceived = 0;
    //        startTime = chrono::high_resolution_clock::now();
    //    }
    //    //int comparisonValue = stringBuffer.compare(randomString);
    //    //if (comparisonValue == 0) {
    //    //    // Strings are equal
    //    //    cout << "Strings are equal." << endl;
    //    //}
    //    //else
    //    //{
    //    //    //cout << "str1 is less than str2." << endl;
    //    //    //isAltering = true;
    //    //    //cout << "Exiting." << endl;
    //    //    //broadcastLoopFlag = true; // Stop the broadcasting thread
    //    //    //breakLoop = true; // Exit the main loop
    //    //}
    //}
    //closesocket(receiverSocket);
}

/*
 Broadcasting happens here in seperate thread.
 It continually sends UDP packets to all IP addresses in the subnet until `whileFlag` is set to `true`.
 The frequency of the broadcasting is controlled by `frequencyDelay` (1/n).
*/
void BensWifi::bindServer()
{
    serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // scope of whole thread
    if (serverSocket == INVALID_SOCKET)
    {
        cerr << "Can't create a socket! Quitting" << endl;
        closesocket(serverSocket);
        return;
    }


    serverAddr.sin_family = AF_INET; // ipv4 family
    serverAddr.sin_port = htons(SOCKET_PORT); //take host byte order (big endian) and return 16 bit network byte; ip port host to network byte                                                              

    inet_pton(AF_INET, BIND_IP, &(serverAddr.sin_addr));

    //bind ip addresses once at the start of this loop, only send data to the bound ip addresses
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cout << "Server bind failed with " << BIND_IP << "; Error: " << (int)WSAGetLastError() << endl;
        error();
    }
    else
    {
        cout << "Bound server to: " << BIND_IP << endl;
    }

    Sleep(500);
    isServerBound = true; 
    //closesocket(serverSocket);
}

void BensWifi::sendString(string input) {
    //consider throwing this consumer into a seperate thread
    if (BensWifi::isServerBound == true)
    {
        for (int ip = BensWifi::startIP; ip <= BensWifi::endIP; ++ip)
        {
            string ipAddress = BensWifi::subnet + "." + to_string(ip); // set the ip string with the final octet
            inet_pton(AF_INET, ipAddress.c_str(), &(BensWifi::serverAddr.sin_addr));
            // cout << "Sending UDP " << randomString + " packet to : " << ipAddress   << endl;
            sendto(BensWifi::serverSocket, input.c_str(), input.length(), 0, (const sockaddr*)&BensWifi::serverAddr, sizeof(sockaddr_in));//send the UDP packet
        }
    }
}