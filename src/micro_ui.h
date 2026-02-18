/* micro_ui is a lightweight, modular user interface layer 
   designed for embedded projects using TFT_eSPI and XPT2046-based 
   touchscreen displays. It provides a clean structure for managing 
   touchable UI elements like buttons, labels, and sliders, with 
   smooth rendering via TFT_eSprite and intuitive touch handling.

✨ Features
- Buttons with visual press feedback and callback support.
- Callback functions receive the button label for simple identification.
- Each UI element includes an identifier for efficient updates and tracking.
- Sliders with draggable and full-track touch support, with callbacks.
- Labels rendered via off-screen sprites for flicker-free updates.
- Touch handler with state tracking and debounce logic.
- Progress bars, common shapes, and direct text drawing support.
- Designed for use with ESP32 and similar microcontrollers.
*/

#ifndef MICRO_UI_H
#define MICRO_UI_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

extern TFT_eSPI tft;

// ===== Available UI Elements (Save some ram) =====
#define MICRO_UI_USE_BUTTONS
#define MICRO_UI_USE_LABELS
#define MICRO_UI_USE_SLIDERS

// ===== Touchscreen Setup =====
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

extern SPIClass touchscreenSPI;
extern XPT2046_Touchscreen touchscreen;

// ===== Screen Setup =====
#define SCREEN_ROTATION     1 
#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       240

// Debug touch calibration values
// #define DEBUG_TOUCH

// Values used to map the raw touch coordinates to screen pixels.
#define MIN_TOUCH_X         268
#define MAX_TOUCH_X         3814
#define MIN_TOUCH_Y         402
#define MAX_TOUCH_Y         3732

#define BACKGROUND_COLOR    TFT_BLACK

// Nicer green for the CYD
#ifdef TFT_GREEN
  #undef TFT_GREEN
#endif
#define TFT_GREEN           0x05E8

// NOTE: Labels use off-screen sprites for flicker-free updates.
// Each label reserves a sprite in RAM — e.g., a 10-character word
// at Font 4 (~260×48 px) consumes ~24 KB. Use drawText() instead
// for direct, no-buffer text rendering when conserving RAM.

// UI memory usage estimates (ESP32 / 32-bit MCU assumed)
//
// Struct sizes (estimated):
// SimpleButton:   ~64 bytes
// LabelSprite:    ~68 bytes (not including sprite data)
// SliderSprite:   ~60 bytes (not including sprite data)
//
// ================ Configurable Limits ==================
// The following values define the maximum number of UI elements.
// ESP32 has generous RAM, so defaults are safe for most apps.
// However, if you wish to tighten memory usage, simply reduce
// these values to match the number of buttons, labels, or sliders
// you actually need in your UI.

#define MAX_BUTTONS         20    // 20x = ~1280 bytes total (20 x 64)
#define MAX_BUTTON_TEXT     10    // Each button has label[10]
#define MAX_LABELS          20    // 20x = ~1360 bytes total (20 x 68)
#define MAX_LABEL_TEXT      32    // Each label has lastText[32]
#define MAX_SLIDERS         10    // 10x = ~600 bytes total (10 x 60)

#define BUTTON_DEBOUNCE_MS  25    // Debounce time - ignore glitchy touches

#define SLIDER_TRACK_THICKNESS  6
#define SLIDER_BUTTON_SIZE      30

void safeCopy(char* dest, const char* src, size_t maxLen);
void safeCopy(char* dest, char* src, size_t maxLen);

extern unsigned long   touchStartTime;

enum Quarter {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT
};

#ifdef MICRO_UI_USE_BUTTONS
    struct SimpleButton {
        int x, y, w, h;
        char label[MAX_BUTTON_TEXT];
        void (*callback)(const char *label);
        bool visible;
        uint8_t fontCode;
        uint16_t bgNormal;
        uint16_t bgPressed;
        uint16_t textColor;
        bool pressed;
        uint32_t generation = 0;
        bool inUse = false;
    };

    extern int             currentButtonIndex;
    extern SimpleButton    buttonList[MAX_BUTTONS];
    struct ButtonHandle    { int index = -1; uint32_t generation = 0; };

    ButtonHandle addButton(int x, int y, int w, int h, const char *label, void (*callback)(const char *label), uint8_t fontCode = 4, uint16_t bgNormal = TFT_BLUE, uint16_t bgPressed = TFT_BLACK);
    void updateButton(ButtonHandle handle, const char* newLabel, uint16_t bgNormal, uint16_t bgPressed);
    void updateButton(ButtonHandle handle, const char* newLabel, uint16_t bgNormal);
    void updateButton(ButtonHandle handle, const char* newLabel);
    void drawButton(const SimpleButton &btn);
    void drawAllButtons();
    void clearButton(ButtonHandle handle);
    void removeButton(ButtonHandle handle);
    void removeAllButtons();
#endif

#ifdef MICRO_UI_USE_LABELS
    struct LabelSprite {
        int x, y, w, h;
        TFT_eSprite* sprite;
        bool visible;
        uint8_t fontCode;
        uint16_t textColor;
        uint16_t bgColor;
        char lastText[MAX_LABEL_TEXT];
        uint32_t generation = 0;
        bool inUse = false;
    };

    extern LabelSprite     labelList [MAX_LABELS];
    struct LabelHandle     { int index = -1; uint32_t generation = 0; };

    LabelHandle addLabel(int x, int y, const char* text, uint8_t fontCode = 4, uint16_t textColor = TFT_WHITE, uint16_t bgColor = TFT_BLACK);
    void updateLabel(LabelHandle handle, const char* text, uint16_t textColor, uint16_t bgColor);
    void updateLabel(LabelHandle handle, const char* text, uint16_t textColor);
    void updateLabel(LabelHandle handle, const char* text); 
    void clearLabel(LabelHandle handle);
    void removeLabel(LabelHandle handle);
    void removeAllLabels();
#endif

#ifdef MICRO_UI_USE_SLIDERS
    struct SliderSprite {
        int x, y, w, h;                 // Container coordinates and dimensions
        TFT_eSprite *sprite;           // Off-screen sprite for smooth drawing
        void (*callback)(int value);   // Callback receives slider value (0–100)

        uint16_t trackColor;
        uint16_t buttonColorNormal;
        uint16_t buttonColorPressed;
        bool visible;
        bool pressed;
        int value;                     // 0–100
        uint32_t generation = 0;
        bool inUse = false;
    };

    extern int             currentSliderIndex;
    extern SliderSprite    sliderList[MAX_SLIDERS];
    struct SliderHandle    { int index = -1; uint32_t generation = 0; };

    SliderHandle addSlider(int x, int y, int w, int h, int value, void (*callback)(int value), uint16_t trackColor, uint16_t buttonColorNormal, uint16_t buttonColorPressed);
    void drawSlider(const SliderSprite &sldr);
    void drawAllSliders();
    void clearSlider(SliderHandle handle);
    void removeSlider(SliderHandle handle);
    void removeAllSliders();
#endif

// ===== Dirty drawing functions =====
void drawText(int x, int y, const char* txt, uint8_t fontCode = 4, uint16_t textColor = TFT_WHITE);
void drawCenteredText(const char *message, uint8_t fontCode = 4, uint16_t textColor = TFT_WHITE, uint16_t bgColor = BACKGROUND_COLOR);
void drawTriangleWithBorder(int x, int y, int w, int h, int borderWidth = 1, uint16_t fillColor = TFT_BLACK, int16_t borderColor = TFT_WHITE);
void drawCircleWithBorder(int x, int y, int radius, int borderWidth = 1, uint16_t fillColor = TFT_BLUE, uint16_t borderColor = TFT_WHITE);
void drawQuarterCircleWithBorder(int x, int y, int radius, int borderWidth = 1, uint16_t fillColor = TFT_BLUE, uint16_t borderColor = TFT_WHITE, Quarter quarter = BOTTOM_RIGHT);

// Gneral functions
void microUIInit();
void microUILoopHandler();
bool getTouch(int &x, int &y);
void clearScreen();

#endif
