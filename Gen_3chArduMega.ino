// Sketch to control Si5351 , All 3 clocks can be controlled.Both freq and enabling of any channel.
// change step from 1 Hz to 1MHz

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include <si5351.h>
//#define ENCODER_DO_NOT_USE_INTERRUPTS
#include <Encoder.h>
#include <EEPROM.h>

// ---------------- TFT + Touch Config ----------------
MCUFRIEND_kbv tft;
#define YP A3
#define XM A2
#define YM 9
#define XP 8

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
#define MINPRESSURE 100
#define MAXPRESSURE 1000

//---------------tft colors ------------------
// Assign human-readable names to some common 16-bit color values:
#define BLACK       0x0000      /*   0,   0,   0 */
#define LIGHTGREY   0xC618      /* 192, 192, 192 */
#define GREY        0x7BEF      /* 128, 128, 128 */
#define BLUE        0x001F      /*   0,   0, 255 */
#define RED         0xF800      /* 255,   0,   0 */
#define YELLOW      0xFFE0      /* 255, 255,   0 */
#define WHITE       0xFFFF      /* 255, 255, 255 */
#define ORANGE      0xFD20      /* 255, 165,   0 */
#define GREEN       0x07E0
#define CYAN        0x07FF
#define MAGENTA     0xF81F
// EXTRA Color definitions thanks Joe Basque
#define NAVY        0x000F      /*   0,   0, 128 */
#define DARKGREEN   0x03E0      /*   0, 128,   0 */
#define DARKCYAN    0x03EF      /*   0, 128, 128 */
#define MAROON      0x7800      /* 128,   0,   0 */
#define PURPLE      0x780F      /* 128,   0, 128 */
#define OLIVE       0x7BE0      /* 128, 128,   0 */
#define DARKGREY    0x742F      /* fGEY*/
#define GREENYELLOW 0xAFE5      /* 173, 255,  47 */
#define PINK        0xF81F

//--------------UI Colour Plan ---------------
#define BkGndColour NAVY       // General back ground colour
#define FreqColour WHITE       // colour for displaying freq - touch area to select a channel for modification
#define ClkColour YELLOW       // Clock (Ckx) on left of freq - touch area for freq decrement
#define HzColour YELLOW        // Hz on right of freq - touch area for increasing the freq
#define HighlightColour GREY   // selected channel has background of this colour 
#define GenTextColour WHITE    // Text colour - non touch
#define ButtonRectColour WHITE // For drawing rectangle around button
#define StepTextColour WHITE   // Text in step size button

// ---------------- Rotary Encoder ----------------
Encoder knob(68, 67);
//Rotary r = Rotary(68, 67);
long lastPos = 0;

// ---------------- Si5351 ----------------
Si5351 si5351;
int32_t correction = 0; // calibration in ppb

// ---------------- Step Sizes ----------------
uint32_t stepSizes[] = {1, 10, 100, 1000, 10000, 100000, 1000000};
const int NUM_STEPS = 7;

// ---------------- State ----------------
struct Channel {
  bool enabled;
  uint32_t freq;
  int stepIndex;
};

Channel channels[3] = {
  {true, 1000000, 3}, // 1 MHz, step = 1 kHz
  {false, 2000000, 3}, // 2 MHz
  {false, 3000000, 3} // 3 MHz
};

int selectedChannel = 0;
bool inCalibration = false;
int prevSelectedChannel = 0;
// ---------------- EEPROM ----------------
struct Settings {
  uint32_t marker; // version marker
  int32_t correction;
  Channel channels[3];
};

#define EEPROM_ADDR 0
#define SETTINGS_MARKER 0x53514731 // "SQG1"

unsigned long lastChange = 0;
bool pendingSave = false;
bool confirmReset = false;

// ---------------- Function Prototypes ----------------
void drawMainUI();
void drawChannel(int i);
void drawStepButtons();
void drawSaveButton(int i);
void drawCalButton();
void updateChannel();
void updateOnOff();
void drawCalibrationUI();
void updateSi5351();
void applyCorrection();
void handleTouch();
void handlencoder();
void saveSettings();
void loadSettings();



void setup() {
  Serial.begin(115200);

  uint16_t ID = tft.readID();
  if (ID == 0xD3D3) ID = 0x9486;
  tft.begin(ID);
  tft.setRotation(1);
  tft.fillScreen(NAVY);

  Wire.begin();   // for si5351

  if (si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0) != true) {
    Serial.println("Si5351 not found!");
    while (1);
  }

  loadSettings();
  applyCorrection();

  for (int i = 0; i < 3; i++) {
    si5351.output_enable((si5351_clock)i, 1);
  }
  updateSi5351();
  drawMainUI();  // display all ch + steps + cal buttons
}

void loop() {
  handleTouch();
  if (!inCalibration) {
    handleEncoder();
  }
  /* if (pendingSave && millis() - lastChange > 30000) {  // save after 30 sec of change
     //    saveSettings();   // replaced ith save button
     pendingSave = false;
    }*/
}  //setup
// ---------------- Main UI ----------------
void drawMainUI() {
  tft.fillScreen(BkGndColour);
  tft.setTextSize(4);

  drawChannel(0);
  drawChannel(1);
  drawChannel(2);

  drawSaveButton(0);  // 0 for initial color Green
  // Step size buttons  for selected channel/clock
  tft.setTextSize(3);
  tft.setTextColor(GenTextColour);
  tft.setCursor(180, 225);
  tft.print("Step Hz");
  drawCalButton();
  drawStepButtons();
}

void drawChannel(int i) {
  tft.setTextSize(4);
  int y = 30 + i * 70;          //y = 30, 100, 170
  if (i == selectedChannel)   // rect= (0,20)-(380,70)
    tft.fillRect(0, y - 10, 380, 50, HighlightColour);
  else {
    tft.fillRect(0, y - 10, 380, 50, BkGndColour);
  }
  tft.setCursor(10, y);
  tft.setTextColor(ClkColour);
  tft.print("Clk");
  tft.print(i);

  tft.setTextColor(FreqColour);
  tft.setCursor(120, y);
  tft.print(channels[i].freq);
  tft.setTextColor(HzColour);
  if (channels[i].freq >= 10000000)
    tft.print("Hz");
  else
    tft.print(" Hz");

  tft.setCursor(400, y);
  tft.setTextColor(channels[i].enabled ? GREEN : RED);
  tft.print(channels[i].enabled ? " ON" : "OFF");
  drawStepButtons();
}  //drawChannel

void drawStepButtons()
{
  // Step size buttons  for selected channel/clock
  tft.setTextSize(3);
  tft.setTextColor(StepTextColour);

  int yBase = 260;

  for (int i = 0; i < NUM_STEPS; i++) {
    int x = 5 + i * 67;
    int w = 65, h = 50;
    if (i == channels[selectedChannel].stepIndex) {
      tft.fillRect(x + 2, yBase + 2, w - 2, h - 2, HighlightColour);
    } else {
      tft.fillRect(x, yBase, w, h, BkGndColour);
      tft.drawRect(x, yBase, w, h, ButtonRectColour);
    }

    if (i == 5)
      tft.setCursor(x, yBase + 15);
    else
      tft.setCursor(x + 4, yBase + 15);
    tft.setTextColor(StepTextColour);

    if (stepSizes[i] < 1000) {
      tft.print(stepSizes[i]);
    } else if (stepSizes[i] < 1000000) {
      tft.print(stepSizes[i] / 1000);
      tft.print("k");
    } else {
      tft.print("1M");
    }
  }
} // drawStepButtons

void drawSaveButton(int i) {
  tft.setTextSize(3);
  tft.setCursor(9, 225);

  if (i == 0) {  // initial cond
    tft.fillRect(5, 215, 74, 40, GREEN);
    tft.setTextColor(RED);
    tft.print("SAVE");
  }
  else {
    tft.fillRect(5, 215, 74, 40, YELLOW); // when saving
    tft.setTextColor(BLUE);
    tft.print("SAVE");
    delay(500); // it happens very fast, so some delay to confirm save, visually, there is also a message on serial port
  }
}
//-----------------------------------------
void drawCalButton() {
  // CAL button
  tft.setTextSize(3);
  tft.fillRect(400, 215, 74, 40, GREEN);
  tft.setCursor(410, 225);
  tft.setTextColor(RED);
  tft.print("CAL");
}

//-------------------
void updateChannel() {
  tft.setTextSize(4);
  int i = prevSelectedChannel;
  drawChannel(i);
  i = selectedChannel;
  drawChannel(i);
}
//-------------
void updateOnOff() {
  tft.setTextSize(4);
  for (int i = 0; i < 3; i++) {
    int y = 30 + i * 70;
    tft.setCursor(400, y);
    tft.fillRect(400, y, 90, 50, BkGndColour);
    tft.setTextColor(channels[i].enabled ? GREEN : RED);
    tft.print(channels[i].enabled ? " ON " : "OFF");
  }
}

// ---------------- Calibration UI ----------------
void drawCalibrationUI() {
  tft.fillScreen(BLACK);
  tft.setTextSize(2);
  tft.setTextColor(WHITE);

  tft.setCursor(50, 30);
  tft.print("Calibration");

  tft.setCursor(50, 80);
  tft.print("Corr: ");
  tft.print(correction);
  tft.print(" ppb");

  // Adjustment buttons
  const char* labels[] = { "-1000", "-100", "-10", "+10", "+100", "+1000" };

  for (int i = 0; i < 6; i++) {
    int x = 10 + (i % 3) * 100;
    int y = 140 + (i / 3) * 50;
    tft.drawRect(x, y, 90, 40, ButtonRectColour);
    tft.setCursor(x + 10, y + 12);
    tft.print(labels[i]);
  }

  // BACK button
  tft.fillRect(110, 260, 100, 40, RED);
  tft.setCursor(130, 270);
  tft.setTextColor(WHITE);
  tft.print("BACK");

  // RESET button
  tft.fillRect(350, 260, 100, 40, YELLOW);
  tft.setCursor(375, 275);
  tft.setTextColor(BLACK);
  tft.print("RESET");

  /*-----*/
  if (confirmReset) {
    // Confirmation popup
    tft.setTextColor(GenTextColour);
    tft.setCursor(275, 80);
    tft.print("RESET TO DEFAULT?");

    // YES button
    tft.fillRect(350, 120, 100, 40, GREEN);
    tft.setCursor(380, 132);
    tft.setTextColor(BLACK);
    tft.print("YES");

    // NO button
    tft.fillRect(350, 190, 100, 40, RED);
    tft.setCursor(380, 202);
    tft.setTextColor(WHITE);
    tft.print("NO");
    return;
  }
} //drawCalibrationUI

// ---------------- SI5351 ----------------
void updateSi5351() {
  for (int i = 0; i < 3; i++) {
    if (channels[i].enabled) {
      si5351.set_freq((uint64_t)channels[i].freq * SI5351_FREQ_MULT, (si5351_clock)i);
      si5351.output_enable((si5351_clock)i, 1);
    } else {
      si5351.output_enable((si5351_clock)i, 0);
    }
  }
 // lastChange = millis();
 // pendingSave = true;
}
//--------------------------------------------------
void applyCorrection() {
  si5351.set_correction(correction, SI5351_PLL_INPUT_XO);
  lastChange = millis();
  pendingSave = true;
}

// ---------------- Input Handling ----------------
void handleTouch() {
  TSPoint p = ts.getPoint();
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
    delay(100);
    int x = map(p.y, 940, 95, 0, 480);
    int y = map(p.x, 910, 145, 0, 320);

    Serial.print(x); Serial.print(":"); Serial.println(y);

    if (!inCalibration) {
      // Channel select touch
      for (int i = 0; i < 3; i++) {
        int yMin = 30 + (i * 70) - 10;   // ymin 20, 90, 160
        int yMax = yMin + 50 + 10;       // ymax 80, 150, 220
        if (y >= yMin && y <= yMax) {
          if (x < 100 && i == selectedChannel) { // decrease freq by step size
            int stepIdx = channels[selectedChannel].stepIndex;
            channels[selectedChannel].freq -= stepSizes[stepIdx];
            drawChannel(i); // only update affected part of display
            updateSi5351();
          }
          else if (x > 100 && x < 300) {   // touch near center to select a channel
            prevSelectedChannel = selectedChannel;
            selectedChannel = i;
            updateChannel(); // only update affected part of display
          }
          else if (x > 300 && x < 400 && i == selectedChannel) {  // increase freq by step size
            int stepIdx = channels[selectedChannel].stepIndex;
            channels[selectedChannel].freq += stepSizes[stepIdx];
            drawChannel(i);
            updateSi5351();
            // only update affected part of display
          }
          else if (x > 420) {  //ON/OFF control buton touch
            channels[i].enabled = !channels[i].enabled;
            updateSi5351();
            //drawMainUI();
            updateOnOff();
          }
        }
      }

      // Step size buttons touch
      int yBase = 260;
      if (y >= yBase && y <= yBase + 50) {
        for (int i = 0; i < NUM_STEPS; i++) {
          int xMin = 5 + i * 67;
          int xMax = xMin + 65;
          if (x >= xMin && x <= xMax) {
            channels[selectedChannel].stepIndex = i;
            lastChange = millis();
            pendingSave = true;
            drawStepButtons(); // only redraw step buttons
          }
        }
      }
      if (x > 5 && x < 100 && y > 215 && y < 255) {
        //save
        saveSettings();
      }
      // CAL button
      if (x > 400 && y > 215 && y < 255) {
        inCalibration = true;
        drawCalibrationUI();
      }
    }  //!inCalibration
    else  //in calibration
    {
      if (confirmReset) {
        // Handle confirmation screen, pressed Yes
        if (x > 350 && x < 450 && y > 120 && y < 160) {
          // YES pressed
          correction = 0;
          channels[0] = {true, 1000000, 3};
          channels[1] = {false, 2000000, 3};
          channels[2] = {false, 3000000, 3};
          applyCorrection();
          updateSi5351();
          //      saveSettings();
          confirmReset = false;
          drawCalibrationUI();
        }
      }
      else if (x > 350 && x < 450 && y > 190 && y < 230) {
        // NO pressed
        confirmReset = false;
        drawCalibrationUI();
      }

      else
        // BACK button
        if (x > 110 && x < 210 && y > 260 && y < 300) {
          inCalibration = false;
          updateSi5351();
          drawMainUI();
        }

      // RESET button
        else if (x > 350 && x < 450 && y > 260) {
          confirmReset = true;
          drawCalibrationUI();
        }
      // Calibration mode
        else if (y > 140 && y < 180) {
          if (x < 100) correction -= 1000;
          else if (x < 200) correction -= 100;
          else if (x < 300) correction -= 10;
          applyCorrection();
          drawCalibrationUI();

        }
        else if (y > 190 && y < 230) {
          if (x < 100) correction += 10;
          else if (x < 200) correction += 100;
          else if (x < 300) correction += 1000;
          applyCorrection();
          drawCalibrationUI();
        }
    }
  }
}      // handleTouch

//-----
void handleEncoder() {
  long pos = knob.read() / 2; // / 4;
  if (pos != lastPos) {
    long delta = pos - lastPos;
    int stepIdx = channels[selectedChannel].stepIndex;
    channels[selectedChannel].freq += delta * stepSizes[stepIdx];

    if (channels[selectedChannel].freq < 8) channels[selectedChannel].freq = 8;
    if (channels[selectedChannel].freq > 160000000) channels[selectedChannel].freq = 160000000;

    updateSi5351();
    //    drawMainUI();
    lastPos = pos;
    updateChannel();
  }
}

// ---------------- EEPROM ----------------
void saveSettings() {
  Settings s;
  s.marker = SETTINGS_MARKER;
  s.correction = correction;
  for (int i = 0; i < 3; i++) {
    s.channels[i] = channels[i];
  }
  drawSaveButton(1);   // yellow button when saving
  EEPROM.put(EEPROM_ADDR, s);
  Serial.println("Settings saved to EEPROM");
  drawSaveButton(0);  //Green button normal
}

void loadSettings() {
  Settings s;
  EEPROM.get(EEPROM_ADDR, s);
  if (s.marker == SETTINGS_MARKER) {
    correction = s.correction;
    for (int i = 0; i < 3; i++) {
      channels[i] = s.channels[i];
    }
    Serial.println("Settings loaded from EEPROM");
  } else {
    Serial.println("EEPROM empty, using defaults");
  }
}
