#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <dht.h>

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

#define THINKSPEAK_KEY "Input here your key"

#define ARRAY_SIZE(array) (sizeof((array))/sizeof((array[0])))

enum TYPES {
  TEMPERATURE   = 0x01,
  HUMIDITY      = 0x02,
  CURRENT       = 0x04,
  LUMINOSIRTY   = 0x08,
};  

//DATA
typedef struct {
  // id of the device -> max is 32 characters
  char id[16];
  // type of the device -> a simple int
  int16_t type;
  // table of value -> 8 values max
  int16_t val[7];
} Payload;
Payload p;

//DHTSENSOR
dht DHT;
#define DHT11PINDATA 2
#define DHT11PINVCC  3
float temperature;
float humidity;

// NRF24L01
// Set up nRF24L01 radio on SPI pin for CE, CSN
RF24 radio(9,10);
// Example below using pipe5 for writing
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0x7365727631LL };

char receivePayload[32];

//count number of entering into main loop
uint8_t loopCounter=500;  

//WATCHDOG
volatile int f_wdt=1;

ISR(WDT_vect) {
  if(f_wdt == 0) {
    f_wdt=1;
  } else {
    //Serial.println("WDT Overrun!!!");
  }
}

void enterSleep(void) {
  radio.powerDown();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);   /* EDIT: could also use SLEEP_MODE_PWR_DOWN for lowest power consumption. */
  sleep_enable();
  
  /* Now enter sleep mode. */
  sleep_mode();
  
  /* The program will continue from here after the WDT timeout*/
  sleep_disable(); /* First thing to do is disable sleep. */
  
  /* Re-enable the peripherals. */
  power_all_enable();
}


void setup() {
  
  //Serial.begin(9600);
  
  //WATCHDOG
  /* Clear the reset flag. */
  MCUSR &= ~(1<<WDRF);
  /* In order to change WDE or the prescaler, we need to
   * set WDCE (This will allow updates for 4 clock cycles).
   */
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  /* set new watchdog timeout prescaler value */
  WDTCSR = 1<<WDP0 | 1<<WDP3; /* 8.0 seconds */
  /* Enable the WD interrupt (note no reset). */
  WDTCSR |= _BV(WDIE);
  
  //CONFIGURE DHT11
  pinMode(DHT11PINVCC, OUTPUT);
  //make it blink at startup
  digitalWrite(DHT11PINVCC, HIGH);
  delay(500);
  digitalWrite(DHT11PINVCC, LOW);
  
  //CONFIGURE RADIO
  radio.begin();
  // Enable this seems to work better
  radio.enableDynamicPayloads();
  radio.setAutoAck(1);
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(70);
  radio.setRetries(15,15);
  radio.setCRCLength(RF24_CRC_8);
  
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1,pipes[1]);
  
  //init a Payload
  strcpy(p.id, THINKSPEAK_KEY);
  p.type = TEMPERATURE | HUMIDITY;
  for (int i=0; i < 8; i++) { 
    p.val[i] = 0;
  }
  
  //Serial.println(p.type);
  
}

void loop() {
  if(f_wdt == 1) {
    loopCounter ++;
    if (loopCounter > 220) {
      // 220 *8 = 1760s = 29.2min @ 16mhtz
      readSensor();
      sendOverRadio();
      loopCounter = 0;
    }
    f_wdt = 0;
    enterSleep();
  } else {
    //nothing
  }
}

void sendOverRadio() {
  //power on radio
  radio.powerUp();
  
  //prepare 
  uint8_t data1 = 0;
  bool timeout=0;
  uint16_t nodeID = pipes[0] & 0xff;
  char outBuffer[32]="";
  unsigned long send_time, rtt = 0;
  
  // Stop listening and write to radio
  radio.stopListening();

  // Send to hub
  if ( radio.write(&p, sizeof(p)) ) {
    //Serial.println("Send successful\n\r");
    send_time = millis();
  } else {
    //Serial.println("Send failed\n\r");
  }
  
  //wait response
  radio.startListening();
  delay(20);
  while ( radio.available() && !timeout ) {
      uint8_t len = radio.getDynamicPayloadSize();
      radio.read( receivePayload, len);

      receivePayload[len] = 0;
      // Compare receive payload with outBuffer
      if ( ! strcmp(outBuffer, receivePayload) ) {
          rtt = millis() - send_time;
          //Serial.println("inBuffer --> rtt:");
          //Serial.println(rtt);
      }

      // Check for timeout and exit the while loop
      if ( millis() - send_time > radio.getMaxTimeout() ) {
          //Serial.println("Timeout!!!");
          timeout = 1;
      }

      delay(10);
  } // End while
}

void readSensor() {
  digitalWrite(DHT11PINVCC, HIGH);
  delay(200);
  int chk = DHT.read11(DHT11PINDATA);
  //Serial.print("Read sensor: ");
  switch (chk) {
    case DHTLIB_OK: 
      //Serial.println("OK"); 
      break;
    case DHTLIB_ERROR_CHECKSUM: 
      //Serial.println("Checksum error"); 
      break;
    case DHTLIB_ERROR_TIMEOUT: 
      //Serial.println("Time out error"); 
      break;
    default: 
      Serial.println("Unknown error"); 
      break;
  }
  p.val[0] = round(DHT.temperature)*100;
  p.val[1] = round(DHT.humidity)*100;
  
  digitalWrite(DHT11PINVCC, LOW);
}
