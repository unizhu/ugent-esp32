/**
 * Hardware Test Sketch for ESP32-2432S028R
 * 
 * Tests TFT_eSPI colors and TFT_Touch WITHOUT LVGL.
 * If this shows B&W, the problem is TFT_eSPI / User_Setup.h.
 * If this shows COLOR, the problem is LVGL / lv_conf.h.
 * 
 * Expected: 4 colored quadrants (RED, GREEN, BLUE, YELLOW)
 * and touch coordinates printed on Serial (115200 baud).
 */

#include <TFT_eSPI.h>
#include <TFT_Touch.h>

// Touch pins
#define DOUT 39
#define DIN  32
#define DCS  33
#define DCLK 25

TFT_eSPI tft = TFT_eSPI(320, 240);
TFT_Touch touch = TFT_Touch(DCS, DCLK, DIN, DOUT);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== Hardware Test ===");
    
    // Init TFT
    tft.begin();
    tft.setRotation(1);  // Landscape
    
    // Fill BLACK first
    tft.fillScreen(TFT_BLACK);
    delay(500);
    
    // Draw 4 colored quadrants to test color rendering
    // Top-left: RED
    tft.fillRect(0, 0, 160, 120, TFT_RED);
    // Top-right: GREEN  
    tft.fillRect(160, 0, 160, 120, TFT_GREEN);
    // Bottom-left: BLUE
    tft.fillRect(0, 120, 160, 120, TFT_BLUE);
    // Bottom-right: YELLOW
    tft.fillRect(160, 120, 160, 120, TFT_YELLOW);
    
    // Draw labels
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("RED", 50, 50, 4);
    tft.setTextColor(TFT_WHITE, TFT_GREEN);
    tft.drawString("GREEN", 200, 50, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.drawString("BLUE", 50, 170, 4);
    tft.setTextColor(TFT_BLACK, TFT_YELLOW);
    tft.drawString("YELLOW", 200, 170, 4);
    
    // Init touch
    touch.setCal(526, 3443, 750, 3377, 320, 240, 1);
    
    Serial.println("Display test: 4 colored quadrants");
    Serial.println("Touch the screen - coordinates will print here");
    Serial.println("If colors are correct, TFT_eSPI + User_Setup.h are OK");
    Serial.println("If B&W, User_Setup.h is wrong or TFT driver is wrong");
}

void loop() {
    if (touch.Pressed()) {
        uint16_t x = touch.X();
        uint16_t y = touch.Y();
        Serial.printf("Touch: x=%d y=%d\n", x, y);
        
        // Draw a small circle where touched
        tft.fillCircle(x, y, 5, TFT_WHITE);
    }
    delay(50);
}
