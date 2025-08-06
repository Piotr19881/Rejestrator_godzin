
// ESP32 WROOM32 + ILI9341 TFT Configuration
#define USER_SETUP_LOADED 1

// Wybór sterownika
#define ILI9341_DRIVER

// ===== KONFIGURACJA TOUCH XPT2046 =====
#define TOUCH_CS 21     // T_CS pin dla touch controllera
#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL true

// Konfiguracja SPI
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY  40000000  // 40MHz SPI
#define SPI_READ_FREQUENCY 20000000

// Kolor tła domyślny
#define TFT_BLACK       0x0000
#define TFT_BLUE        0x001F
#define TFT_RED         0xF800
#define TFT_GREEN       0x07E0
#define TFT_CYAN        0x07FF
#define TFT_MAGENTA     0xF81F
#define TFT_YELLOW      0xFFE0
#define TFT_WHITE       0xFFFF
