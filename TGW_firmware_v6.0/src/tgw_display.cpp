#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "tgw_display.h"

static const int OLED_WIDTH = 128;
static const int OLED_HEIGHT = 64;
static const int OLED_RESET_PIN = -1;
// Pinos do HW-724 (ajuste aqui se seu hardware usar outros pinos).
static const int OLED_SDA_PIN = 5;
static const int OLED_SCL_PIN = 4;
static const uint8_t OLED_I2C_ADDR = 0x3C;

static Adafruit_SSD1306 s_display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
static bool s_display_ok = false;

void tgw_display_init() {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    s_display_ok = s_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
    if (!s_display_ok) {
        Serial.printf("[DISPLAY] SSD1306 init failed (addr=0x%02X, SDA=%d SCL=%d); disabling display output\n",
                      OLED_I2C_ADDR, OLED_SDA_PIN, OLED_SCL_PIN);
        return;
    }
    s_display.clearDisplay();
    s_display.setTextSize(1);
    s_display.setTextColor(SSD1306_WHITE);
    tgw_display_status("TGW boot", "init...");
}

void tgw_display_status(const char* line1, const char* line2) {
    if (!s_display_ok) return;
    s_display.clearDisplay();
    s_display.setCursor(0, 0);
    s_display.println(line1 ? line1 : "");
    if (line2) {
        s_display.println(line2);
    }
    s_display.display();
}

void tgw_display_summary(uint32_t hello_count,
                         uint32_t telem_count,
                         uint8_t node_id,
                         uint32_t cycle,
                         uint16_t soil_mean_raw,
                         uint16_t vbat_mean_raw,
                         int8_t rssi) {
    if (!s_display_ok) return;
    s_display.clearDisplay();
    s_display.setTextSize(1);
    s_display.setTextColor(SSD1306_WHITE);

    // Linha 0: titulo + contadores globais
    s_display.setCursor(0, 0);
    s_display.print("TGW H:");
    s_display.print(hello_count);
    s_display.print(" T:");
    s_display.print(telem_count);

    // Linha 1: node e ciclo
    s_display.setCursor(0, 10);
    s_display.print("Node ");
    s_display.print(node_id);
    s_display.print(" Cyc ");
    s_display.print(cycle);

    // Linha 2: soil/vbat
    s_display.setCursor(0, 20);
    s_display.print("Soil ");
    s_display.print(soil_mean_raw);
    s_display.setCursor(0, 30);
    s_display.print("Vbat ");
    s_display.print(vbat_mean_raw);

    // Linha 3: RSSI
    s_display.setCursor(0, 40);
    s_display.print("RSSI ");
    s_display.print((int)rssi);

    s_display.display();
}
