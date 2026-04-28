#include <Arduino.h>
#include <ModbusMaster.h>
#include <lvgl.h>

// --- LIBRARY DRIVER LAYAR WAVESHARE ---
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "TCA9554.h"
// Catatan: Pastikan file 'esp_lcd_touch_axs15231b.h' dan .c dari example 
// sudah Anda letakkan di dalam folder 'src' (atau include/lib).
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
#define RS485_RX 16
#define RS485_TX 17

// --- INISIALISASI OBJEK HARDWARE ---
TCA9554 TCA(0x20);
Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_QSPI_CS, LCD_QSPI_CLK, LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3);
Arduino_GFX *gfx = new Arduino_AXS15231B(bus, -1 /* RST */, 0 /* rotation */, false, 320, 480);

HardwareSerial RS485Serial(1); 
ModbusMaster node;

// --- VARIABEL LVGL & MODBUS ---
uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf1;
lv_color_t *disp_draw_buf2;

unsigned long lastModbusPoll = 0;
const unsigned long POLLING_INTERVAL = 2000;

// Objek UI
lv_obj_t * ui_LabelGas;
lv_obj_t * ui_LabelSuhu;
lv_obj_t * ui_LabelKelembaban;
lv_obj_t * ui_LabelStatus;

// --- CALLBACK LVGL V9 ---
uint32_t millis_cb(void) {
  return millis();
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
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
  
  // Mengatur warna background layar menjadi gelap agar teks lebih jelas
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A1A), LV_PART_MAIN);

  // Membuat Panel / Kartu di tengah
  lv_obj_t * panel = lv_obj_create(scr);
  lv_obj_set_size(panel, 280, 300);
  lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);

  // Judul
  lv_obj_t * title = lv_label_create(panel);
  lv_label_set_text(title, "AQMS MONITORING");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_text_color(title, lv_color_hex(0x00A8FF), LV_PART_MAIN);

  // Label Gas
  ui_LabelGas = lv_label_create(panel);
  lv_label_set_text(ui_LabelGas, "Gas : Menunggu...");
  lv_obj_align(ui_LabelGas, LV_ALIGN_TOP_LEFT, 20, 60);

  // Label Suhu
  ui_LabelSuhu = lv_label_create(panel);
  lv_label_set_text(ui_LabelSuhu, "Suhu: Menunggu...");
  lv_obj_align(ui_LabelSuhu, LV_ALIGN_TOP_LEFT, 20, 100);

  // Label Kelembaban
  ui_LabelKelembaban = lv_label_create(panel);
  lv_label_set_text(ui_LabelKelembaban, "RH  : Menunggu...");
  lv_obj_align(ui_LabelKelembaban, LV_ALIGN_TOP_LEFT, 20, 140);

  // Label Status
  ui_LabelStatus = lv_label_create(panel);
  lv_label_set_text(ui_LabelStatus, "Stat: Menghubungkan...");
  lv_obj_align(ui_LabelStatus, LV_ALIGN_TOP_LEFT, 20, 180);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Sistem Memulai...");

  // 1. Inisialisasi Cip Ekspander (TCA9554) & I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  TCA.begin();
  TCA.pinMode1(1, OUTPUT);
  TCA.write1(1, 1); delay(10);
  TCA.write1(1, 0); delay(10);
  TCA.write1(1, 1); delay(200);

  // 2. Inisialisasi Touchscreen
  bsp_touch_init(&Wire, -1, 0, 320, 480);

  // 3. Inisialisasi Driver Grafis LCD
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_BLACK);
  
  // 4. NYALAKAN BACKLIGHT
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  // 5. Inisialisasi LVGL v9
  lv_init();
  lv_tick_set_cb(millis_cb);

  screenWidth = gfx->width();
  screenHeight = gfx->height();
  
  // Alokasi memori buffer LVGL (menggunakan PSRAM jika tersedia)
  bufSize = screenWidth * screenHeight;
  disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  
  if (!disp_draw_buf1 || !disp_draw_buf2) {
    Serial.println("Alokasi memori LVGL gagal!");
  } else {
    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf1, disp_draw_buf2, bufSize * 2, LV_DISPLAY_RENDER_MODE_FULL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
  }

  // 6. Buat Antarmuka Pengguna
  build_ui();

  // 7. Inisialisasi Modbus RS485
  RS485Serial.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
  node.begin(1, RS485Serial); 
}

void loop() {
  // Tugas 1: Proses animasi dan layar sentuh
  lv_task_handler(); 
  delay(5); 
  
  // Tugas 2: Proses data sensor secara berkala (Non-Blocking)
  unsigned long currentMillis = millis();
  if (currentMillis - lastModbusPoll >= POLLING_INTERVAL) {
    lastModbusPoll = currentMillis; 
    
    // Minta 4 register ke ESP Dummy (Slave)
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
            lv_obj_set_style_text_color(ui_LabelStatus, lv_color_hex(0x00FF00), LV_PART_MAIN); // Hijau
        } else {
            lv_label_set_text(ui_LabelStatus, "Stat: WARNING");
            lv_obj_set_style_text_color(ui_LabelStatus, lv_color_hex(0xFFA500), LV_PART_MAIN); // Oranye
        }
      }
    } else {
      if (ui_LabelStatus != NULL) {
         lv_label_set_text(ui_LabelStatus, "Stat: DISCONNECTED");
         lv_obj_set_style_text_color(ui_LabelStatus, lv_color_hex(0xFF0000), LV_PART_MAIN); // Merah
      }
    }
  }
}