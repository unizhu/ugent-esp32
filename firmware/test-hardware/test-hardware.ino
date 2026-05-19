/**
 * Hardware Test Sketch for ESP32-2432S028R (Cheap Yellow Display)
 * 
 * Tests TFT_eSPI colors and XPT2046 touch WITHOUT LVGL.
 * Based on witnessmenow/ESP32-Cheap-Yellow-Display proven configuration:
 *   - Display on HSPI (SPI2) via TFT_eSPI
 *   - Touch on separate VSPI (SPI3) via XPT2046_Touchscreen library
 * 
 * Expected: 4 colored quadrants (RED, GREEN, BLUE, YELLOW)
 * Touch coordinates printed on Serial (115200 baud).
 * 
 * REQUIRED LIBRARIES:
 *   - TFT_eSPI (Library Manager)
 *   - XPT2046_Touchscreen by Paul Stoffregen (Library Manager, search "XPT2046")
 */

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// ─── Touch pins (separate VSPI bus) ───────────────────────────────────────────
//  The CYD touch controller (XPT2046) is wired to non-default SPI pins
//  on a SEPARATE bus from the display.
#define XPT2046_CLK  25
#define XPT2046_MISO 39
#define XPT2046_MOSI 32
#define XPT2046_CS   33
#define XPT2046_IRQ  36

// Create a separate VSPI instance for touch
SPIClass touchSpi = SPIClass(VSPI);

// Touch driver using the separate SPI bus
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

// Display
TFT_eSPI tft = TFT_eSPI(320, 240);  // Landscape dimensions

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== Hardware Test (witnessmenow config) ===");
    
    // ─── Init Touch (separate VSPI bus) ───────────────────────────────────────
    touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSpi);
    ts.setRotation(1);  // Landscape
    
    // ─── Init TFT (HSPI bus via TFT_eSPI) ─────────────────────────────────────
    tft.begin();
    tft.setRotation(1);  // Landscape 320x240
    
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
    
    Serial.println("Display test: 4 colored quadrants");
    Serial.println("Touch the screen - coordinates will print here");
    Serial.println("If colors are correct, TFT_eSPI + User_Setup.h are OK");
    Serial.println("If B&W or wrong colors, check User_Setup.h");
}

void loop() {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        Serial.printf("Touch: raw x=%d y=%d z=%d\n", p.x, p.y, p.z);
        
        // Map raw touch coordinates to screen coordinates (landscape)
        int16_t screenX = map(p.x, 200, 3700, 0, 320);
        int16_t screenY = map(p.y, 240, 3800, 0, 240);
        
        // Clamp to screen bounds
        screenX = constrain(screenX, 0, 319);
        screenY = constrain(screenY, 0, 239);
        
        Serial.printf("  mapped: x=%d y=%d\n", screenX, screenY);
        
        // Draw a small circle where touched
        tft.fillCircle(screenX, screenY, 5, TFT_WHITE);
    }
    delay(50);
}
