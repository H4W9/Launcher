#include "powerSave.h"
#include <globals.h>
#include <interface.h>

// Rotary encoder
#include <RotaryEncoder.h>
RotaryEncoder *encoder = nullptr;
IRAM_ATTR void checkPosition() { encoder->tick(); }

// Battery libs
#if defined(T_EMBED_1101)
// Power handler for battery detection
#include <Wire.h>
// Charger chip
#define XPOWERS_CHIP_BQ25896
#include <XPowersLib.h>
#include <esp32-hal-dac.h>
XPowersPPM PPM;
#endif

#ifdef USE_BQ27220_VIA_I2C
#define BATTERY_DESIGN_CAPACITY 1300
#include <bq27220.h>
BQ27220 bq;
#endif

/***************************************************************************************
** Function name: _setup_gpio()
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    pinMode(SEL_BTN, INPUT);
#ifdef T_EMBED_1101
    // T-Embed CC1101 has a antenna circuit optimized to each frequency band, controlled by SW0 and SW1
    // Set antenna frequency settings
    pinMode(CC1101_SW1_PIN, OUTPUT);
    pinMode(CC1101_SW0_PIN, OUTPUT);

    // Chip Select CC1101, SD and TFT to HIGH State to fix SD initialization
    pinMode(CC1101_SS_PIN, OUTPUT);
    digitalWrite(CC1101_SS_PIN, HIGH);
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(SDCARD_CS, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);
    pinMode(44, OUTPUT);
    digitalWrite(44, HIGH);

    // Power chip pin
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH); // Power on CC1101 and LED
    bool pmu_ret = false;
    Wire.begin(GROVE_SDA, GROVE_SCL);
    pmu_ret = PPM.init(Wire, GROVE_SDA, GROVE_SCL, BQ25896_SLAVE_ADDRESS);
    if (pmu_ret) {
        PPM.setSysPowerDownVoltage(3300);
        PPM.setInputCurrentLimit(3250);
        Serial.printf("getInputCurrentLimit: %d mA\n", (int)PPM.getInputCurrentLimit());
        PPM.disableCurrentLimitPin();
        PPM.setChargeTargetVoltage(4208);
        PPM.setPrechargeCurr(64);
        PPM.setChargerConstantCurr(832);
        PPM.getChargerConstantCurr();
        Serial.printf("getChargerConstantCurr: %d mA\n", (int)PPM.getChargerConstantCurr());
        PPM.enableMeasure(PowersBQ25896::CONTINUOUS);
        PPM.disableOTG();
        PPM.enableCharge();
    }
    if (bq.getDesignCap() != BATTERY_DESIGN_CAPACITY) { bq.setDesignCap(BATTERY_DESIGN_CAPACITY); }

#endif

#ifdef T_EMBED_1101
    pinMode(BK_BTN, INPUT);
#endif
    pinMode(ENCODER_KEY, INPUT);
    encoder = new RotaryEncoder(ENCODER_INA, ENCODER_INB, RotaryEncoder::LatchMode::TWO03);
    attachInterrupt(digitalPinToInterrupt(ENCODER_INA), checkPosition, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_INB), checkPosition, CHANGE);
}

/***************************************************************************************
** Function name: getBattery()
** Description:   Delivers the battery value from 1-100
***************************************************************************************/
#if defined(USE_BQ27220_VIA_I2C)
int getBattery() {
    int percent = 0;
    percent = bq.getChargePcnt();
    return (percent < 0) ? 0 : (percent >= 100) ? 100 : percent;
}
#endif
/*********************************************************************
**  Function: setBrightness
**  set brightness value
**********************************************************************/
void _setBrightness(uint8_t brightval) {
    if (brightval == 0) {
        analogWrite(TFT_BL, brightval);
    } else if (brightval > 99) {
        analogWrite(TFT_BL, 254);
    } else {
        int bl = MINBRIGHT + round(((255 - MINBRIGHT) * brightval / 100));
        analogWrite(TFT_BL, bl);
    }
}

/*********************************************************************
** Function: InputHandler
** Handles the variables PrevPress, NextPress, SelPress, AnyKeyPress and EscPress
**********************************************************************/
void InputHandler(void) {
    static unsigned long tm = millis(); // debauce for buttons
    static int posDifference = 0;
    static int lastPos = 0;
    bool sel = !BTN_ACT;
    bool esc = !BTN_ACT;

    int newPos = encoder->getPosition();
    if (newPos != lastPos) {
        posDifference += (newPos - lastPos);
        lastPos = newPos;
    }

    if (millis() - tm < 200 && !LongPress) return;

    sel = digitalRead(SEL_BTN);
#ifdef T_EMBED_1101
    esc = digitalRead(BK_BTN);
#endif

    if (posDifference != 0 || sel == BTN_ACT || esc == BTN_ACT) {
        if (!wakeUpScreen()) AnyKeyPress = true;
        else return;
    }
    if (posDifference > 0) {
        PrevPress = true;
        posDifference--;
    }
    if (posDifference < 0) {
        NextPress = true;
        posDifference++;
    }

    if (sel == BTN_ACT) {
        posDifference = 0;
        SelPress = true;
        tm = millis();
    }
    if (esc == BTN_ACT) {
        EscPress = true;
        tm = millis();
    }
}

void powerOff() {
#ifdef T_EMBED_1101
    PPM.shutdown();
#endif
}

void checkReboot() {
#ifdef T_EMBED_1101
    int countDown;
    /* Long press power off */
    if (digitalRead(BK_BTN) == BTN_ACT) {
        uint32_t time_count = millis();
        while (digitalRead(BK_BTN) == BTN_ACT) {
            // Display poweroff bar only if holding button
            if (millis() - time_count > 500) {
                tft->setTextSize(1);
                tft->setTextColor(FGCOLOR, BGCOLOR);
                countDown = (millis() - time_count) / 1000 + 1;
                if (countDown < 4)
                    tft->drawCentreString("DeepSleep in " + String(countDown) + "/3", tftWidth / 2, 12, 1);
                else {
                    tft->fillScreen(BGCOLOR);
                    while (digitalRead(BK_BTN) == BTN_ACT);
                    delay(200);
                    digitalWrite(PIN_POWER_ON, LOW);
                    esp_sleep_enable_ext0_wakeup(GPIO_NUM_6, LOW);
                    esp_deep_sleep_start();
                }
                delay(10);
            }
        }

        // Clear text after releasing the button
        delay(30);
        if (millis() - time_count > 500) tft->fillRect(tftWidth / 2 - 9 * LW, 12, 18 * LW, LH * FP, BGCOLOR);
    }
#endif
}
/***************************************************************************************
** Function name: isCharging()
** Description:   Determines if the device is charging
***************************************************************************************/
#ifdef USE_BQ27220_VIA_I2C
bool isCharging() {
    return bq.getIsCharging(); // Return the charging status from BQ27220
}
#else
bool isCharging() { return false; }
#endif
