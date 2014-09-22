#include <cstdlib>
#include <iostream>
#include <curl/curl.h>
#include "./RF24/librf24-rpi/librf24/RF24.h"

using namespace std;

// Radio pipe addresses for the 2 nodes to communicate.
// First pipe is for writing, 2nd, 3rd, 4th, 5th & 6th is for reading...
// Pipe0 in bytes is "serv1" for mirf compatibility
const uint64_t pipes[6] = { 0x7365727631LL, 0xF0F0F0F0E1LL, 0xF0F0F0F0E2LL, 0xF0F0F0F0E3LL, 0xF0F0F0F0E4, 0xF0F0F0F0E5 };

// CE and CSN pins On header using GPIO numbering (not pin numbers)
RF24 radio("/dev/spidev0.0",8000000,25);  // Setup for GPIO 25 CSN


enum TYPES {
  TEMPERATURE   = 1,
  HUMIDITY      = 2,
  CURRENT       = 4,
  LUMINOSITY    = 8,
  PRESSURE      = 16,
};

//DATA
typedef struct {
  int16_t id;
  int16_t type;
  float val[10];
} Payload;
Payload p;


void setup(void)
{
        //
        // Refer to RF24.h or nRF24L01 DS for settings
        radio.begin();
        radio.enableDynamicPayloads();
        radio.setAutoAck(1);
        radio.setRetries(15,15);
        radio.setDataRate(RF24_250KBPS);
        radio.setPALevel(RF24_PA_MAX);
        radio.setChannel(70);
        radio.setCRCLength(RF24_CRC_8);

        // Open 6 pipes for readings ( 5 plus pipe0, also can be used for reading )
        radio.openWritingPipe(pipes[0]);
        radio.openReadingPipe(1,pipes[1]);
        radio.openReadingPipe(2,pipes[2]);
        radio.openReadingPipe(3,pipes[3]);
        radio.openReadingPipe(4,pipes[4]);
        radio.openReadingPipe(5,pipes[5]);

        // Start Listening
        radio.startListening();

        radio.printDetails();

        usleep(1000);
}

void sendToServer(char* str) 
{
  CURL *curl;
  CURLcode res;
 
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.thingspeak.com/update");
    /* example.com is redirected, so we tell libcurl to follow redirection */ 
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, str);
 
    /* Perform the request, res will get the return code */ 
    res = curl_easy_perform(curl);

    cout << res;

    /* Check for errors */ 
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
 
    /* always cleanup */ 
    curl_easy_cleanup(curl);
  }
}

void loop(char* key)
{
        char receivePayload[32];
        uint8_t pipe = 0;


         while ( radio.available( &pipe ) ) {

                uint8_t len = radio.getDynamicPayloadSize();
                radio.read( &p, sizeof(p) );

                char temp[5];

                char outBuffer[1024]="";
                strcat(outBuffer, "");
                strcat(outBuffer, "key=");
                strcat(outBuffer, key);
                strcat(outBuffer, "?id=");
                sprintf(temp, "%d", p.id);
                strcat(outBuffer, temp);
                int val = 0;
                if ((p.type & TEMPERATURE) == TEMPERATURE) { 
                        strcat(outBuffer,"&temperature=");
                        sprintf(temp, "%3.2f", p.val[val++]);
                        strcat(outBuffer, temp);
                }
                if ((p.type & HUMIDITY) == HUMIDITY) { 
                        strcat(outBuffer,"&humidity=");
                        sprintf(temp, "%3.2f", p.val[val++]);
                        strcat(outBuffer, temp);
                }
                if ((p.type & CURRENT) == CURRENT) { 
                        strcat(outBuffer,"&current=");
                        sprintf(temp, "%3.2f", p.val[val++]);
                        strcat(outBuffer, temp);
                }
                if ((p.type & LUMINOSITY) == LUMINOSITY) { 
                        strcat(outBuffer,"&luminosity=");
                        sprintf(temp, "%3.2f", p.val[val++]);
                        strcat(outBuffer, temp);
                }
                if ((p.type & PRESSURE) == PRESSURE) { 
                        strcat(outBuffer,"&pressure=");
                        sprintf(temp, "%3.2f", p.val[val++]);
                        strcat(outBuffer, temp);
                }

                // Display it on screen
                printf("Recv: size=%i pipe=%i type=%d address=%d data=%s",len,pipe,p.type,p.id,outBuffer);
                sendToServer(outBuffer);

                // Send back payload to sender
                radio.stopListening();


                // if pipe is 7, do not send it back
                if ( pipe != 7 ) {
                        // Send back using the same pipe
                        // radio.openWritingPipe(pipes[pipe]);
                        radio.write(receivePayload,len);

                        receivePayload[len]=0;
                        printf("\t Send: size=%i payload=%s pipe:%i\n\r",len,receivePayload,pipe);
                } else {
                        printf("\n\r");
                }

                // Enable start listening again
                radio.startListening();
        }
        //read on all pipes
        pipe++;
        if ( pipe > 5 ) pipe = 0;
        
        usleep(20);
}

int main(int argc, char** argv) 
{
        if (argc <= 1)
        {
                cout << "Usage: " << argv[0] << " <Serveur>" << endl;
                exit(1);
        }
        setup();
        while(1)
                loop(argv[1]);
        
        return 0;
}