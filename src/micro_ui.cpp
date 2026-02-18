// Micro UI library for TFT displays with touch support
#include "micro_ui.h"

TFT_eSPI tft = TFT_eSPI();  // TFT_eSPI uses pins defined in User_Setup.h
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

unsigned long   touchStartTime      = 0;

#ifdef MICRO_UI_USE_BUTTONS
// ===== Button Handling =====
SimpleButton    buttonList[MAX_BUTTONS];
int             currentButtonIndex  = -1;

ButtonHandle addButton(int x, int y, int w, int h, const char *label, void (*callback)(const char *label), uint8_t fontCode, uint16_t bgNormal, uint16_t bgPressed) {
    for (int i = 0; i < MAX_BUTTONS; i++) {
        if (!buttonList[i].inUse) {
            SimpleButton &btn = buttonList[i];
            btn.x = x;
            btn.y = y;
            btn.w = w;
            btn.h = h;
            safeCopy(btn.label, label, MAX_BUTTON_TEXT);
            btn.callback = callback;
            btn.visible = true;
            btn.fontCode = fontCode;
            btn.bgNormal = bgNormal;
            btn.bgPressed = bgPressed;
            btn.pressed = false;
            btn.generation++;
            btn.inUse = true;
            
            ButtonHandle result; 
            result.index = i; 
            result.generation = btn.generation; 
            return result;
        }
    }
    ButtonHandle result;
    result.index = -1; 
    result.generation = 0;  // Error: no available button slots
    return result;
}

void updateButton(ButtonHandle handle, const char* newLabel, uint16_t bgNormal, uint16_t bgPressed) {
    if (handle.index < 0 || handle.index >= MAX_BUTTONS) return;
    SimpleButton &btn = buttonList[handle.index];
    if (!btn.inUse || btn.generation != handle.generation) return;
    btn.bgNormal = bgNormal;
    btn.bgPressed = bgPressed;
    updateButton(handle, newLabel);
}

void updateButton(ButtonHandle handle, const char* newLabel, uint16_t bgNormal) {
    if (handle.index < 0 || handle.index >= MAX_BUTTONS) return;
    SimpleButton &btn = buttonList[handle.index];
    if (!btn.inUse || btn.generation != handle.generation) return;
    btn.bgNormal = bgNormal;
    updateButton(handle, newLabel);
}

void updateButton(ButtonHandle handle, const char* newLabel) {
    if (handle.index < 0 || handle.index >= MAX_BUTTONS) return;
    SimpleButton &btn = buttonList[handle.index];
    if (!btn.inUse || btn.generation != handle.generation) return;
    safeCopy(btn.label, newLabel, MAX_BUTTON_TEXT);
    drawButton(btn);
}

void drawButton(const SimpleButton &btn) {
    if (!btn.visible) 
        return;
    uint16_t bgColor = btn.pressed ? btn.bgPressed : btn.bgNormal;
    tft.fillRect(btn.x, btn.y, btn.w, btn.h, bgColor);
    tft.drawRect(btn.x, btn.y, btn.w, btn.h, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, bgColor);
    tft.setTextFont(btn.fontCode);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(btn.label, btn.x + btn.w / 2, btn.y + btn.h / 2 + 2);
}

void drawAllButtons() {
    for (int i = 0; i < MAX_BUTTONS; i++) {
        if (buttonList[i].inUse) drawButton(buttonList[i]);
    }
}

void clearButton(ButtonHandle handle) {
    if (handle.index < 0 || handle.index >= MAX_BUTTONS) return;
    SimpleButton &btn = buttonList[handle.index];
    if (!btn.inUse || btn.generation != handle.generation) return;
    tft.fillRect(btn.x, btn.y, btn.w, btn.h, BACKGROUND_COLOR);
    tft.drawRect(btn.x, btn.y, btn.w, btn.h, TFT_WHITE);
}

void removeButton(ButtonHandle handle) {
    if (handle.index < 0 || handle.index >= MAX_BUTTONS) return;
    SimpleButton &btn = buttonList[handle.index];
    if (!btn.inUse || btn.generation != handle.generation) return;
    btn.inUse = false;
}

void removeAllButtons() {
    for (int i = 0; i < MAX_BUTTONS; i++) buttonList[i].inUse = false;
}

void handleTouchButtons(int tx, int ty) {
    if (currentButtonIndex != -1) return;

    for (int i = 0; i < MAX_BUTTONS; i++) {
        SimpleButton &btn = buttonList[i];
        if (!btn.inUse || !btn.visible) continue;

        if (tx >= btn.x && tx <= (btn.x + btn.w) &&
            ty >= btn.y && ty <= (btn.y + btn.h)) {
            currentButtonIndex = i;
            touchStartTime = millis();
            btn.pressed = true;
            drawButton(btn);
            break;
        }
    }
}

void releaseActiveButton() {
    if (currentButtonIndex == -1) return;

    int i = currentButtonIndex;
    currentButtonIndex = -1;

    if (buttonList[i].inUse && (millis() - touchStartTime >= BUTTON_DEBOUNCE_MS)) {
        if (buttonList[i].callback) {
            buttonList[i].callback(buttonList[i].label);
        }
    }

    if (buttonList[i].inUse) {
        buttonList[i].pressed = false;
        drawButton(buttonList[i]);
    }
}
#endif


#ifdef MICRO_UI_USE_LABELS
// ===== Label Handling =====
LabelSprite     labelList [MAX_LABELS];

LabelHandle addLabel(int x, int y, const char* text, uint8_t fontCode, uint16_t textColor, uint16_t bgColor) {
    for (int i = 0; i < MAX_LABELS; i++) {
        if (!labelList[i].inUse) {
            LabelSprite &lbl = labelList[i];
            safeCopy(lbl.lastText, text, MAX_LABEL_TEXT);
            lbl.visible = true;
            lbl.fontCode = fontCode;
            lbl.textColor = textColor;
            lbl.bgColor = bgColor;

            // Temporary sprite to measure text
            TFT_eSprite temp(&tft);
            temp.setColorDepth(8);
            temp.createSprite(1, 1);
            temp.setTextFont(fontCode);
            temp.setTextDatum(MC_DATUM);

            int w = temp.textWidth(text) + 10;
            int h = temp.fontHeight() + 4;

            // Clip width/height to screen
            if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
            if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;

            // Fallback if it's still out of bounds
            if (w <= 0 || h <= 0) break;

            lbl.x = x;
            lbl.y = y;
            lbl.w = w;
            lbl.h = h;

            lbl.sprite = new TFT_eSprite(&tft);
            lbl.sprite->setColorDepth(8);
            lbl.sprite->createSprite(w, h);
            lbl.sprite->fillSprite(bgColor);
            lbl.sprite->setTextColor(textColor, bgColor);
            lbl.sprite->setTextFont(fontCode);
            lbl.sprite->setTextDatum(MC_DATUM);
            lbl.sprite->drawString(text, w / 2, h / 2);
            lbl.sprite->pushSprite(x, y);

            lbl.generation++;
            lbl.inUse = true;

            LabelHandle result;
            result.index = i;
            result.generation = lbl.generation;
            return result;
        }
    }

    // Failure fallback
    LabelHandle errorHandle;
    errorHandle.index = -1;
    errorHandle.generation = 0;
    return errorHandle;
}

void updateLabel(LabelHandle handle, const char* text) {
    if (handle.index < 0 || handle.index >= MAX_LABELS) return;
    LabelSprite &lbl = labelList[handle.index];
    updateLabel(handle, text, lbl.textColor, lbl.bgColor);
}

void updateLabel(LabelHandle handle, const char* text, uint16_t textColor) {
    if (handle.index < 0 || handle.index >= MAX_LABELS) return;
    LabelSprite &lbl = labelList[handle.index];
    lbl.textColor = textColor;
    updateLabel(handle, text, textColor, lbl.bgColor);
}

void updateLabel(LabelHandle handle, const char* text, uint16_t textColor, uint16_t bgColor) {
    if (handle.index < 0 || handle.index >= MAX_LABELS) return;
    LabelSprite &lbl = labelList[handle.index];
    if (!lbl.inUse || lbl.generation != handle.generation) return;

    if (strncmp(lbl.lastText, text, MAX_LABEL_TEXT) != 0) {
        // Create temp sprite to measure new size
        TFT_eSprite temp(&tft);
        temp.setColorDepth(8);
        temp.createSprite(1, 1); // Placeholder
        temp.setTextFont(lbl.fontCode);
        temp.setTextDatum(MC_DATUM);
        int newW = temp.textWidth(text) + 10;
        int newH = temp.fontHeight() + 4;
        temp.deleteSprite();

        // Clip to screen bounds
        if (lbl.x + newW > SCREEN_WIDTH) newW = SCREEN_WIDTH - lbl.x;
        if (lbl.y + newH > SCREEN_HEIGHT) newH = SCREEN_HEIGHT - lbl.y;

        // Save previous size for cleanup
        int prevW = lbl.w;
        int prevH = lbl.h;

        // Resize sprite if needed
        if (newW != lbl.w || newH != lbl.h) {
            lbl.w = newW;
            lbl.h = newH;

            if (lbl.sprite) {
                delete lbl.sprite;
                lbl.sprite = nullptr;
            }
            lbl.sprite = new TFT_eSprite(&tft);
            lbl.sprite->setColorDepth(8);
            lbl.sprite->createSprite(lbl.w, lbl.h);
            lbl.sprite->setTextFont(lbl.fontCode);
            lbl.sprite->setTextDatum(MC_DATUM);
        }

        // Set colors and draw
        lbl.textColor = textColor;
        lbl.bgColor = bgColor;
        lbl.sprite->setTextColor(textColor, bgColor);
        lbl.sprite->fillSprite(bgColor);
        lbl.sprite->drawString(text, lbl.w / 2, lbl.h / 2);
        lbl.sprite->pushSprite(lbl.x, lbl.y);

        // Clear leftover area from previous larger label
        if (prevW > lbl.w) {
            int dx = prevW - lbl.w;
            tft.fillRect(lbl.x + lbl.w, lbl.y, dx, lbl.h, bgColor);
        }
        if (prevH > lbl.h) {
            int dy = prevH - lbl.h;
            tft.fillRect(lbl.x, lbl.y + lbl.h, lbl.w, dy, bgColor);
        }

        // Track last label
        safeCopy(lbl.lastText, text, MAX_LABEL_TEXT);
    }
}

void clearLabel(LabelHandle handle) {
    if (handle.index < 0 || handle.index >= MAX_LABELS) return;
    LabelSprite &lbl = labelList[handle.index];
    if (!lbl.inUse || lbl.generation != handle.generation || !lbl.sprite) return;
    lbl.sprite->fillSprite(lbl.bgColor);
    lbl.sprite->pushSprite(lbl.x, lbl.y);
}

void removeLabel(LabelHandle handle) {
    if (handle.index < 0 || handle.index >= MAX_LABELS) return;
    LabelSprite &lbl = labelList[handle.index];
    if (!lbl.inUse || lbl.generation != handle.generation) return;
    if (lbl.sprite) {
        delete lbl.sprite;
        lbl.sprite = nullptr;
    }
    lbl.inUse = false;
}

void removeAllLabels() {
    for (int i = 0; i < MAX_LABELS; i++) {
        if (labelList[i].inUse) {
            if (labelList[i].sprite) {
                delete labelList[i].sprite;
                labelList[i].sprite = nullptr;
            }
            labelList[i].inUse = false;
        }
    }
}
#endif

#ifdef MICRO_UI_USE_SLIDERS
// ===== Slider Functions =====
SliderSprite    sliderList[MAX_SLIDERS];
int             currentSliderIndex  = -1;

SliderHandle addSlider(int x, int y, int w, int h, int value, void (*callback)(int value), uint16_t trackColor = TFT_WHITE, uint16_t buttonColorNormal = TFT_BLUE, uint16_t buttonColorPressed = TFT_BLACK) {
    for (int i = 0; i < MAX_SLIDERS; i++) {
        if (!sliderList[i].inUse) {
            SliderSprite &sldr = sliderList[i];
            sldr.x = x;
            sldr.y = y;
            sldr.w = w;
            sldr.h = h;
            sldr.callback = callback;
            sldr.trackColor = trackColor;
            sldr.buttonColorNormal = buttonColorNormal;
            sldr.buttonColorPressed = buttonColorPressed;
            sldr.visible = true;
            sldr.pressed = false;
            sldr.value = constrain(value, 0, 100);
            sldr.generation++;
            sldr.inUse = true;

            // Create sprite
            sldr.sprite = new TFT_eSprite(&tft);
            sldr.sprite->setColorDepth(8);
            sldr.sprite->createSprite(w, h);

            // Return handle
            SliderHandle result;
            result.index = i;
            result.generation = sldr.generation;
            return result;
        }
    }

    // Failed to allocate
    SliderHandle result;
    result.index = -1;
    result.generation = 0;
    return result;
}

void drawSlider(const SliderSprite &sldr) {
    if (!sldr.visible || !sldr.sprite) return;

    int range = sldr.w - SLIDER_BUTTON_SIZE;
    int thumbX = (sldr.value * range) / 100;
    int thumbY = (sldr.h - SLIDER_BUTTON_SIZE) / 2;

    sldr.sprite->fillSprite(BACKGROUND_COLOR);

    // Draw track
    int trackY = (sldr.h - SLIDER_TRACK_THICKNESS) / 2;
    sldr.sprite->fillRect(0, trackY, sldr.w, SLIDER_TRACK_THICKNESS, sldr.trackColor);

    // Draw thumb button
    uint16_t btnColor = sldr.pressed ? sldr.buttonColorPressed : sldr.buttonColorNormal;
    sldr.sprite->fillRect(thumbX, thumbY, SLIDER_BUTTON_SIZE, SLIDER_BUTTON_SIZE, btnColor);
    sldr.sprite->drawRect(thumbX, thumbY, SLIDER_BUTTON_SIZE, SLIDER_BUTTON_SIZE, TFT_WHITE);

    // Draw text centered inside thumb
    char buffer[6];
    sprintf(buffer, "%d", sldr.value);
    sldr.sprite->setTextDatum(MC_DATUM);
    sldr.sprite->setTextColor(TFT_WHITE, btnColor);
    sldr.sprite->setTextFont(2);
    sldr.sprite->drawString(buffer, thumbX + SLIDER_BUTTON_SIZE / 2, thumbY + SLIDER_BUTTON_SIZE / 2);

    // Push to screen
    sldr.sprite->pushSprite(sldr.x, sldr.y);
}


void drawAllSliders() {
    for (int i = 0; i < MAX_SLIDERS; i++) {
        if (sliderList[i].inUse) drawSlider(sliderList[i]);
    }
}

void clearSlider(SliderHandle handle) {
    if (handle.index < 0 || handle.index >= MAX_SLIDERS) return;
    SliderSprite &sldr = sliderList[handle.index];
    if (!sldr.inUse || sldr.generation != handle.generation || !sldr.sprite) return;

    sldr.sprite->fillSprite(BACKGROUND_COLOR);
    sldr.sprite->pushSprite(sldr.x, sldr.y);
}

void removeSlider(SliderHandle handle) {
    if (handle.index < 0 || handle.index >= MAX_SLIDERS) return;
    SliderSprite &sldr = sliderList[handle.index];
    if (!sldr.inUse || sldr.generation != handle.generation) return;

    if (sldr.sprite) {
        delete sldr.sprite;
        sldr.sprite = nullptr;
    }
    sldr.inUse = false;
}

void removeAllSliders() {
    for (int i = 0; i < MAX_SLIDERS; i++) {
        if (sliderList[i].inUse) {
            if (sliderList[i].sprite) {
                delete sliderList[i].sprite;
                sliderList[i].sprite = nullptr;
            }
            sliderList[i].inUse = false;
        }
    }
}

void updateSliderValueFromTouch(SliderSprite &sldr, int tx) {
    int range = sldr.w - SLIDER_BUTTON_SIZE;
    int newValue = ((tx - sldr.x - SLIDER_BUTTON_SIZE / 2) * 100) / range;
    newValue = constrain(newValue, 0, 100);

    if (newValue != sldr.value) {
        sldr.value = newValue;
        drawSlider(sldr);
    }
}

void handleTouchSliders(int tx, int ty) {
    if (currentSliderIndex != -1) {
        SliderSprite &sldr = sliderList[currentSliderIndex];
        updateSliderValueFromTouch(sldr, tx);
        return;
    }

    for (int i = 0; i < MAX_SLIDERS; i++) {
        SliderSprite &sldr = sliderList[i];
        if (!sldr.inUse || !sldr.visible) continue;

        if (tx >= sldr.x && tx <= (sldr.x + sldr.w) &&
            ty >= sldr.y && ty <= (sldr.y + sldr.h)) {
            currentSliderIndex = i;
            touchStartTime = millis();
            sldr.pressed = true;
            updateSliderValueFromTouch(sldr, tx);
            break;
        }
    }
}

void releaseActiveSlider() {
    if (currentSliderIndex == -1) return;

    int i = currentSliderIndex;
    currentSliderIndex = -1;

    SliderSprite &sldr = sliderList[i];
    sldr.pressed = false;
    drawSlider(sldr);

    if (millis() - touchStartTime >= BUTTON_DEBOUNCE_MS) {
        if (sldr.callback) {
            sldr.callback(sldr.value);
        }
    }
}
#endif


void clearScreen() {
    tft.fillScreen(BACKGROUND_COLOR);
#ifdef MICRO_UI_USE_BUTTONS
    removeAllButtons();
#endif
#ifdef MICRO_UI_USE_SLIDERS
    removeAllSliders();
#endif
#ifdef MICRO_UI_USE_LABELS
    removeAllLabels();
#endif
}

void microUILoopHandler() {
    int tx, ty;
    if (getTouch(tx, ty)) {
#ifdef MICRO_UI_USE_BUTTONS
        handleTouchButtons(tx, ty);
#endif
#ifdef MICRO_UI_USE_SLIDERS
        handleTouchSliders(tx, ty);
#endif
    } else {
#ifdef MICRO_UI_USE_BUTTONS
        releaseActiveButton();
#endif
#ifdef MICRO_UI_USE_SLIDERS
        releaseActiveSlider();
#endif
    }
}

// ===== Touch Handling =====
bool getTouch(int &x, int &y) {
    if (touchscreen.touched()) {
        TS_Point p = touchscreen.getPoint();
#ifdef DEBUG_TOUCH
        Serial.print("Raw Touch: X=");
        Serial.print(p.x);
        Serial.print(" Y=");
        Serial.print(p.y);
        Serial.print(" Z=");
        Serial.println(p.z);
#endif
        // Map raw touch readings to screen pixel values.
        x = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 0, SCREEN_WIDTH);
        y = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 0, SCREEN_HEIGHT);

        // Ensure the mapped coordinates are within screen bounds.
        x = constrain(x, 0, SCREEN_WIDTH);
        y = constrain(y, 0, SCREEN_HEIGHT);
#ifdef DEBUG_TOUCH
        Serial.print("Mapped Touch: X=");
        Serial.print(x);
        Serial.print(" Y=");
        Serial.println(y);
#endif
        return true;
    }
    return false;
}

void drawProgressBar(int x, int y, int w, int h, int value, uint16_t fillColor = TFT_GREEN, uint16_t bgColor = TFT_DARKGREY) {
    // Clamp to [0, 100]
    if (value < 0) value = 0;
    if (value > 100) value = 100;

    // Colors
    uint16_t borderColor = TFT_WHITE;

    // Outer border (1px)
    tft.drawRect(x, y, w, h, borderColor);

    // Inner area dimensions (excluding border)
    int innerX = x + 1;
    int innerY = y + 1;
    int innerW = w - 2;
    int innerH = h - 2;

    // Clear background inside bar
    tft.fillRect(innerX, innerY, innerW, innerH, bgColor);

    // Filled width
    int fillW = (innerW * value) / 100;

    // Draw filled portion
    if (fillW > 0) {
        tft.fillRect(innerX, innerY, fillW, innerH, fillColor);
    }
}

void FullWidthProgressBar(int value) {
    drawProgressBar(10, SCREEN_HEIGHT / 2 - 15, SCREEN_WIDTH-20, 30, value);
};

inline void safeCopy(char* dest, const char* src, size_t maxLen) {
    if (!dest || !src || maxLen == 0) 
        return;
    memset(dest, 0, maxLen);
    strncpy(dest, src, maxLen - 1);
}

inline void safeCopy(char* dest, char* src, size_t maxLen) {
    if (!dest || !src || maxLen == 0) 
        return;
    memset(dest, 0, maxLen);
    strncpy(dest, src, maxLen - 1);
}

void microUIInit() {
    tft.begin();
    tft.setRotation(1);  // Match your display orientation
    tft.fillScreen(BACKGROUND_COLOR);

    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    touchscreen.setRotation(SCREEN_ROTATION);  // Match your screen orientation
}

void drawTriangleWithBorder(int x, int y, int w, int h, int borderWidth, uint16_t fillColor, int16_t borderColor) {
    int cx = x + w / 2;
    int top = y;
    int left = x;
    int right = x + w;
    int bottom = y + h;

    // always fill (defaults to TFT_BLACK)
    tft.fillTriangle(cx, top, left, bottom, right, bottom, fillColor);

    // Draw border if requested
    if (borderWidth > 0) {
        for (int i = 0; i < borderWidth; i++) {
            // Offset inward to create border thickness
            tft.drawLine(cx, top + i, left + i, bottom - i, borderColor);
            tft.drawLine(left + i, bottom - i, right - i, bottom - i, borderColor);
            tft.drawLine(right - i, bottom - i, cx, top + i, borderColor);
        }
    }
}

void drawCircleWithBorder(int x, int y, int radius, int borderWidth, uint16_t fillColor, uint16_t borderColor) {
    TFT_eSprite spr = TFT_eSprite(&tft);
    int size = radius * 2 + 1;
    spr.createSprite(size, size);
    spr.fillSprite(TFT_TRANSPARENT); // Or a background color if needed
  
    int cx = radius; // Center of the circle
    int cy = radius;
    int outerR2 = radius * radius;
    int innerR = radius - borderWidth;
    int innerR2 = innerR * innerR;
  
    for (int sy = 0; sy < size; sy++) {
      for (int sx = 0; sx < size; sx++) {
        int dx = sx - cx;
        int dy = sy - cy;
        int dist2 = dx * dx + dy * dy;
  
        if (dist2 <= outerR2) {
          spr.drawPixel(sx, sy, borderColor);
          if (dist2 <= innerR2) {
            spr.drawPixel(sx, sy, fillColor);
          }
        }
      }
    }
  
    spr.pushSprite(x, y);
    spr.deleteSprite();
}

void drawQuarterCircleWithBorder(int x, int y, int radius, int borderWidth, uint16_t fillColor, uint16_t borderColor, Quarter quarter) {
    // The sprite will be sized so that indices run from 0 to radius.
    TFT_eSprite spr = TFT_eSprite(&tft);
    int size = radius + 1;
    spr.createSprite(size, size);
    spr.fillSprite(TFT_TRANSPARENT);  // Or use a background color if needed
  
    int outerR = radius;                  // Outer radius for the border
    int innerR = radius - borderWidth;    // Inner radius for the fill
  
    // ----------------------------------------
    // Step 1: Draw the entire quarter in borderColor.
    // We map each pixel (sx, sy) to a local coordinate (dx,dy)
    // based on the quarter's anchor:
    //   TOP_LEFT:        dx = sx,             dy = sy
    //   TOP_RIGHT:       dx = radius - sx,    dy = sy
    //   BOTTOM_LEFT:     dx = sx,             dy = radius - sy
    //   BOTTOM_RIGHT:    dx = radius - sx,    dy = radius - sy
    for (int sy = 0; sy <= radius; sy++) {
      for (int sx = 0; sx <= radius; sx++) {
        int dx, dy;
        switch (quarter) {
          case TOP_LEFT:
            dx = sx;
            dy = sy;
            break;
          case TOP_RIGHT:
            dx = radius - sx;
            dy = sy;
            break;
          case BOTTOM_LEFT:
            dx = sx;
            dy = radius - sy;
            break;
          case BOTTOM_RIGHT:
            dx = radius - sx;
            dy = radius - sy;
            break;
        }
        // If within the outer quarter circle, draw the border color.
        if (dx * dx + dy * dy <= outerR * outerR) {
          spr.drawPixel(sx, sy, borderColor);
        }
      }
    }
  
    // ----------------------------------------
    // Step 2: Inset the fill.
    // For each pixel that lies inside the inner circle (with radius innerR)
    // we redraw it with fillColor. However, if the pixel is on the flat (vertical/horizontal)
    // border edge, we leave it as borderColor.
    //
    // The “flat edge” for each quarter is defined as follows:
    //   TOP_LEFT:     sx < borderWidth   OR   sy < borderWidth
    //   TOP_RIGHT:    sx >= (size - borderWidth)   OR   sy < borderWidth
    //   BOTTOM_LEFT:  sx < borderWidth   OR   sy >= (size - borderWidth)
    //   BOTTOM_RIGHT: sx >= (size - borderWidth)   OR   sy >= (size - borderWidth)
    for (int sy = 0; sy <= radius; sy++) {
      for (int sx = 0; sx <= radius; sx++) {
        int dx, dy;
        switch (quarter) {
          case TOP_LEFT:
            dx = sx;
            dy = sy;
            break;
          case TOP_RIGHT:
            dx = radius - sx;
            dy = sy;
            break;
          case BOTTOM_LEFT:
            dx = sx;
            dy = radius - sy;
            break;
          case BOTTOM_RIGHT:
            dx = radius - sx;
            dy = radius - sy;
            break;
        }
        // Test if within the inner fill circle.
        if (innerR > 0 && (dx * dx + dy * dy) <= innerR * innerR) {
          bool onFlatEdge = false;
          switch (quarter) {
            case TOP_LEFT:
              onFlatEdge = (sx < borderWidth || sy < borderWidth);
              break;
            case TOP_RIGHT:
              onFlatEdge = (sx >= (size - borderWidth) || sy < borderWidth);
              break;
            case BOTTOM_LEFT:
              onFlatEdge = (sx < borderWidth || sy >= (size - borderWidth));
              break;
            case BOTTOM_RIGHT:
              onFlatEdge = (sx >= (size - borderWidth) || sy >= (size - borderWidth));
              break;
          }
          // Only draw the fill if the pixel is not along one of the flat edges.
          if (!onFlatEdge) {
            spr.drawPixel(sx, sy, fillColor);
          }
        }
      }
    }
  
    // Push the sprite to the display at (x,y) and clean up.
    spr.pushSprite(x, y);
    spr.deleteSprite();
}
  
void drawText(int x, int y, const char* txt, uint8_t fontCode, uint16_t textColor) {
    tft.setTextColor(textColor);
    tft.setTextFont(fontCode);
    tft.setCursor(x, y);
    tft.print(txt);
}
  
void drawCenteredText(const char *message, uint8_t fontCode, uint16_t textColor, uint16_t bgColor) {
    tft.setTextColor(textColor, bgColor);
    tft.setTextFont(fontCode);
    tft.setTextDatum(MC_DATUM);          // centers the text both horizontally and vertically
    tft.drawString(message, SCREEN_WIDTH/2, SCREEN_HEIGHT/2);
}

void drawTriangleWithBorder(int x, int y, int w, int h, uint16_t fillColor, int16_t borderColor = -1, uint8_t borderWidth = 1) {
    int cx = x + w / 2;
    int top = y;
    int left = x;
    int right = x + w;
    int bottom = y + h;

    // Fill triangle if fillColor is set
    if (fillColor != -1) {
        tft.fillTriangle(cx, top, left, bottom, right, bottom, fillColor);
    }

    // Draw border if requested
    if (borderColor != -1 && borderWidth > 0) {
        for (uint8_t i = 0; i < borderWidth; i++) {
            // Offset inward to create border thickness
            tft.drawLine(cx, top + i, left + i, bottom - i, borderColor);
            tft.drawLine(left + i, bottom - i, right - i, bottom - i, borderColor);
            tft.drawLine(right - i, bottom - i, cx, top + i, borderColor);
        }
    }
}
