
#include <Arduino.h>
#include <ModbusMaster.h> // Tambahan Library Modbus
#include <lvgl.h>

/*To use the built-in examples and demos of LVGL uncomment the includes below respectively.
 *You also need to copy `lvgl/examples` to `lvgl/src/examples`. Similarly for the demos `lvgl/demos` to `lvgl/src/demos`.
 Note that the `lv_examples` library is for LVGL v7 and you shouldn't install it for this version (since LVGL v8)
 as the examples and demos are now part of the main LVGL library. */

// #include <examples/lv_examples.h>
// #include <demos/lv_demos.h>

#define DIRECT_RENDER_MODE // Uncomment to enable full frame buffer

#include <Arduino_GFX_Library.h>
#include "TCA9554.h"
#include <Wire.h>
#include "esp_lcd_touch_axs15231b.h"

// --- Konfigurasi RS485 ---
#define RS485_RX 43
#define RS485_TX 44
HardwareSerial RS485Serial(1); 
ModbusMaster node;

// --- Variabel Global Label LVGL ---
lv_obj_t * label_gas;
lv_obj_t * label_temp;
lv_obj_t * label_hum;
lv_obj_t * label_status;

// --- Variabel Timing (Pengganti delay) ---
unsigned long lastModbusRead = 0;
const unsigned long modbusInterval = 2000;

#define GFX_BL 6  // default backlight pin, you may replace DF_GFX_BL to actual backlight pin

#define LCD_QSPI_CS   12
#define LCD_QSPI_CLK  5
#define LCD_QSPI_D0   1
#define LCD_QSPI_D1   2
#define LCD_QSPI_D2   3
#define LCD_QSPI_D3   4

#define I2C_SDA       8
#define I2C_SCL       7

TCA9554 TCA(0x20);

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_QSPI_CS, LCD_QSPI_CLK, LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3);

Arduino_GFX *gfx = new Arduino_AXS15231B(bus, -1 /* RST */, 0 /* rotation */, false, 320, 480);

uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_disp_draw_buf_t draw_buf;
lv_color_t *disp_draw_buf1;
lv_color_t *disp_draw_buf2;
lv_disp_drv_t disp_drv;

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf) {
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp_drv);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  touch_data_t touch_data;
  bsp_touch_read();

  if (bsp_touch_get_coordinates(&touch_data)) {
    data->state = LV_INDEV_STATE_PR;
    /*Set the coordinates*/
    data->point.x = touch_data.coords[0].x;
    data->point.y = touch_data.coords[0].y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// --- Fungsi Pembacaan Sensor RS485 & Update UI ---
void read_rs485_data() {
  uint8_t result;
  
  // Meminta 4 register dimulai dari alamat 0x0000
  result = node.readHoldingRegisters(0x0000, 4);

  if (result == node.ku8MBSuccess) {
    // Ambil data raw (integer) langsung dari Modbus
    uint16_t rawGas    = node.getResponseBuffer(0);
    uint16_t rawTemp   = node.getResponseBuffer(1);
    uint16_t rawHum    = node.getResponseBuffer(2);
    uint16_t rawStatus = node.getResponseBuffer(3);
    
    // PERBAIKAN: Hindari penggunaan float dan '%f'. 
// Gunakan %u karena rawGas, rawTemp, rawHum adalah unsigned 16-bit
    lv_label_set_text_fmt(label_gas, "Gas: %u.%u ppm", (rawGas / 10), (rawGas % 10));
    lv_label_set_text_fmt(label_temp, "Suhu: %u.%u C", (rawTemp / 10), (rawTemp % 10));
    lv_label_set_text_fmt(label_hum, "Kelembaban: %u.%u %%", (rawHum / 10), (rawHum % 10));
    
    // Status Logic
    if(rawStatus == 0) {
      lv_label_set_text(label_status, "Status: OK");
      lv_obj_set_style_text_color(label_status, lv_color_hex(0x00FF00), 0); // Hijau
    } else if(rawStatus == 1) {
      lv_label_set_text(label_status, "Status: WARNING");
      lv_obj_set_style_text_color(label_status, lv_color_hex(0xFFA500), 0); // Oranye
    } else {
      lv_label_set_text(label_status, "Status: ERROR");
      lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF0000), 0); // Merah
    }
    
    Serial.println("Data GUI Berhasil Diperbarui!");
  } else {
    Serial.printf("Error Code Modbus: %02X\n", result);
  }
}
void setup() {
#ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
#endif

  Wire.begin(I2C_SDA, I2C_SCL);
  TCA.begin();
  TCA.pinMode1(1, OUTPUT);
  TCA.write1(1, 1);
  delay(10);
  TCA.write1(1, 0);
  delay(10);
  TCA.write1(1, 1);
  delay(200);

  Serial.begin(115200);
  Serial.println("Arduino_GFX LVGL_Arduino_v8 example ");
  String LVGL_Arduino = String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  // Inisialisasi RS485
  RS485Serial.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
  node.begin(1, RS485Serial);

  // Init Display
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  screenWidth = gfx->width();
  screenHeight = gfx->height();

  bsp_touch_init(&Wire, -1, 0, screenWidth, screenHeight);

#ifdef DIRECT_RENDER_MODE
  bufSize = screenWidth * screenHeight;
#else
  bufSize = screenWidth * 80;
#endif

  disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
  disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);

  if (!disp_draw_buf1 || !disp_draw_buf2) {
    Serial.println("LVGL disp_draw_buf allocate failed!");
  } else {
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf1, disp_draw_buf2, bufSize);
    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
#ifdef DIRECT_RENDER_MODE
    disp_drv.full_refresh = true;
#endif
    lv_disp_drv_register(&disp_drv);

    /* Initialize the (dummy) input device driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

/* --- Membuat UI Pembacaan Sensor --- */
    label_gas = lv_label_create(lv_scr_act());
    lv_label_set_text(label_gas, "Gas: -- ppm");
    lv_obj_set_style_text_color(label_gas, lv_color_hex(0x000000), 0); // Set ke putih
    lv_obj_align(label_gas, LV_ALIGN_CENTER, 0, -60);

    label_temp = lv_label_create(lv_scr_act());
    lv_label_set_text(label_temp, "Suhu: -- C");
    lv_obj_set_style_text_color(label_temp, lv_color_hex(0x000000), 0); // Set ke putih
    lv_obj_align(label_temp, LV_ALIGN_CENTER, 0, -20);

    label_hum = lv_label_create(lv_scr_act());
    lv_label_set_text(label_hum, "Kelembaban: -- %");
    lv_obj_set_style_text_color(label_hum, lv_color_hex(0x000000), 0); // Set ke putih
    lv_obj_align(label_hum, LV_ALIGN_CENTER, 0, 20);

    label_status = lv_label_create(lv_scr_act());
    lv_label_set_text(label_status, "Status: --");
    // Status tidak perlu diset putih di sini jika Anda ingin defaultnya putih, 
    // tapi lebih aman diset juga agar konsisten sebelum Modbus terbaca.
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x000000), 0); 
    lv_obj_align(label_status, LV_ALIGN_CENTER, 0, 60);
  }

  Serial.println("Setup done");
}

void loop() {
  lv_timer_handler(); /* let the GUI do its work */

  // Pembacaan Modbus RS485 setiap 2 detik tanpa mem-block LVGL
  if (millis() - lastModbusRead >= modbusInterval) {
    lastModbusRead = millis();
    read_rs485_data();
  }

  delay(5);
}