# ESP32 RS485 Sensor Monitor with LVGL

Proyek ini adalah sistem monitoring sensor (Gas, Suhu, dan Kelembaban) menggunakan protokol komunikasi **Modbus RTU (RS485)**. Antarmuka pengguna (GUI) dibangun menggunakan library **LVGL v8.3** pada layar ESP32 dengan driver **AXS15231B**.

## 🚀 Fitur Utama

* **Real-time Monitoring**: Membaca data Gas (ppm), Suhu (°C), dan Kelembaban (%) secara real-time dari sensor RS485.
* **Modern UI**: Menggunakan LVGL v8.3 untuk tampilan yang responsif dan estetis.
* **Modbus RTU Master**: Implementasi protokol industri yang stabil untuk pembacaan data sensor.
* **Status Indicator**: Visualisasi status (OK, WARNING, ERROR) dengan perubahan warna teks dinamis.
* **Non-blocking Logic**: Pembacaan sensor dan update UI berjalan secara independen menggunakan `millis()` tanpa mengganggu performa rendering layar.

## 🛠️ Persyaratan Perangkat Lunak

Pastikan Anda telah menginstal library berikut melalui Library Manager di VS Code (PlatformIO) atau Arduino IDE:

1.  **LVGL (v8.3.x)**
2.  **Arduino_GFX_Library**
3.  **ModbusMaster** (oleh docwalker)
4.  **Wire** & **SPI** (Built-in ESP32)
5.  **TCA9554** (untuk I2C Expander pada beberapa board display)

## 🔌 Konfigurasi Pinout

### Layar (QSPI Interface)
| Komponen | Pin ESP32 |
| :--- | :--- |
| LCD CS | 12 |
| LCD CLK | 5 |
| LCD D0 | 1 |
| LCD D1 | 2 |
| LCD D2 | 3 |
| LCD D3 | 4 |
| Backlight (BL) | 6 |

### Komunikasi RS485 (Modbus)
| Jalur | Pin ESP32 |
| :--- | :--- |
| RS485 RX | 43 |
| RS485 TX | 44 |

### I2C (Touch & Expander)
| Jalur | Pin ESP32 |
| :--- | :--- |
| SDA | 8 |
| SCL | 7 |

## 📂 Struktur Proyek

* `LayarLCD.cpp`: Kode utama untuk Master (Display). Berisi inisialisasi LVGL, GFX, dan logika pembacaan Modbus.
* `dummyslave.cpp`: Kode simulasi untuk Slave. Gunakan ini jika Anda tidak memiliki sensor fisik untuk mencoba komunikasi RS485.

## 🛠️ Cara Instalasi

1.  **Clone Repositori**:
    ```bash
    git clone [https://github.com/username/repository-name.git](https://github.com/username/repository-name.git)
    ```
2.  **Buka di VS Code**:
    Pastikan ekstensi **PlatformIO** atau **Arduino** sudah terpasang.
3.  **Konfigurasi LVGL**:
    Salin file `lv_conf_template.h` menjadi `lv_conf.h`. Pastikan memori dan warna diatur dengan benar (`LV_COLOR_DEPTH` biasanya 16 untuk RGB565).
4.  **Upload**:
    Pilih port yang sesuai dan klik **Upload**.

## 🧪 Simulasi (Testing)

Jika Anda melakukan pengujian menggunakan `dummyslave.cpp` pada ESP32 kedua:
1.  Hubungkan **TX Master (44)** ke **RX Slave (16)**.
2.  Hubungkan **RX Master (43)** ke **TX Slave (17)**.
3.  Pastikan kedua board berbagi **Ground (GND)** yang sama.
4.  Data simulasi akan dikirim setiap 3 detik dan langsung dirender pada antarmuka LVGL di Master.

## 📝 Catatan Tambahan

* **Warna Teks**: Pastikan `lv_obj_set_style_text_color` telah dikonfigurasi ke warna terang (seperti putih `0xFFFFFF`) pada kode master jika menggunakan latar belakang gelap (`RGB565_BLACK`).
* **RS485 Termination**: Untuk instalasi di lapangan dengan kabel yang panjang, pastikan menambahkan resistor terminasi 120 ohm di kedua ujung jaringan.

---
*Dikembangkan oleh Tim R&D PT Artium Multikarya Indonesia.*