#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <Adafruit_WINC1500.h>
#include <Adafruit_WINC1500MDNS.h>
#include "Timer.h"

#define OLED_RESET 5
#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B01000000,
  B00000001, B01000000,
  B00000001, B01000000,
  B00000010, B00100000,
  B11110010, B00100000,
  B11000000, B00001000,
  B01000000, B00000001,
  B00110000, B10000001,
  B00010000, B00000100,
  B00001000, B00010000,
  B00010000, B10100000,
  B00100000, B00100000,
  B00100000, B00010000,
  B01000000, B00010000,
  B01000000, B00010000,
  B00000000, B00010000
};

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif
Adafruit_SSD1306 display(OLED_RESET);

#define NEO_PIN 10
#define NUM_PIXELS 60
#define MIC_PIN A1
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800); // NUM_PIXELS minutes of data

// NUM_PIXELS sized array of colors
int r[NUM_PIXELS];
int g[NUM_PIXELS];
int b[NUM_PIXELS];
const int sampleWindow = 50; // Sample window width in mS (50 mS = 20Hz)

// Define the MDNS name
#define MDNS_NAME "frank"

// If you're following the Adafruit WINC1500 board
// guide you don't need to modify these:
#define WINC_CS   8
#define WINC_IRQ  7
#define WINC_RST  4
#define WINC_EN   2     // or, tie EN to VCC and comment this out

// Setup the WINC1500 connection with the pins above and the default hardware SPI.
Adafruit_WINC1500 WiFi(WINC_CS, WINC_IRQ, WINC_RST);

char ssid[] = "hi-hi-hi";      // your network SSID (name)
char pass[] = "it's a good password";   // your network password
int keyIndex = 0;                 // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;

// Create a MDNS responder to listen and respond to MDNS name requests.
MDNSResponder mdns(&WiFi);

Adafruit_WINC1500Server server(80);

/*
     NUM_PIXELS minutes worth of max sound levels
     NUM_PIXELS minutes worth of min sound levels
     current minute's max sound level
     current minute's min sound level
*/

int pastMax[NUM_PIXELS];
int pastMin[NUM_PIXELS];
int levels[NUM_PIXELS];
int currentMax = 512;
int currentMin = 512;

Timer *timer1 = new Timer(5000);

void setup() {
  timer1->setOnTimer(&everyMinute);
  pinMode(MIC_PIN, INPUT);

#ifdef WINC_EN
  pinMode(WINC_EN, OUTPUT);
  digitalWrite(WINC_EN, HIGH);
#endif

  //Initialize serial and wait for port to open:
//  Serial.begin(9600);
//  while (!Serial) {
//    ; // wait for serial port to connect. Needed for native USB port only
//  }
//  // Serial.println("begun Serial, starting pixels now");

  // neopixel init
  strip.begin();
  strip.show();
  for (int i = 0; i < NUM_PIXELS; i++) {
    r[i] = 0;
    g[i] = 0;
    b[i] = 0;
    pastMax[i] = 0;
    pastMin[i] = 0;
    levels[i] = 0;
  }

  // Some example procedures showing how to display to the pixels:
  colorWipe(strip.Color(0, 255, 0), 50); // Green
  colorWipe(strip.Color(0, 0, 255), 50); // Blue
  colorWipe(strip.Color(255, 0, 0), 50); // Red

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.display();
  display.clearDisplay();
  // // Serial.println("cleared display buffer");

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    // Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  // attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) {
    // Serial.print("Attempting to connect to SSID: ");
    // Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  // connected
//  printWifiStatus();

  server.begin();
  // Setup the MDNS responder to listen to the configured name.
  // NOTE: You _must_ call this _after_ connecting to the WiFi network and
  // being assigned an IP address.
  if (!mdns.begin(MDNS_NAME)) {
    // Serial.println("Failed to start MDNS responder!");
    while (1);
  }
  // Serial.print("Server listening at http://");
  // Serial.print(MDNS_NAME);
  // Serial.println(".local/");

  // finally, start the interval timer
  timer1->Start();

}


void loop() {
  timer1->Update();
  // Call the update() function on the MDNS responder every loop iteration to
  // make sure it can detect and respond to name requests.
  mdns.update();
  listen(); // sample sound levels more or less continuously
  doOLED(); // update screen with histogram
  doPixels(); // update neopixels


  // listen for incoming clients
  Adafruit_WINC1500Client client = server.available();
  if (client) {
    // Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
//        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output the value of each analog input pin
          // HERE insert mic sensor readings
          for (int timebin = 0; timebin < NUM_PIXELS; timebin++) {
            client.print("<div >At t= ");
            client.print(timebin);
            client.print(" the range was from ");
            client.print(pastMin[timebin] + " to " + pastMax[timebin]);
            client.println("</div><br />");
          }
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
    // Serial.println("client disconnected");
  }
} // end of loop


void printWifiStatus() {
  // print the SSID of the network you're attached to:
  // Serial.print("SSID: ");
  // Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  // Serial.print("IP Address: ");
  // Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  // Serial.print("signal strength (RSSI):");
  // Serial.print(rssi);
  // Serial.println(" dBm");
}
