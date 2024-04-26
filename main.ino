#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>   
#include <TaskScheduler.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

#define OLED_MOSI  D7
#define OLED_CLK   D5
#define OLED_DC    D3
#define OLED_CS    D8
#define OLED_RESET D4

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);

const char* ssid = "Brad & Peggy's";
const char* password = "Bradcanfixit";

WiFiUDP udp;
const unsigned int localUdpPort = 4210;  // local port to listen on

uint8_t buffer1[1024];  // Buffer size must be enough to hold all screen data
uint8_t buffer2[1024];
uint8_t* frontBuffer = buffer1;
uint8_t* backBuffer = buffer2;
volatile bool bufferLocked = false;  // Simulated mutex

volatile bool updateReady = false;  // Flag to control the update execution

unsigned long lastMicro = 0;

Scheduler ts;
Task tDisplayUpdate(0, TASK_FOREVER, NULL, &ts, false);  // Start with task disabled

void switchBuffers() {
    noInterrupts();  // Disable interrupts to ensure atomic operation
    uint8_t* temp = frontBuffer;
    frontBuffer = backBuffer;
    backBuffer = temp;
    interrupts();  // Re-enable interrupts
}

void updateDisplay() {
    if (!updateReady) return;
    updateReady = false;
    switchBuffers();
    display.clearDisplay();
    display.drawBitmap(0, 0, frontBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
    display.display();

    unsigned long micro = micros();
    if (lastMicro != 0) {
        unsigned long elapsedMicro = micro - lastMicro;
        if (elapsedMicro > 0) {
            float fps = 1000000.0 / elapsedMicro;
            Serial.print(fps);
            Serial.println(" fps");
        }
    }
    lastMicro = micro;
    tDisplayUpdate.disable();  // Prevent re-execution unless explicitly enabled
}

void setup() {
    Serial.begin(115200);
    display.begin(SSD1306_SWITCHCAPVCC);
    SPI.setFrequency(8000000);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    udp.begin(localUdpPort);
    Serial.println("UDP listening on IP: " + WiFi.localIP().toString());

    tDisplayUpdate.setCallback(&updateDisplay);
}

void loop() {
    ts.execute();

    int packetSize = udp.parsePacket();
    if (packetSize) {
        while (bufferLocked);  // Simple spinlock
        bufferLocked = true;
        int len = udp.read((char*)backBuffer, sizeof(buffer1));
        bufferLocked = false;
        if (len > 0) {
            updateReady = true;
            tDisplayUpdate.enable();  // Trigger display update
        }
    }
}
