using fs::File;

#define SHOW_ADC false
#define HIDE_WIFI true

#define CONFIG_FN "/calibration.dat"

#define HC12_RX     16
#define HC12_TX     17
#define HC12_BAUD   9600
#define HC12_SET    4

SoftwareSerial hc12(HC12_RX, HC12_TX);  // RX_ESP32, TX_ESP32

String cal_entry    = "";
float weight        = 0;
float accu      = 0;
float last_weight        = 0;
float last_accu      = 0;
int   screen_num    = -1;
long  weight_counter = 0;
long  last_weight_counter = 0;

long rawADC = 0;
float meanADC = 0;
float smoothedADC = 0;
float filteredADC = 0;
float lockedADC = 0;
float dampedADC = 0;
std::vector<float> meanBuffer;

float meanDeltaSmoothed   = 0;
float filterDeltaSmoothed = 0;
float lockDeltaSmoothed   = 0;
float dampDeltaSmoothed   = 0;
float weightDeltaSmoothed = 0;

bool isStable = false;
bool isVeryStable = false;

char hc_buf[20];  // Buffer for incoming number
uint8_t bindex = 0;  // Buffer index

char out_buf[64];
char in_buf[64];
int  in_pos = 0;
char cal_buf[64];
int  cal_pos = 0;

struct CalibrationData {
    int capacity;        // capacity (kg)
    int divisions;       // Number of discrete steps
    int dp;              // 0 1 2 3
    long zero_raw;       // gathered from getZero()
    long cal_kg;         // entered from doCalibration()
    long cal_raw;        // gathered from getCalibration()
    long fso_raw;        // from calculateFSO()
    int meanSlider;      // getTrimmedMean()
    int filterSlider;    // advancedFilteredADC()
    int vibrationSlider; // getVibrationFilteredADC()
    int lockingSlider;   // getStableADC()
};
  
CalibrationData config = {
    .capacity         = 1,        // 1kg load cell
    .divisions        = 3000,     // Targeting high precision
    .dp               = 2,        // 0.01 resolution
    .zero_raw         = 9590,
    .cal_kg           = 1,
    .cal_raw          = 1088000,
    .fso_raw          = 1088000,
    .meanSlider       = 0,
    .filterSlider     = 70,
    .vibrationSlider  = 0,
    .lockingSlider    = 50
};
  
// int stepThreshold;    // 2 (sensitive)       to 20 (less reactive)
// float alpha;          // 0.05 (slow filter)  to 0.5 (fast filter)
// int maxBuffer;

void setupHC12();
void setupFS();
void updateStability();
void checkHC12();
bool saveCalibrationData();
bool loadCalibrationData();

long calculateFSO(long zero_raw, long cal_raw, long cal_kg, int capacity);
void updateWeight(bool force = true);

void readHC12Response();
char* floatToStr(float value, int precision = 2);

float getTrimmedMean(long);
float advancedFilteredADC(float);
float getVibrationFilteredADC(float);
float getLockedADC(float);
float ADC2Weight(float);

LabelHandle weightLabel, 
            accuLabel,
            filterWeightLabel,
            meanDeltaLabel,
            filterDeltaLabel,
            lockDeltaLabel,
            dampDeltaLabel,
            weightDeltaLabel;
  
char row1Labels[5][2] = { "1", "2", "3", "4", "5" };
char row2Labels[5][2] = { "6", "7", "8", "9", "0" };

void createNumberPpad();
void createNextButtons();
void createMainButtons();
void createMenuButtons();
void createFilterButtons();
void frontScreen();
void menuScreen();
void filterScreen();
void calScreen1();

enum Screens {
  FRONT_SCREEN,
  MENU_SCREEN,
  FILTERS_SCREEN,
  CAL_SCREEN1,
  CAL_SCREEN2,
  CAL_SCREEN3,
  CAL_SCREEN4,
  CAL_SCREEN5
};