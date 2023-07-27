//#ifndef _COM_TOOLKIT2
//#define _COM_TOOLKIT2  //FIX THIS

#include <iostream>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <windows.h>
#include <thread>
#include <atomic>
#include <string>
#include <random>
#include <conio.h>
//sorry, i don't know which libraries you might be already including

using namespace std;

#pragma comment(lib, "ws2_32.lib") //obj comment to link winsock to the executable 

#define USE_BROADCAST_ADDRESS       0 // use broadcast address in general unless client doesn't support it
#define FREQUENCY_DELAY             1  // delay  =  1 / Freq Delay
#define UPDATE_QUADPART_TICKS       QueryPerformanceCounter(&tick); // update the high-resolution counter
#define TICKS_PER_SEC               ticksPerSecond.QuadPart // retrieve the number of ticks per second
#define SOCKET_PORT                 54007
#define BIND_IP                     "192.168.0.5"
#define MESSAGE_SIZE                1460

/**
	Class for SPI connection
*/
class BensWifi {
	// Define the subnet range for UDP broadcasting


	atomic<int> totalPacketsReceived;
	// Atomic variables for safe multithreading
	atomic<unsigned int> frequencyDelay(unsigned int);
	atomic<bool> isAltering;   // stop the server loop while we're changing the frequency
	atomic<bool> breakLoop;    // main loop flag
	

public:
	static string subnet; // Subnet address without the last octet
	void receiverFunction();
	static bool isServerBound;
	void error();
	static int startIP;                   // Starting IP address in the subnet
	static int endIP;                   // Ending IP address in the subnet
	atomic<bool> broadcastLoopFlag;    // atomic flag to kill the broadcast loopers thread
	static SOCKET serverSocket; 
	static sockaddr_in serverAddr;
	void sendString(string input);
	BensWifi(string _subnet, int _startIP, int _endIP) {
		string subnet = _subnet;
		int startIP = _startIP;
		int endIP = _endIP;
		isServerBound = false; 
		begin();
	};
private:
	void cleanup();
	int begin();
	void bindServer();

};



//
//#endif