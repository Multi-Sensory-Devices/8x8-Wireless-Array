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

  WORKING NO BUFFER 1300 PACKETS PER SECOND SPI
  1.5MBs
  ESP32 UDP packet receiver by Ben Kazemi for Ryuji Hirayama @ MSD group UCL London 2023

*/

// using VSPI SPI
#include <CircularBuffer.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Ticker.h>  // not always aligning with one second of transfers


#define BENCHMARK_ENABLED 1
#define PATTERN_REPEATS 5        //frames per packet
#define PACKET_SIZE 1460         //Can increase this with kconfig
#define BENCHMARK_INTERVAL 1000  //seconds
#define FRAME_SIZE 1370
#define SERVER_PACKETS_PER_SECOND 120
#define SERIAL_BUFFER_RX 1460
#define SERIAL_USB_BAUD 1000000
#define SERIAL_1_BAUD 1000000
#define UDP_PORT 54007
#define UDP_IP "192.168.0.173"
#define VSPI_MISO MISO
#define VSPI_MOSI MOSI
#define VSPI_SCLK SCK
#define VSPI_SS SS

static const int spiClk = 20000000;  // 40 MHz - 25mhz is good
SPIClass* vspi = NULL;               //uninitalised pointers to SPI objects

Ticker benchmark;

CircularBuffer<char, 20000> packetBuf;  //538 byte overhead, 100 KB capacity

// WiFi network name and password:
const char* networkName = "PickMe";
const char* networkPswd = "31642576";

//IP address to send UDP data to:
// either use the ip address of the server or
// a network broadcast address
const char* udpAddress = UDP_IP;
const int udpPort = UDP_PORT;

volatile int totalBytesRx = 0;

String expectedPattern;

//Are we currently connected?
boolean connected = false;

//The udp library class
WiFiUDP udp;
char packet[PACKET_SIZE];

void setup() {
  delay(2000);
  Serial.begin(SERIAL_USB_BAUD);
  //  Serial1.begin(SERIAL_1_BAUD);
  //  Serial1.setRxBufferSize(SERIAL_BUFFER_RX);
  //  Serial1.setTxBufferSize(SERIAL_BUFFER_RX);


  vspi = new SPIClass(VSPI);
  vspi->begin();
  pinMode(vspi->pinSS(), OUTPUT);  //VSPI SS

  connectToWiFi(networkName, networkPswd);


  for (int i = 0; i < PATTERN_REPEATS; i++) {
    for (int j = 0; j < 128; j++) {
      expectedPattern.concat(j);
    }
  }
  Serial.println(expectedPattern);
  if (BENCHMARK_ENABLED)
    benchmark.attach_ms(BENCHMARK_INTERVAL, benchmarker);
}

void benchmarker() {
  float throughput = ((float(totalBytesRx) / 1000) / 1000);
  float packetsLostPerSecond = abs((float(FRAME_SIZE) * float(SERVER_PACKETS_PER_SECOND)) - float(totalBytesRx)) / float(FRAME_SIZE);
  float percentageLoss = (float(packetsLostPerSecond) / float(SERVER_PACKETS_PER_SECOND)) * 100.00;
  if (totalBytesRx == 0) {
    packetsLostPerSecond = 0;
    percentageLoss = 0;
  }
  Serial.print("\n\nPacket loss estimate: ");
  Serial.print(packetsLostPerSecond);  // 1000 packets at 1370 without overhead per send
  Serial.print(" ");
  Serial.print(percentageLoss, 3);
  Serial.println(" %");

  Serial.print("Throughput: ");
  Serial.print(throughput, 5);
  Serial.println(" MB/s\n");
  totalBytesRx = 0;
}

void spiCommand(SPIClass* spi, byte data) {
  //use it as you would the regular arduino SPI API
  spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  // digitalWrite(spi->pinSS(), LOW);  //pull SS slow to prep other end for transfer
  spi->transfer(data);
  // digitalWrite(spi->pinSS(), HIGH);  //pull ss high to signify end of data transfer
  spi->endTransaction();
}

void loop() {
  
  // spiCommand(vspi, expectedPattern);  // write expectedPattern
  
  if (connected) {

    int packetSize = udp.parsePacket();

    // Received packets go to buffer
    if (packetSize > 0) {
      int len = udp.read(packet, PACKET_SIZE);

      //for the benchmarker
      totalBytesRx += len; // uncomment if you want to benchmark the input
      //uncomment this line if you want to skip the verification from the server, only for debugging
//      if (!verifyNumberString(packet)) {
//        Serial.println("err");
//        connected = false;
//      }
      //send the data over SPI
      vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
      vspi->transfer(packet, len);
      vspi->endTransaction();

      //uncomment these few lines if you need a C string
      // if (len > 0) //removed incase it bothers the buffer - make this a delimiter if needed
      // {
      //   packet[len] = '\0';
      // }

      // uncomment these several lines if you want to use the buffer
      // if (packetBuf.available() < FRAME_SIZE) {
      //   Serial.println("          Buffer capacity too small");
      //   return;
      // }


      // for (int i = 0; i < len; i++) {
      //   if (!packetBuf.push(packet[i])) {
      //     Serial.println("          an overwrite occured!!!");
      //   }
      // }
      //   }

      //uncomment this block of code if you are using the buffer
      //We shift buffer to FPGA
      // if (packetBuf.size() > FRAME_SIZE * 5)  // debugging purpose, shift out when hit a certain size
      // {
      //   //Serial.print("\nshifted:");
      //   vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));

      //   for (int i = 0; i < FRAME_SIZE * 5; i++) {  //shift this much out - ask ryuji for number
      //     totalBytesRx++;                           //benchmarking
      //                                               // Serial1.write(packetBuf.shift()); // uncomment if you want to benchmark the output
      //     // vspi->transfer(packetBuf.shift());
      //     // packetBuf.shift();
      //   }
      //   // vspi->endTransaction();
      // }
      // }
    }
  }
}

void connectToWiFi(const char* ssid, const char* pwd) {
  Serial.println("Connecting to WiFi network: " + String(ssid));

  // delete old config
  WiFi.disconnect(true);
  //register event handler
  WiFi.onEvent(WiFiEvent);

  //Initiate connection
  WiFi.begin(ssid, pwd);

  Serial.println("Waiting for WIFI connection...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
}

bool verifyNumberString(String inputString) {
  if (inputString.length() != expectedPattern.length()) {
    return false;  // Input string is too short
  }
  for (int i = 0; i < PATTERN_REPEATS; i++) {
    for (int j = 0; j < 64; j++) {
      if (inputString[(i * 128) + j] != expectedPattern[(i * 128) + j]) {
        char buf[60];
        sprintf(buf, "No match at pointer: %d value: %d", (i * 128) + j, inputString[(i * 128) + j]);
        Serial.println(buf);
        return false;
      }
    }
  }
  return true;  // Pattern is not valid
}

//wifi event handler
void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      //initializes the UDP state
      //This initializes the transfer buffer
      udp.begin(WiFi.localIP(), udpPort);
      connected = true;
      WiFi.setTxPower(WIFI_POWER_19_5dBm);
      //When connected set
      Serial.print("WiFi connected! IP address: ");
      Serial.println(WiFi.localIP());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      connected = false;
      break;
    default: break;
  }
}