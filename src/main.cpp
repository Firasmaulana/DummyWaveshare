#include <Arduino.h>
#include <ModbusMaster.h>
#include <lvgl.h>

// --- LIBRARY DRIVER LAYAR WAVESHARE ---
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "TCA9554.h"
#include "esp_lcd_touch_axs15231b.h"

// --- DEFINISI PIN LCD & I2C ---
#define GFX_BL        6
#define LCD_QSPI_CS   12
#define LCD_QSPI_CLK  5
#define LCD_QSPI_D0   1
#define LCD_QSPI_D1   2
#define LCD_QSPI_D2   3
#define LCD_QSPI_D3   4
#define I2C_SDA       8
#define I2C_SCL       7

// --- DEFINISI PIN MODBUS ---
#define RS485_RX 43
#define RS485_TX 44

// --- INISIALISASI OBJEK HARDWARE ---
TCA9554 TCA(0x20);
Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_QSPI_CS, LCD_QSPI_CLK, LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3);
Arduino_GFX *gfx = new Arduino_AXS15231B(bus, -1 /* RST */, 0 /* rotation */, false, 320, 480);

HardwareSerial RS485Serial(1); 
ModbusMaster node;

// --- VARIABEL LVGL V8 & MODBUS ---
uint32_t screenWidth;
uint32_t screenHeight;
lv_disp_draw_buf_t draw_buf; // V8 menggunakan ini
lv_color_t *disp_draw_buf1;
lv_color_t *disp_draw_buf2;

unsigned long lastModbusPoll = 0;
const unsigned long POLLING_INTERVAL = 2000;

// Objek UI
lv_obj_t * ui_LabelGas;
lv_obj_t * ui_LabelSuhu;
lv_obj_t * ui_LabelKelembaban;
lv_obj_t * ui_LabelStatus;

// --- CALLBACK LVGL V8 ---
// Flush callback (Render layar)
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
  lv_disp_flush_ready(disp_drv);
}

// Read callback (Touchscreen)
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  touch_data_t touch_data;
  bsp_touch_read();
  if (bsp_touch_get_coordinates(&touch_data)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touch_data.coords[0].x;
    data->point.y = touch_data.coords[0].y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// --- FUNGSI PEMBUAT UI ---
void build_ui() {
  lv_obj_t * scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A1A), LV_PART_MAIN);

  lv_obj_t * panel = lv_obj_create(scr);
  lv_obj_set_size(panel, 280, 300);
  lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t * title = lv_label_create(panel);
  lv_label_set_text(title, "AQMS MONITORING");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_text_color(title, lv_color_hex(0x00A8FF), LV_PART_MAIN);

  ui_LabelGas = lv_label_create(panel);
  lv_label_set_text(ui_LabelGas, "Gas : Menunggu...");
  lv_obj_align(ui_LabelGas, LV_ALIGN_TOP_LEFT, 20, 60);

  ui_LabelSuhu = lv_label_create(panel);
  lv_label_set_text(ui_LabelSuhu, "Suhu: Menunggu...");
  lv_obj_align(ui_LabelSuhu, LV_ALIGN_TOP_LEFT, 20, 100);

  ui_LabelKelembaban = lv_label_create(panel);
  lv_label_set_text(ui_LabelKelembaban, "RH  : Menunggu...");
  lv_obj_align(ui_LabelKelembaban, LV_ALIGN_TOP_LEFT, 20, 140);

  ui_LabelStatus = lv_label_create(panel);
  lv_label_set_text(ui_LabelStatus, "Stat: Menghubungkan...");
  lv_obj_align(ui_LabelStatus, LV_ALIGN_TOP_LEFT, 20, 180);
}

void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);
  TCA.begin();
  TCA.pinMode1(1, OUTPUT);
  TCA.write1(1, 1); delay(10);
  TCA.write1(1, 0); delay(10);
  TCA.write1(1, 1); delay(200);

  bsp_touch_init(&Wire, -1, 0, 320, 480);

  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_BLACK);
  
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  // --- INISIALISASI LVGL V8 ---
  lv_init();

  screenWidth = gfx->width();
  screenHeight = gfx->height();
  
  // Alokasi Buffer untuk V8 (Lebih hemat memori)
  uint32_t bufSize = screenWidth * 40; // Render 40 baris per frame
  disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(bufSize * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  
  if (!disp_draw_buf1) {
    Serial.println("Alokasi memori LVGL gagal!");
  } else {
    // 1. Inisialisasi Draw Buffer
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf1, NULL, bufSize);

    // 2. Inisialisasi Display Driver V8
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // 3. Inisialisasi Input Device V8
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
  }

  build_ui();

  RS485Serial.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
  node.begin(1, RS485Serial); 
}

void loop() {
  lv_timer_handler(); // V8 menggunakan lv_timer_handler, BUKAN lv_task_handler
  delay(5); 
  
  unsigned long currentMillis = millis();
  if (currentMillis - lastModbusPoll >= POLLING_INTERVAL) {
    lastModbusPoll = currentMillis; 
    
    uint8_t result = node.readHoldingRegisters(0x0000, 4);

    if (result == node.ku8MBSuccess) {
      float gasConcentration = node.getResponseBuffer(0) / 10.0;
      float temperature      = node.getResponseBuffer(1) / 10.0;
      float humidity         = node.getResponseBuffer(2) / 10.0;
      uint16_t rawStatus     = node.getResponseBuffer(3);

      if (ui_LabelGas != NULL) {
        lv_label_set_text_fmt(ui_LabelGas, "Gas : %.1f ppm", gasConcentration);
        lv_label_set_text_fmt(ui_LabelSuhu, "Suhu: %.1f °C", temperature);
        lv_label_set_text_fmt(ui_LabelKelembaban, "RH  : %.1f %%", humidity);
        
        if (rawStatus == 0) {
            lv_label_set_text(ui_LabelStatus, "Stat: OK (Online)");
            lv_obj_set_style_text_color(ui_LabelStatus, lv_color_hex(0x00FF00), LV_PART_MAIN);
        } else {
            lv_label_set_text(ui_LabelStatus, "Stat: WARNING");
            lv_obj_set_style_text_color(ui_LabelStatus, lv_color_hex(0xFFA500), LV_PART_MAIN);
        }
      }
    } else {
      if (ui_LabelStatus != NULL) {
         lv_label_set_text(ui_LabelStatus, "Stat: DISCONNECTED");
         lv_obj_set_style_text_color(ui_LabelStatus, lv_color_hex(0xFF0000), LV_PART_MAIN);
      }
    }
  }
}