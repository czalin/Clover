#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include "MifareUltralight.h"
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h>

using namespace ndef_mfrc522;

#define BTN1_PIN        4
#define LED_PIN         6
#define BTN2_PIN        7
#define BTN3_PIN        8
#define RST_PIN         5           // Configurable, see typical pin layout above
#define SS_PIN          10          // Configurable, see typical pin layout above

#define NUM_LEDS        4

typedef enum {
  AWAIT,
  PLAY,
  PAUSE,
  NEXT,
  PREV
} LED_STATE; 

Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

// Use pins 2 and 3 to communicate with DFPlayer Mini
static const uint8_t PIN_MP3_TX = 2; // Connects to module's RX
static const uint8_t PIN_MP3_RX = 3; // Connects to module's TX
SoftwareSerial softwareSerial(PIN_MP3_RX, PIN_MP3_TX);

// Create the Player object
DFRobotDFPlayerMini player;

void setup() {
  // Init USB serial port for debugging
  Serial.begin(9600);
  while (!Serial);      // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  
  // Init serial port for DFPlayer Mini
  softwareSerial.begin(9600);

  // Start communication with DFPlayer Mini
  do {
    Serial.println("Connecting to DFPlayer...");
  } while(!player.begin(softwareSerial));
  Serial.println("OK");

  player.volume(10);

  delay(10);
  
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card

  delay(10);

  randomSeed(analogRead(A5));
  
  leds.begin();
}

byte oldVolume = 10;
bool oldBtn1State = HIGH;
bool oldBtn2State = HIGH;
bool oldBtn3State = HIGH;
byte cardNum = 0;
byte trackNum = 0;
byte numTracks = 1;
byte cardType = 0;
byte color1[3] = {0xff, 0xff, 0xff};
byte color2[3] = {0xff, 0xff, 0xff};
int ticks = 0;
byte playlist[99];
LED_STATE led_state = LED_STATE::AWAIT;
int anim_start = 0;
void loop() {
  leds.clear();
  switch(led_state) {
    case LED_STATE::AWAIT:
      led_await();
      break;
    case LED_STATE::PLAY:
      led_play();
      break;
    case LED_STATE::PAUSE:
      led_pause();
      break;
    case LED_STATE::NEXT:
      led_next();
      break;
    case LED_STATE::PREV:
      led_prev();
      break; 
    default:
      break;
  }
  leds.show();
  ticks++;
  
  // read the input on analog pin 0:
  byte volume = (int)ceil(((1024.0 - analogRead(A0)) / 1024.0) * 20.0);
  if(volume != oldVolume) {
    player.volume(volume);
    oldVolume = volume;
  }
  delay(1);  // delay in between reads for stability

  bool btn1State = digitalRead(BTN1_PIN);
  if(btn1State != oldBtn1State) {
    if(btn1State == LOW) {
      // button pressed
      int playerState = player.readState();
      if(playerState != 0 && playerState != 512) {
        trackNum++;
        trackNum %= numTracks;
        player.playFolder(cardNum, cardType == 0 ? trackNum+1 : playlist[trackNum]);
        anim_start = ticks;
        led_state = LED_STATE::NEXT;
      }
    }
    oldBtn1State = btn1State;
  }

  bool btn2State = digitalRead(BTN2_PIN);
  if(btn2State != oldBtn2State) {
    if(btn2State == LOW) {
      // button pressed
      int playerState = player.readState();
      if(playerState == 1 || playerState == 513) {
        player.pause();
        led_state = LED_STATE::PAUSE;
      } else if(playerState != 0 && playerState != 512) {
        player.start();
        led_state = LED_STATE::PLAY;
      }
    }
    oldBtn2State = btn2State;
  }

  bool btn3State = digitalRead(BTN3_PIN);
  if(btn3State != oldBtn3State) {
    if(btn3State == LOW) {
      // button pressed
      int playerState = player.readState();
      if(playerState != 0 && playerState != 512) {
        trackNum = trackNum == 0 ? numTracks-1 : trackNum - 1;
        player.playFolder(cardNum, cardType == 0 ? trackNum+1 : playlist[trackNum]);
        anim_start = ticks;
        led_state = LED_STATE::PREV;
      }
    }
    oldBtn3State = btn3State;
  }

  // Go to next song when current song is done
  if(ticks % 50 == 0){
    if(player.available()) {
      if(player.readType() == DFPlayerPlayFinished) {
        trackNum++;
        trackNum %= numTracks;
        player.playFolder(cardNum, cardType == 0 ? trackNum+1 : playlist[trackNum]);
        anim_start = ticks;
        led_state = LED_STATE::NEXT;
      }
    }
  }

  byte statusBuf[34];
  if ( ! mfrc522.PICC_IsNewCardPresent())
    return;

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
    return;
    
  MifareUltralight reader = MifareUltralight(mfrc522);
  NfcTag tag = reader.read();
  //Uncomment Ndef.h#NDEF_USE_SERIAL
  //tag.print();
  byte payload[32];
  byte *pp = payload;
  tag.getNdefMessage().getRecord(0).getPayload(payload);

  cardNum = ((payload[3] - '0') * 10) + (payload[4] - '0');
  numTracks = ((payload[6] - '0') * 10) + (payload[7] - '0');
  cardType = ((payload[9] - '0') * 10) + (payload[10] - '0');
  
  sscanf(pp+12, "%2hhx", &color1[0]);
  sscanf(pp+14, "%2hhx", &color1[1]);
  sscanf(pp+16, "%2hhx", &color1[2]);
  sscanf(pp+19, "%2hhx", &color2[0]);
  sscanf(pp+21, "%2hhx", &color2[1]);
  sscanf(pp+23, "%2hhx", &color2[2]);

  Serial.print("CardNum: ");
  Serial.println(cardNum);
  Serial.println();
  Serial.println("Card color 1:");
  dump_byte_array(color1, 3);
  Serial.println();
  Serial.println("Card color 2:");
  dump_byte_array(color2, 3);
  Serial.println();
  if(cardType == 1) {
    buildShuffle();
  }
  trackNum = 0;
  player.playFolder(cardNum, cardType == 0 ? trackNum+1 : playlist[trackNum]);
  led_state = LED_STATE::PLAY;

  //mfrc522.PICC_DumpMifareUltralightToSerial();

  mfrc522.PICC_HaltA();
}

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}

void buildShuffle() {
  for(byte i = 0; i < numTracks; i++) {
    playlist[i] = i;
  }

  for(byte i = numTracks-1; i > 0; i--) {
    byte j = random(i);
    playlist[i] ^=  playlist[j];
    playlist[j] ^= playlist[i];
    playlist[i] ^= playlist[j];
  }
}

void led_await() {
    for(byte i = 0; i < NUM_LEDS; i++) {
      leds.setPixelColor(i, leds.Color(255,255,255));
    }
    leds.setBrightness(leds.sine8(ticks*3)/2);
}

void led_play() {
  leds.setBrightness(255);
  for(int i = 0; i < NUM_LEDS; i++) {
    float fadeLevel = (float)leds.sine8((ticks*2)-(i*64)) / 255.0;
    if(i % 2 == 0) {
      leds.setPixelColor(i, leds.Color((int)((float)color1[0])*fadeLevel, (int)((float)color1[1])*fadeLevel, (int)((float)color1[2])*fadeLevel));
    } else {
      leds.setPixelColor(i, leds.Color((int)((float)color2[0])*fadeLevel, (int)((float)color2[1])*fadeLevel, (int)((float)color2[2])*fadeLevel));
    }
  }
}

void led_pause() {
  leds.setBrightness(255);
  if(floor((float)(ticks % 32) / 16.0) == 0) {
    leds.setPixelColor(0, leds.Color(color1[0], color1[1], color1[2]));
  } else {
    leds.setPixelColor(2, leds.Color(color2[0], color2[1], color2[2]));
  }
}

void led_next() {
  byte interval = 5;
  if(ticks - anim_start < interval) {
    leds.setPixelColor(2, leds.Color(color1[0], color1[1], color1[2]));
  } else if(ticks - anim_start < interval * 2) {
    leds.setPixelColor(3, leds.Color(color1[0], color1[1], color1[2]));
  } else if(ticks - anim_start < interval * 3) {
    leds.setPixelColor(0, leds.Color(color1[0], color1[1], color1[2]));
  } else if(ticks - anim_start < interval * 5) {
    leds.setPixelColor(1, leds.Color(color1[0], color1[1], color1[2]));
  } else {
    led_state = LED_STATE::PLAY;
  }
}

void led_prev() {
  byte interval = 5;
  if(ticks - anim_start < interval) {
    leds.setPixelColor(3, leds.Color(color2[0], color2[1], color2[2]));
  } else if(ticks - anim_start < interval * 2) {
    leds.setPixelColor(2, leds.Color(color2[0], color2[1], color2[2]));
  } else if(ticks - anim_start < interval * 3) {
    leds.setPixelColor(1, leds.Color(color2[0], color2[1], color2[2]));
  } else if(ticks - anim_start < interval * 5) {
    leds.setPixelColor(0, leds.Color(color2[0], color2[1], color2[2]));
  } else {
    led_state = LED_STATE::PLAY;
  }
}
