#include <Arduino.h>
#include "micro_ui.h"
#include <FS.h>
#include <LITTLEFS.h>
#include <SoftwareSerial.h>
#include <algorithm> // Required for std::fill()
#include <vector> // 
#include "loadcell_receiver.h"

//#define FORMAT_FLASH
#define DEBUG_COUNT 300
//#define DEBUG_FILTER
//#define DEBUG_LOCK

// ===== Callbacks =====
void sliderMeanCallback(int value) {
    Serial.print("Slider Mean value: ");
    Serial.println(value);
    config.meanSlider = value;
}

void sliderFilterCallback(int value) {
    Serial.print("Slider Filter value: ");
    Serial.println(value);
    config.filterSlider = value;
}

void sliderVibrationCallback(int value) {
    Serial.print("Slider Vibration value: ");
    Serial.println(value);
    config.vibrationSlider = value;
}

void sliderLockingCallback(int value) {
    Serial.print("Slider Lock value: ");
    Serial.println(value);
    config.lockingSlider = value;
}

void numberPadCallback(const char *label) {
    Serial.print("SliderA value: ");
    Serial.println(label);
}

void mainButtonsCallback(const char* label) {
    if (strcmp(label, "ZERO") == 0) {
        Serial.println("Zero pressed");
    } else if (strcmp(label, "Clear") == 0) {
        Serial.println("Clear pressed");
    } else if (strcmp(label, "ADD") == 0) {
        Serial.println("Add pressed");
    } else if (strcmp(label, "MENU") == 0) {
        Serial.println("MENU pressed");
        menuScreen();
    }
}

void menuButtonsCallback(const char* label) {
    if (strcmp(label, "BACK") == 0) {
        Serial.println("Back pressed");
        frontScreen();
    } else if (strcmp(label, "FILTERS") == 0) {
        Serial.println("Filters pressed");
        filterScreen();
    } else if (strcmp(label, "CALIBRATION") == 0) {
        Serial.println("Calibration pressed");
    } else if (strcmp(label, "OTHER") == 0) {
        Serial.println("Other pressed");
    }
}

void nextButtonsCallback(const char* label) {
    if (strcmp(label, ">") == 0) {
        Serial.println("> pressed");
        if(screen_num == FILTERS_SCREEN) {

            return;
        }
    }
    
    if (strcmp(label, "X") == 0) {
        Serial.println("X pressed");
        frontScreen();
        return;
    }
}

void frontScreen() {
    screen_num = FRONT_SCREEN;
    clearScreen();
    createMainButtons();
    drawAllButtons();
    weightLabel = addLabel(0, 0, "0.00", 8);
    accuLabel   = addLabel(0, 90, "0.00", 8, TFT_DARKGREY);

    drawCircleWithBorder(SCREEN_WIDTH-50, 4, 20, 2, TFT_BLACK);

    drawQuarterCircleWithBorder(SCREEN_WIDTH-50, 55, 38, 2, TFT_BLACK);
}

void menuScreen() {
    screen_num = MENU_SCREEN;
    clearScreen();
    createMenuButtons();
}

void filterScreen() {
    screen_num = FILTERS_SCREEN;
    clearScreen();
    createFilterButtons();
}

// ===== Setup and Loop =====
void setup() {
    Serial.begin(115200);
    delay(1);
	Serial.println("\n\nStarting");
    microUIInit();
    setupHC12();
    setupFS();
    frontScreen();
    updateWeight(true);
}

int i=0;
void loop() {
    microUILoopHandler();
    checkHC12();
}

void checkHC12() {
    while (hc12.available()) {
        char c = hc12.read();  
    
        if (c == '\n') {  
            hc_buf[bindex] = '\0';  
            if (bindex > 0) {  
                static float filteredValue      = 0;
                static float meanDelta          = 0;
                static float filterDelta        = 0;
                static float dampDelta          = 0;
                static float lockDelta          = 0;
                static float weightDelta        = 0;
                filteredValue = atol(hc_buf);

                const float deltaSmoothFactor = 0.1f;  // Lower = more smoothing

                // TRIMMED MEAN STAGE
                {
                    // Use a static variable to store the previous cycle's output.
                    static float lastTrimmed = filteredValue;
                    float trimmed = getTrimmedMean(filteredValue);
                    meanDelta = abs(trimmed - lastTrimmed);
                    meanDeltaSmoothed += (meanDelta - meanDeltaSmoothed) * deltaSmoothFactor;
                    lastTrimmed = trimmed;        // update stored output for next cycle
                    filteredValue = trimmed;        // pass stage output on
                }
                
                // ADVANCED FILTER STAGE
                {
                    static float lastAdvanced = filteredValue;
                    float advanced = advancedFilteredADC(filteredValue);
                    filterDelta = abs(advanced - lastAdvanced);
                    filterDeltaSmoothed += (filterDelta - filterDeltaSmoothed) * deltaSmoothFactor;
                    lastAdvanced = advanced;
                    filteredValue = advanced;
                }
                
                // VIBRATION FILTER STAGE
                {
                    static float lastVibration = filteredValue;
                    float vibration = getVibrationFilteredADC(filteredValue);
                    dampDelta = abs(vibration - lastVibration);
                    dampDeltaSmoothed += (dampDelta - dampDeltaSmoothed) * deltaSmoothFactor;
                    lastVibration = vibration;
                    filteredValue = vibration;
                }
                
                // LOCK FILTER STAGE
                {
                    static float lastLocked = filteredValue;
                    float locked = getLockedADC(filteredValue);
                    lockDelta = abs(locked - lastLocked);
                    lockDeltaSmoothed += (lockDelta - lockDeltaSmoothed) * deltaSmoothFactor;
                    lastLocked = locked;
                    filteredValue = locked;
                }
                
                // FINAL STAGE: CONVERT TO WEIGHT
                {
                    static float lastWeightOutput = filteredValue;
                    weight = ADC2Weight(filteredValue);
                    weightDelta = abs(weight - lastWeightOutput);
                    weightDeltaSmoothed += (weightDelta - weightDeltaSmoothed) * deltaSmoothFactor;
                    lastWeightOutput = weight;
                }
                                
                float scale_factor = pow(10, config.dp+1); 
                weight = round(weight * scale_factor) / scale_factor;

                updateStability();

//                Serial.println(weight,4);
                weight_counter++;
                
                if(SHOW_ADC) {
                  Serial.print("Raw ADC: ");
                  Serial.print(rawADC);
                  Serial.print(" | Smoothed ADC: ");
                  Serial.print(smoothedADC);
                  Serial.print(" | Weight: ");
                  Serial.print(weight);
                  Serial.print(" | Weight (kg): ");
                  Serial.println(weight, 2);
                }
    
                if(screen_num == FRONT_SCREEN) {
                  updateWeight();
                } else if(screen_num == FILTERS_SCREEN) {
                  updateLabel(filterWeightLabel, floatToStr(weight, config.dp));
                  updateLabel(weightDeltaLabel, floatToStr(weightDelta, config.dp+1));

                  updateLabel(meanDeltaLabel, floatToStr(meanDelta, 0));
                  updateLabel(filterDeltaLabel, floatToStr(filterDelta, 0));
                  updateLabel(lockDeltaLabel, floatToStr(lockDelta, 0));
                  updateLabel(dampDeltaLabel, floatToStr(dampDelta, 0));
                }
            } else {
    //                Serial.println("Miss");
            }
            bindex = 0;  
        } else if (bindex < sizeof(hc_buf) - 1) {  
            hc_buf[bindex++] = c;  
        } else {
    //            Serial.println("⚠️ Buffer Overflow: Resetting!");  
            bindex = 0;  
        }
    }
}

void saveCalDataDisplay() {
    if(saveCalibrationData()) {
      drawCenteredText("SAVED");
    } else {
      drawCenteredText("ERROR SAVING");
    }
  }
  
bool saveCalibrationData() {
    File file = LITTLEFS.open(CONFIG_FN, "w");
    if (!file) {
      Serial.println("Failed to open file for writing");
      return false;
    }
  
    size_t written = file.write((uint8_t *)&config, sizeof(CalibrationData));
    file.close();
    
    Serial.println("Saved Calibration bytes " + String(sizeof(CalibrationData)));
  
    return written == sizeof(CalibrationData);  // Ensure function always returns a value
}
  
bool loadCalibrationData() {
    if (!LITTLEFS.exists(CONFIG_FN)) {
      return false;
    }
  
    File file = LITTLEFS.open(CONFIG_FN, "r");
    if (!file) {
      Serial.println("Failed to open file for reading");
      return false;
    }
  
    size_t read = file.read((uint8_t *)&config, sizeof(CalibrationData));
    file.close();
  
    return read == sizeof(CalibrationData);
  }

  
void setupFS() {
#ifdef FORMAT_FLASH
  LITTLEFS.format();
  delay(100);
  Serial.println("*** FORMAT_FLASH is #defined *** Formatting complete");
#endif
  if (!LITTLEFS.begin(true)) {
    Serial.println("Failed to mount LITTLEFS");
    return;
  }
  
  Serial.println("LITTLEFS mounted successfully");
  
  if (loadCalibrationData()) {
    Serial.println("Calibration data loaded");
  } else {
    Serial.println("No calibration data found. Using defaults.");
    saveCalibrationData();
  }

  Serial.println("capacity        "	+ String(config.capacity));
  Serial.println("divisions       " + String(config.divisions));     
  Serial.println("dp              " + String(config.dp));            
  Serial.println("zero_raw        " + String(config.zero_raw));
  Serial.println("cal_kg          " + String(config.cal_kg));      
  Serial.println("cal_raw         " + String(config.cal_raw));      
  Serial.println("fso_raw         " + String(config.fso_raw));      
  Serial.println("meanSlider      " + String(config.meanSlider));
  Serial.println("filterSlider    " + String(config.filterSlider));
  Serial.println("vibrationSlider " + String(config.vibrationSlider));
  Serial.println("lockingSlider   " + String(config.lockingSlider));
}

void readHC12Response() {
    long timeout = millis() + 1000; // 1-second timeout
    while (millis() < timeout) {
        while (hc12.available()) {
            char c = hc12.read();
            Serial.write(c); // Print response to Serial Monitor
        }
    }
}
  
void setupHC12() {
    drawCenteredText("ARLC-1 Connecting");
    pinMode(HC12_SET, OUTPUT);
    digitalWrite(HC12_SET, LOW);  // Enter AT command mode before begin() !
    delay(100);
    hc12.begin(9600);
    delay(100);
    hc12.println("AT+FU3");
    readHC12Response();
    hc12.println("AT+P1");
    readHC12Response();
    hc12.println("AT+RX");
    readHC12Response();
    digitalWrite(HC12_SET, HIGH); // Enter normal mode
}

void updateWeight(bool force) {
//    Serial.println(weight, 5);
    if (!force && last_weight == weight && last_accu == accu) {
        return; // Skip update if no change
    }

    if(screen_num == FRONT_SCREEN) {
        updateLabel(weightLabel, floatToStr(weight, config.dp));
        updateLabel(accuLabel, floatToStr(accu, config.dp));
    } else if(screen_num == FILTERS_SCREEN) {
        updateLabel(filterWeightLabel, floatToStr(weight, config.dp));
        updateLabel(meanDeltaLabel, floatToStr(meanDeltaSmoothed, 0));
        updateLabel(filterDeltaLabel, floatToStr(filterDeltaSmoothed, 0));
        updateLabel(lockDeltaLabel, floatToStr(lockDeltaSmoothed, 0));
        updateLabel(dampDeltaLabel, floatToStr(dampDeltaSmoothed, 0));
        updateLabel(weightDeltaLabel, floatToStr(weightDeltaSmoothed, 0));
    }

//    Serial.println(floatToStr(weight, config.dp));

    // Update last known values
    last_weight = weight;
    last_accu   = accu;
}

long calculateFSO(long zero_raw, long cal_raw, long cal_kg, int capacity) {
    double result = (double)zero_raw + (((double)capacity * ((double)cal_raw - (double)zero_raw)) / (double)cal_kg);
    return (long)round(result);
}
  
char* floatToStr(float value, int precision) {
    static char buffer[16]; // Static buffer to store result
  
    snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
  
    // Fix -0.00 issue (Check if the string starts with "-0")
    if (buffer[0] == '-' && atof(buffer) == 0.0) {
        snprintf(buffer, sizeof(buffer), "%.*f", precision, 0.0);
    }
  
    return buffer;
}


inline float constrainFloat(float val, float minVal, float maxVal) {
    if (val < minVal) return minVal;
    else if (val > maxVal) return maxVal;
    else return val;
}

float getTrimmedMean(long filteredValue) {
    if(config.meanSlider == 0) {
        return filteredValue;
    }

    int maxSize = max(5, config.meanSlider);  // Minimum size to make trimming meaningful
    meanBuffer.push_back(filteredValue);

    // Keep buffer size in check
    if (meanBuffer.size() > maxSize)
        meanBuffer.erase(meanBuffer.begin());

    // Create a sorted copy for trimmed processing
    std::vector<float> sorted = meanBuffer;
    std::sort(sorted.begin(), sorted.end());

    // Calculate trim count (e.g., 10% on each side)
    int trimCount = sorted.size() * 10 / 100;
    trimCount = min(trimCount, (int)sorted.size() / 2);  // Avoid trimming too much

    // Compute trimmed mean
    float sum = 0;
    int count = 0;
    for (int i = trimCount; i < (int)sorted.size() - trimCount; ++i) {
        sum += sorted[i];
        count++;
    }

    return count > 0 ? sum / count : filteredValue;
}

float advancedFilteredADC(float input) {
    static float output = 0;
    static bool firstRun = true;
    static unsigned long debugCount = 0;

    // Handle filter disabled: no filtering at all.
    if (config.filterSlider == 0) {
        output = input;
        firstRun = true;
        return input;
    }

    // First-time setup: initialize output.
    if (firstRun) {
        output = input;
        firstRun = false;
        return output;
    }

    // Define working range
    float range = std::max<float>(1.0f, config.fso_raw);
    float safeMin = config.zero_raw - range * 0.5f;
    float safeMax = config.zero_raw + range * 1.5f;

    // Optional: ignore inputs way outside the expected bounds.
    if (input < safeMin || input > safeMax) {
#ifdef DEBUG_FILTER
        if (debugCount < DEBUG_COUNT) {
            Serial.printf("[ADV-FILTER %02d] INPUT OUT OF RANGE: in=%.2f, skipping\n", debugCount, input);
        }
#endif
        return output;
    }

    // --- Invert the slider mapping ---
    // Our new normalized value 'u' is defined so that u=1 when slider=1 (i.e. least filtering)
    // and u=0 when slider=100 (i.e. best filtering).
    float u = constrain((100.0f - config.filterSlider) / 99.0f, 0.0f, 1.0f);

    // Map u nonlinearly for smoothing factor and step limit.
    float alpha = u * u;  // When u=1 (slider at 1), alpha=1 (raw, no filtering). When u=0 (slider at 100), alpha=0.
    float stepLimit = range * (0.001f + pow(u, 2.5f) * 0.05f);  // Higher u gives a higher allowable delta.

    float delta = input - output;
    bool stepOverride = false;

    // If the absolute difference exceeds stepLimit, snap to the new input.
    if (fabs(delta) > stepLimit) {
        output = input;
        stepOverride = true;
    } else {
        // Otherwise, update using exponential smoothing.
        output += alpha * delta;
    }

#ifdef DEBUG_FILTER
    if (debugCount < DEBUG_COUNT) {
        Serial.printf("[ADV-FILTER %02lu] in=%.2f, out=%.2f, delta=%.2f, alpha=%.3f, stepLimit=%.1f%s\n",
                      debugCount, input, output, delta, alpha, stepLimit,
                      stepOverride ? "  [STEP OVERRIDE]" : "");
        debugCount++;
    }
#endif

    return output;
}


float getLockedADC(float filteredValue) {
    static float lockedValue = 0;       // Committed lock value
    static float visualOutput = 0;      // Smooth output
    static float pendingValue = 0;      // Candidate for locking
    static int lockCount = 0;           // Stability counter
    static unsigned long debugCount = 0;

    if (config.lockingSlider == 0) {
        lockedValue = filteredValue;
        visualOutput = filteredValue;
        pendingValue = 0;
        lockCount = 0;
        return filteredValue;
    }

    float stabilityFactor = constrain(config.lockingSlider / 100.0f, 0.01f, 1.0f);

    
    // Full slider (100) = ~3s lock time at 25Hz update (i.e. 75 samples)
    const int maxLockCount = 75; // ~3s at 25Hz
    const int lockCountNeeded = 1 + (int)(stabilityFactor * (maxLockCount - 1));

    const float flickerThreshold = (float)config.fso_raw / config.divisions * (1.0f + stabilityFactor * 1.5f);
    const float easing = 0.01f + (1.0f - stabilityFactor) * 0.25f;

    float deltaToLocked = fabs(filteredValue - lockedValue);
    float deltaToPending = fabs(filteredValue - pendingValue);

    const float unlockThreshold = flickerThreshold * 3.0f;  // Or tune this multiplier

    if (fabs(filteredValue - lockedValue) > unlockThreshold) {
        lockedValue = filteredValue;  // Or 0 if you want it to drop fast
        pendingValue = 0;
        lockCount = 0;
#ifdef DEBUG_LOCK
        if (debugCount < DEBUG_COUNT) {
            Serial.println(" → FORCE UNLOCKED due to jump");
        }
#endif
    }

#ifdef DEBUG_LOCK
    if (debugCount < DEBUG_COUNT) {
        Serial.printf("getStableValue in %.5f | deltaToLock=%.2f | threshold=%.2f | easing=%.3f | lockCountNeeded=%d\n",
                      filteredValue, deltaToLocked, flickerThreshold, easing, lockCountNeeded);
    }
#endif

    if (deltaToLocked < flickerThreshold) {
        pendingValue = 0;
        lockCount = 0;
    } else {
        if (pendingValue == 0 || deltaToPending >= flickerThreshold) {
            pendingValue = filteredValue;
            lockCount = 1;
        } else {
            lockCount++;
            if (lockCount >= lockCountNeeded) {
                lockedValue = pendingValue;
#ifdef DEBUG_LOCK
                if (debugCount < DEBUG_COUNT) {
                    Serial.println(" → LOCKED: " + String(lockedValue));
                }
#endif
                pendingValue = 0;
                lockCount = 0;
            }
        }
    }

    visualOutput = visualOutput * (1.0f - easing) + lockedValue * easing;

#ifdef DEBUG_LOCK
    if (debugCount++ < DEBUG_COUNT) {
        Serial.println(" → DISPLAYING: " + String(visualOutput));
    }
#endif

    return visualOutput;
}

float getVibrationFilteredADC(float filteredValue) {
    static float vibDisplay = 0;
    static float vibHistory[6] = {0};
    static int vibIndex = 0;

    if(config.vibrationSlider == 0) {
        return filteredValue;
    }
    // Convert slider to float (0.0 to 1.0)
    float vibrationFiltering = (float)config.vibrationSlider / 100.0f;
    if (vibrationFiltering < 0.0f) vibrationFiltering = 0.0f;
    if (vibrationFiltering > 1.0f) vibrationFiltering = 1.0f;

    // Damping control: higher damping at low slider values
    float smoothness = 1.0f - vibrationFiltering;  // 1 = max damping
    float dampingAlpha = 0.01f + (smoothness * 0.29f);  // Range: 0.01 – 0.30

    // IIR smoothing
    vibDisplay = dampingAlpha * filteredValue + (1.0f - dampingAlpha) * vibDisplay;

    // Cadence rejection using moving average
    float movingAvg = 0;
    for (int i = 0; i < 6; i++) movingAvg += vibHistory[i];
    movingAvg /= 6.0f;

    vibHistory[vibIndex++] = vibDisplay;
    if (vibIndex >= 6) vibIndex = 0;

    // If filtering is aggressive and variation is subtle, freeze at average
    if (vibrationFiltering > 0.8f && fabs(vibDisplay - movingAvg) < 0.002f) {
        vibDisplay = movingAvg;
    }

    return vibDisplay;
}

float ADC2Weight(float filteredValue) {
    float weight_kg = 0.0;
    if (config.fso_raw != config.zero_raw) { 
        weight_kg = round(((float)(filteredValue - config.zero_raw) / (float)(config.fso_raw - config.zero_raw)) * config.divisions) * ((float)config.capacity / (float)config.divisions);
    }
    return weight_kg;
}


void updateStability() {
    if(isVeryStable == isStable) return;
    isVeryStable = isStable;
    if(isStable) {
        drawCircleWithBorder(SCREEN_WIDTH-50, 4, 22, 2, TFT_RED);
    } else {
        drawCircleWithBorder(SCREEN_WIDTH-50, 4, 22, 2, TFT_BLACK);
    }

}

void createNumberPad() {
    const int screenWidth = SCREEN_WIDTH;
    const int screenHeight = SCREEN_HEIGHT;
    const int numButtonsPerRow = 5;

    // Horizontal margins and gap (in pixels)
    const int hMargin = 2;   // margin at left and right edges
    const int gap = 4;       // gap between buttons

    // Compute the available width for buttons and their width.
    int availableWidth = screenWidth - 2 * hMargin - gap * (numButtonsPerRow - 1);
    int buttonWidth = availableWidth / numButtonsPerRow;  // 300 / 5 = 60

    // Set the new button height.
    const int buttonHeight = 50;

    // Vertical margins and gap.
    const int vMargin = 2;  // bottom margin
    const int vGap = 4;     // vertical gap between rows

    // Calculate the y positions for the two rows.
    int row2_y = screenHeight - vMargin - buttonHeight;  // Lower row: 240 - 2 - 46 = 192
    int row1_y = row2_y - vGap - buttonHeight;             // Upper row: 192 - 4 - 46 = 142

    for (int i = 0; i < numButtonsPerRow; i++) {
        int x = hMargin + i * (buttonWidth + gap);
        addButton(x, row1_y, buttonWidth, buttonHeight,
            row1Labels[i],
            numberPadCallback, 4, TFT_BLUE, TFT_BLACK);
        addButton(x, row2_y, buttonWidth, buttonHeight,
            row2Labels[i],
            numberPadCallback, 4, TFT_BLUE, TFT_BLACK);
    }
}

void createNextButtons() {
    addButton(SCREEN_WIDTH-(SCREEN_WIDTH/5)-20, 0, SCREEN_WIDTH/5+20, 50, ">", nextButtonsCallback, 4, TFT_GREEN, TFT_BLACK);
    addButton(0, 0, SCREEN_WIDTH/5 + 20, 50, "X", nextButtonsCallback, 4, TFT_RED, TFT_BLACK);
}

void createMainButtons() {
    int buttonCount = 4;
    int padding = 4;
    int totalPadding = (buttonCount + 1) * padding;
    int availableWidth = SCREEN_WIDTH - totalPadding;
    int buttonWidth = availableWidth / buttonCount;
    int buttonHeight = 50;
    int y = SCREEN_HEIGHT - buttonHeight - 5;

    int x = padding;
    addButton(x, y, buttonWidth, buttonHeight, "ZERO", mainButtonsCallback, 4);

    x += buttonWidth + padding;
    addButton(x, y, buttonWidth, buttonHeight, "CLR", mainButtonsCallback, 4, TFT_DARKGREY);

    x += buttonWidth + padding;
    addButton(x, y, buttonWidth, buttonHeight, "ADD", mainButtonsCallback, 4, TFT_DARKGREY);

    x += buttonWidth + padding;
    addButton(x, y, buttonWidth, buttonHeight, "MENU", mainButtonsCallback, 4);
}

void createMenuButtons() {
    int cols = 2;
    int rows = 2;
    int padding = 4;

    int totalPaddingX = (cols + 1) * padding;
    int totalPaddingY = (rows + 1) * padding;

    int buttonWidth = (SCREEN_WIDTH - totalPaddingX) / cols;
    int buttonHeight = (SCREEN_HEIGHT - totalPaddingY) / rows;

    int x = padding;
    int y = padding;

    addButton(x, y, buttonWidth, buttonHeight, "BACK", menuButtonsCallback, 4);
    x += buttonWidth + padding;
    addButton(x, y, buttonWidth, buttonHeight, "FILTERS", menuButtonsCallback, 4);
    x = padding;
    y += buttonHeight + padding;
    addButton(x, y, buttonWidth, buttonHeight, "CALIBRATION", menuButtonsCallback, 4);
    x += buttonWidth + padding;
    addButton(x, y, buttonWidth, buttonHeight, "OTHER", menuButtonsCallback, 4);

    drawAllButtons();

}

void createFilterButtons() {
    createNextButtons();
    drawAllButtons();

    addSlider(5, 50,  SCREEN_WIDTH-76, 50, config.meanSlider,       sliderMeanCallback, TFT_GREEN, TFT_BLUE, TFT_BLACK);   
    addSlider(5, 100, SCREEN_WIDTH-76, 50, config.filterSlider,     sliderFilterCallback, TFT_GREEN, TFT_BLUE, TFT_BLACK);
    addSlider(5, 150, SCREEN_WIDTH-76, 50, config.vibrationSlider,  sliderVibrationCallback, TFT_GREEN, TFT_BLUE, TFT_BLACK);
    addSlider(5, 200, SCREEN_WIDTH-76, 50, config.lockingSlider,    sliderLockingCallback, TFT_GREEN, TFT_BLUE, TFT_BLACK);
    drawAllSliders();

    drawTriangleWithBorder(95, 30, 18, 18, 3, TFT_BLACK, TFT_YELLOW);

    drawTriangleWithBorder(SCREEN_WIDTH-66, 60 , 16, 16, 3, TFT_BLACK, TFT_YELLOW);
    drawTriangleWithBorder(SCREEN_WIDTH-66, 110, 16, 16, 3, TFT_BLACK, TFT_YELLOW);
    drawTriangleWithBorder(SCREEN_WIDTH-66, 160, 16, 16, 3, TFT_BLACK, TFT_YELLOW);
    drawTriangleWithBorder(SCREEN_WIDTH-66, 210, 16, 16, 3, TFT_BLACK, TFT_YELLOW);

    filterWeightLabel = addLabel(120,  0, "0000", 4);
    weightDeltaLabel  = addLabel(120, 28, "0000", 4);

    meanDeltaLabel    = addLabel(SCREEN_WIDTH-52, 54,  "0000", 2);
    filterDeltaLabel  = addLabel(SCREEN_WIDTH-52, 104, "0000", 2);
    dampDeltaLabel    = addLabel(SCREEN_WIDTH-52, 154, "0000", 2);
    lockDeltaLabel    = addLabel(SCREEN_WIDTH-52, 204, "0000", 2);

    drawText(SCREEN_WIDTH-46,  72, "MEAN"  , 2, TFT_GREEN);
    drawText(SCREEN_WIDTH-46, 122, "FILTER", 2, TFT_GREEN);
    drawText(SCREEN_WIDTH-46, 172, "DAMP"  , 2, TFT_GREEN);
    drawText(SCREEN_WIDTH-46, 222, "LOCK"  , 2, TFT_GREEN);
}