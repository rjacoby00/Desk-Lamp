// Ryan Jacoby
#include <SPI.h>
#include <WiFiNINA.h>
#include <Adafruit_NeoPixel.h>

#ifdef __AVR__
#include <avr/power.h>
#endif

#include "arduino_secrets.h"
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS; 

#define PIN 8
#define NUM_LEDS 30
#define BRIGHTNESS 50
#define SPIWIFI SPI
#define SPIWIFI_SS 10   
#define SPIWIFI_ACK 7
#define ESP32_RESETN 5   
#define ESP32_GPIO0 6

int led_status = 1;
int led_color_r = 50;
int led_color_g = 50;
int led_color_b = 50;
int led_color_w = 0xFF;
int led_color_r2 = 0;
int led_color_g2 = 0;
int led_color_b2 = 0;
int led_color_w2 = 0;

bool animating = false;
int animation_type = 1;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRBW + NEO_KHZ800);
WiFiServer server(80);   //Web server object. Will be listening in port 80 (default for HTTP)\

int status = WL_IDLE_STATUS;

byte neopix_gamma[] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
  10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
  17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
  25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
  37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
  51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
  69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
  90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
  115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
  144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
  177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
  215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
};

char device[20];
char color[20];
char color2[20];
char extra[20];
char action[20];

void setup() {
  Serial.begin(115200);

  attachInterrupt(digitalPinToInterrupt(2), actionHandler, RISING);

  WiFi.setPins(SPIWIFI_SS, SPIWIFI_ACK, ESP32_RESETN, ESP32_GPIO0, &SPIWIFI);
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("Communication with WiFi module failed!"));
    // don't continue
    while (true);
  }

  {
  String fv = WiFi.firmwareVersion();
  if (fv < "1.0.0") {
    Serial.println(F("Please upgrade the firmware"));
  }
  }

  // attempt to connect to Wifi network:
  Serial.print(F("Attempting to connect to SSID: "));
  Serial.println(ssid);
  // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
  do {
    status = WiFi.begin(ssid, pass);
    delay(100);     // wait until connection is ready!
  } while (status != WL_CONNECTED);
  
  Serial.println(F("Connected to wifi"));
  printWifiStatus();

  server.begin();

  // server.on("/action", actionHandler); //Associate the handler function to the path

  strip.setBrightness(BRIGHTNESS);
  strip.begin();
  // strip.show();
  // colorWipe(strip.Color(150, 0, 150, 5), 50);
  colorWipe(strip.Color(50, 50, 50, 255), 25);
  // dualColorWipe(strip.Color(255, 0, 0, 50), strip.Color(0, 255, 0, 50), 1, 1, 50);
}

void loop() {
  if (animating) {
    if (animation_type == 0) {
      colorWipe(strip.Color(led_color_r, led_color_g, led_color_b, led_color_w), 25);
      animating = false;
    } else if (animation_type == 1) {
      dualColorWipe(strip.Color(0, 255, 0, 50), strip.Color(255, 0, 0, 50), 1, 1, 0);
      delay(500);
      dualColorWipe(strip.Color(255, 0, 0, 50), strip.Color(0, 255, 0, 50), 1, 1, 0);
      delay(500);
      animating = true;
    } else if (animation_type == 2) {
      rainbowCycle(25);
      animating = true;
    } else if (animation_type == 3) {
      gradientWipe(strip.Color(led_color_r, led_color_g, led_color_b, led_color_w), strip.Color(led_color_r2, led_color_g2, led_color_b2), 25);
      animating = false;
    }
  }
}

void actionHandler() { //Handles and parses all incoming requests
  char message[50] = "Invalid/no device.  Usage: ?device={DEVICE}\0";
  char request[100] = "\0";
  int i = 0;

  WiFiClient client = server.available();
  
  if (client) {
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (i < 100)
          request[i] = c;
        i++;
        

        if (c == '\n' && currentLineIsBlank) {
          if (i < 100)
            request[i] = '\0';

          for (int i = 0; i < sizeof(request) - 4; i++) {
            if (request[i + 4] == ' ') {
              request[i + 4] = '\0';
              break;
            }
          }

          int argsOffset;
          for(argsOffset = 0; argsOffset < sizeof(request); argsOffset++) 
            if (request[argsOffset] == '?') break;

          Serial.println("\n\nRequest to parse:");
          Serial.println(request);
          Serial.println("End request dump");
          Serial.print("Get: ");
          Serial.println(checkGETRequest(request));
          Serial.print("Args offset: ");
          Serial.println(argsOffset);
          
          // PARSING CODE
          // For some reason they crash the Arduino when placed in fucntions

          // Device
          int deviceOffset, deviceSize = 0;
          for(deviceOffset = argsOffset; deviceOffset < sizeof(request) - 6; deviceOffset++) 
            if (request[deviceOffset] == 'd' && request[deviceOffset + 1] == 'e' && request[deviceOffset + 2] == 'v' && request[deviceOffset + 3] == 'i' && request[deviceOffset + 4] == 'c' && request[deviceOffset + 5] == 'e' && request[deviceOffset + 6] == '=') break;
          deviceOffset += 7;

          if (deviceOffset <= sizeof(request)) {
            for (deviceSize; deviceSize + deviceOffset < sizeof(request); deviceSize++) {
              if (request[deviceOffset + deviceSize] == '&') break;
            }
          }

          for (int i = 0; i < sizeof(device); i++) 
            if(i < deviceSize) device[i] = request[deviceOffset + i];
          if(deviceSize < sizeof(device)) device[deviceSize] = '\0';
          else device[sizeof(device) - 1] = '\0';

          // Color
          int colorOffset, colorSize = 0;
          for(colorOffset = argsOffset; colorOffset < sizeof(request) - 5; colorOffset++) 
            if (request[colorOffset] == 'c' && request[colorOffset + 1] == 'o' && request[colorOffset + 2] == 'l' && request[colorOffset + 3] == 'o' && request[colorOffset + 4] == 'r' && request[colorOffset + 5] == '=') break;
          colorOffset += 6;

          if (colorOffset <= sizeof(request)) {
            for (colorSize; colorSize + colorOffset < sizeof(request); colorSize++) {
              if (request[colorOffset + colorSize] == '&') break;
            }
          }
          
          for (int i = 0; i < sizeof(color); i++) 
            if(i < colorSize) color[i] = request[colorOffset + i];
          if(colorSize < sizeof(color)) color[colorSize] = '\0';
          else color[sizeof(color) - 1] = '\0';

          // Color2
          int color2Offset, color2Size = 0;
          for(color2Offset = argsOffset; color2Offset < sizeof(request) - 5; color2Offset++) 
            if (request[color2Offset] == 'c' && request[color2Offset + 1] == 'o' && request[color2Offset + 2] == 'l' && request[color2Offset + 3] == 'o' && request[color2Offset + 4] == 'r' && request[color2Offset + 5] == '2' && request[color2Offset + 6] == '=') break;
          color2Offset += 7;

          if (color2Offset <= sizeof(request)) {
            for (color2Size; color2Size + color2Offset < sizeof(request); color2Size++) {
              if (request[color2Offset + color2Size] == '&') break;
            }
          }
          
          for (int i = 0; i < sizeof(color2); i++) 
            if(i < color2Size) color2[i] = request[color2Offset + i];
          if(color2Size < sizeof(color2)) color2[color2Size] = '\0';
          else color2[sizeof(color2) - 1] = '\0';

          // Extra
          int extraOffset, extraSize = 0;
          for(extraOffset = argsOffset; extraOffset < sizeof(request) - 5; extraOffset++) 
            if (request[extraOffset] == 'e' && request[extraOffset + 1] == 'x' && request[extraOffset + 2] == 't' && request[extraOffset + 3] == 'r' && request[extraOffset + 4] == 'a' && request[extraOffset + 5] == '=') break;
          extraOffset += 6;

          if (extraOffset <= sizeof(request)) {
            for (extraSize; extraSize + extraOffset < sizeof(request); extraSize++) {
              if (request[extraOffset + extraSize] == '&') break;
            }
          }
          
          for (int i = 0; i < sizeof(extra); i++) 
            if(i < extraSize) extra[i] = request[extraOffset + i];
          if(extraSize < sizeof(extra)) extra[extraSize] = '\0';
          else extra[sizeof(extra) - 1] = '\0';

          // Action
          int actionOffset, actionSize = 0;
          for(actionOffset = argsOffset; actionOffset < sizeof(request) - 6; actionOffset++) 
            if (request[actionOffset] == 'a' && request[actionOffset + 1] == 'c' && request[actionOffset + 2] == 't' && request[actionOffset + 3] == 'i' && request[actionOffset + 4] == 'o' && request[actionOffset + 5] == 'n' && request[actionOffset + 6] == '=') break;
          actionOffset += 7;

          if (actionOffset <= sizeof(request)) {
            for (actionSize; actionSize + actionOffset < sizeof(request); actionSize++) {
              if (request[actionOffset + actionSize] == '&') break;
            }
          }
          
          for (int i = 0; i < sizeof(action); i++) 
            if(i < actionSize) action[i] = request[actionOffset + i];
          if(actionSize < sizeof(action)) action[actionSize] = '\0';
          else action[sizeof(action) - 1] = '\0';

          /*  Parsing debug code; not needed for production
          Serial.print("Color 1: ");
          Serial.println(color);
          Serial.print("Color 2: ");
          Serial.println(color2);
          Serial.print("Extra: ");
          Serial.println(extra);
          Serial.print("Action: ");
          Serial.println(action);
          Serial.print("Device: ");
          Serial.println(device);
          Serial.print("Mem free: ");
          Serial.println(freeRam());
          */

          // END PARSING CODE

          if (strcmp(device, "neopixels") == 0) {
            if (strcmp(action, "on") == 0) {
              Serial.println("ACTION IS ON");
              if (led_color_w == 0 && led_color_r == 0 && led_color_g == 0 && led_color_b == 0) {
                led_color_r = 50;
                led_color_g = 50;
                led_color_b = 50;
                led_color_w = 255;
              }
              animation_type = 0;
              animating = true;
              led_status = 1;

              strncpy(message, "on", 2);
              message[2] = '\0';
            } else if (strcmp(action, "off") == 0) {
              Serial.println("ACTION IS OFF");
              led_color_w = 0;
              led_color_r = 0;
              led_color_g = 0;
              led_color_b = 0;
              animation_type = 0;
              animating = true;
              led_status = 0;

              strncpy(message, "off", 3);
              message[3] = '\0';
            } else if (strcmp(action, "custom_color") == 0) {
              int i;
              for(i = 0; i < sizeof(color); i++) 
                if (color[i] == '\0') break;
              
              if (i == 7) {
                led_color_r = hexchartoint(color[1], color[2]);
                led_color_g = hexchartoint(color[3], color[4]);
                led_color_b = hexchartoint(color[5], color[6]);
                led_color_w = 0;
              } else if (i == 6) {
                led_color_r = hexchartoint(color[0], color[1]);
                led_color_g = hexchartoint(color[2], color[3]);
                led_color_b = hexchartoint(color[4], color[5]);
                led_color_w = 0;
              } else if (i == 4) {
                led_color_r = 16 * hexchartoint('0', color[1]);
                led_color_g = 16 * hexchartoint('0', color[2]);
                led_color_b = 16 * hexchartoint('0', color[3]);
                led_color_w = 0;
              } else if (i == 3) {
                led_color_r = 16 * hexchartoint('0', color[0]);
                led_color_g = 16 * hexchartoint('0', color[1]);
                led_color_b = 16 * hexchartoint('0', color[2]);
                led_color_w = 0;
              } else if (i == 9) {
                led_color_r = hexchartoint(color[1], color[2]);
                led_color_g = hexchartoint(color[3], color[4]);
                led_color_b = hexchartoint(color[5], color[6]);
                led_color_w = hexchartoint(color[7], color[8]);
              } else if (i == 8) {
                led_color_r = hexchartoint(color[0], color[1]);
                led_color_g = hexchartoint(color[2], color[3]);
                led_color_b = hexchartoint(color[4], color[5]);
                led_color_w = hexchartoint(color[6], color[7]);
              }
              
              animation_type = 0;
              animating = true;
              led_status = 1;
              
              strncpy(message, "set to a custom color", 21);
              message[21] = '\0';
            } else if (strcmp(action, "status") == 0) {
              message[0] = 48 + led_status;
              message[1] = '\0';
            } else if (strcmp(action, "status_color") == 0) {
              strncpy(message, "#afafaf", 7);
              message[7] = '\0';
            } else if (strcmp(action, "animate") == 0) {
              if (strcmp(extra, "0") == 0) {
                animation_type = 2;
                animating = true;
                led_status = 1;

                strncpy(message, "Rainbow", 7);
                message[7] = '\0';
              } else if (strcmp(extra, "1") == 0) {
                int i;
                for(i = 0; i < sizeof(color); i++) 
                  if (color[i] == '\0') break;
                
                if (i == 7) {
                  led_color_r = hexchartoint(color[1], color[2]);
                  led_color_g = hexchartoint(color[3], color[4]);
                  led_color_b = hexchartoint(color[5], color[6]);
                  led_color_w = 0;
                } else if (i == 6) {
                  led_color_r = hexchartoint(color[0], color[1]);
                  led_color_g = hexchartoint(color[2], color[3]);
                  led_color_b = hexchartoint(color[4], color[5]);
                  led_color_w = 0;
                } else if (i == 4) {
                  led_color_r = 16 * hexchartoint('0', color[1]);
                  led_color_g = 16 * hexchartoint('0', color[2]);
                  led_color_b = 16 * hexchartoint('0', color[3]);
                  led_color_w = 0;
                } else if (i == 3) {
                  led_color_r = 16 * hexchartoint('0', color[0]);
                  led_color_g = 16 * hexchartoint('0', color[1]);
                  led_color_b = 16 * hexchartoint('0', color[2]);
                  led_color_w = 0;
                } else if (i == 9) {
                  led_color_r = hexchartoint(color[1], color[2]);
                  led_color_g = hexchartoint(color[3], color[4]);
                  led_color_b = hexchartoint(color[5], color[6]);
                  led_color_w = hexchartoint(color[7], color[8]);
                } else if (i == 8) {
                  led_color_r = hexchartoint(color[0], color[1]);
                  led_color_g = hexchartoint(color[2], color[3]);
                  led_color_b = hexchartoint(color[4], color[5]);
                  led_color_w = hexchartoint(color[6], color[7]);
                }

                int j;
                for(j = 0; j < sizeof(color); j++) 
                  if (color[j] == '\0') break;
                
                if (j == 7) {
                  led_color_r2 = hexchartoint(color2[1], color2[2]);
                  led_color_g2 = hexchartoint(color2[3], color2[4]);
                  led_color_b2 = hexchartoint(color2[5], color2[6]);
                  led_color_w2 = 0;
                } else if (j == 6) {
                  led_color_r2 = hexchartoint(color2[0], color2[1]);
                  led_color_g2 = hexchartoint(color2[2], color2[3]);
                  led_color_b2 = hexchartoint(color2[4], color2[5]);
                  led_color_w2 = 0;
                } else if (j == 4) {
                  led_color_r2 = 16 * hexchartoint('0', color2[1]);
                  led_color_g2 = 16 * hexchartoint('0', color2[2]);
                  led_color_b2 = 16 * hexchartoint('0', color2[3]);
                  led_color_w2 = 0;
                } else if (j == 3) {
                  led_color_r2 = 16 * hexchartoint('0', color2[0]);
                  led_color_g2 = 16 * hexchartoint('0', color2[1]);
                  led_color_b2 = 16 * hexchartoint('0', color2[2]);
                  led_color_w2 = 0;
                } else if (j == 9) {
                  led_color_r2 = hexchartoint(color2[1], color2[2]);
                  led_color_g2 = hexchartoint(color2[3], color2[4]);
                  led_color_b2 = hexchartoint(color2[5], color2[6]);
                  led_color_w2 = hexchartoint(color2[7], color2[8]);
                } else if (j == 8) {
                  led_color_r2 = hexchartoint(color2[0], color2[1]);
                  led_color_g2 = hexchartoint(color2[2], color2[3]);
                  led_color_b2 = hexchartoint(color2[4], color2[5]);
                  led_color_w2 = hexchartoint(color2[6], color2[7]);
                }

                animation_type = 3;
                animating = true;
                led_status = 1;

                strncpy(message, "Gradient", 8);
                message[8] = '\0';
              }
            } else {
              strncpy(message, "NeoPixels selected, no action", 29);
              message[29] = '\0';
            }
          } else {
            strncpy(message, "Invalid/no device.  Usage: ?device={DEVICE}", 44);
            message[44] = '\0';
          }
          

          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/plain");
          client.println("Connection: close");
          client.println();
          client.println(message);          

          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }

    delay(1);

    client.stop();
  }
}

// Functions for animations

void colorWipe(uint32_t c, uint8_t wait) { // Adafruit
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void dualColorWipe(uint32_t c1, uint32_t c2, uint8_t s1, uint8_t s2, uint8_t wait) { //RJ
  bool col1 = false;
  uint16_t goal = 0;
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    if(i >= goal) {
      if(col1) goal += s2;
      else goal += s1;

      col1 = !col1;
    }
    
    if(col1) strip.setPixelColor(i, c1);
    else strip.setPixelColor(i, c2);
    strip.show();
    delay(wait);
  }
}

void gradientWipe(uint32_t c1, uint32_t c2, uint8_t wait) { //RJ
  double cr = red(c1);
  double cg = green(c1);
  double cb = blue(c1);
  double cw = white(c1);
  double dr = (red(c1) - red(c2)) / (double)strip.numPixels();
  double dg = (green(c1) - green(c2)) / (double)strip.numPixels();
  double db = (blue(c1) - blue(c2)) / (double)strip.numPixels();
  double dw = (white(c1) - white(c2)) / (double)strip.numPixels();

  for(uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color((int)cr, (int)cg, (int)cb, (int)cw));
    strip.show();

    cr -= dr;
    cg -= dg;
    cb -= db;
    cw -= dw;

    delay(wait);
  }
}

void pulseWhite(uint8_t wait) { //Adafruit
  for (int j = 0; j < 256 ; j++) {
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0, neopix_gamma[j] ) );
    }
    delay(wait);
    strip.show();
  }

  for (int j = 255; j >= 0 ; j--) {
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0, neopix_gamma[j] ) );
    }
    delay(wait);
    strip.show();
  }
}


void rainbowFade2White(uint8_t wait, int rainbowLoops, int whiteLoops) { //Adafruit
  float fadeMax = 100.0;
  int fadeVal = 0;
  uint32_t wheelVal;
  int redVal, greenVal, blueVal;

  for (int k = 0 ; k < rainbowLoops ; k ++) {

    for (int j = 0; j < 256; j++) { // 5 cycles of all colors on wheel

      for (int i = 0; i < strip.numPixels(); i++) {

        wheelVal = Wheel(((i * 256 / strip.numPixels()) + j) & 255);

        redVal = red(wheelVal) * float(fadeVal / fadeMax);
        greenVal = green(wheelVal) * float(fadeVal / fadeMax);
        blueVal = blue(wheelVal) * float(fadeVal / fadeMax);

        strip.setPixelColor( i, strip.Color( redVal, greenVal, blueVal ) );

      }

      //First loop, fade in!
      if (k == 0 && fadeVal < fadeMax - 1) {
        fadeVal++;
      }

      //Last loop, fade out!
      else if (k == rainbowLoops - 1 && j > 255 - fadeMax ) {
        fadeVal--;
      }

      strip.show();
      delay(wait);
    }

  }



  delay(500);


  for (int k = 0 ; k < whiteLoops ; k ++) {

    for (int j = 0; j < 256 ; j++) {

      for (uint16_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0, neopix_gamma[j] ) );
      }
      strip.show();
    }

    delay(2000);
    for (int j = 255; j >= 0 ; j--) {

      for (uint16_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0, neopix_gamma[j] ) );
      }
      strip.show();
    }
  }

  delay(500);


}

void whiteOverRainbow(uint8_t wait, uint8_t whiteSpeed, uint8_t whiteLength ) { //Adafruit

  if (whiteLength >= strip.numPixels()) whiteLength = strip.numPixels() - 1;

  int head = whiteLength - 1;
  int tail = 0;

  int loops = 3;
  int loopNum = 0;

  static unsigned long lastTime = 0;


  while (true) {
    for (int j = 0; j < 256; j++) {
      for (uint16_t i = 0; i < strip.numPixels(); i++) {
        if ((i >= tail && i <= head) || (tail > head && i >= tail) || (tail > head && i <= head) ) {
          strip.setPixelColor(i, strip.Color(0, 0, 0, 255 ) );
        }
        else {
          strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
        }

      }

      if (millis() - lastTime > whiteSpeed) {
        head++;
        tail++;
        if (head == strip.numPixels()) {
          loopNum++;
        }
        lastTime = millis();
      }

      if (loopNum == loops) return;

      head %= strip.numPixels();
      tail %= strip.numPixels();
      strip.show();
      delay(wait);
    }
  }

}

void fullWhite() { //Adafruit

  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0, 255 ) );
  }
  strip.show();
}


// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) { //Adafruit
  uint16_t i, j;

  for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

void rainbow(uint8_t wait) { //Adafruit
  uint16_t i, j;

  for (j = 0; j < 256; j++) {
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) { //Adafruit
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3, 0);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3, 0);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0, 0);
}

uint8_t white(uint32_t c) {
  return (c >> 24);
}
uint8_t red(uint32_t c) {
  return (c >> 16);
}
uint8_t green(uint32_t c) {
  return (c >> 8);
}
uint8_t blue(uint32_t c) {
  return (c);
}

uint8_t hexchartoint(char a, char b) {
  uint8_t A, B;
  
  if(!((a <= 57 && a >= 48) || (a <= 70 && a >= 65)) || !((b <= 57 && b >= 48) || (b <= 70 && b >= 65))) return 0;
  

  if(a <= 57 && a >= 48) A = a - 48;
  else A = a - 55;

  if(b <= 57 && b >= 48) B = b - 48;
  else B = b - 55;

  return B + (16 * A);
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

boolean checkGETRequest(char a[]) {
  int i;
  for (i = 0; i < sizeof(a); i++) {
    if (a[i] == 'G') break;
  }
  return a[i + 1] == 'E' && a[i + 2] == 'T';
}

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
