#include "scale.h"

NAU7802 scale;

static unsigned long lastTareTime = 0;
static bool isZeroed = false;
static bool settingsDetected = false; // Used to prompt user to calibrate their scale

// Create an array to take average of weights. This helps smooth out jitter.
float avgWeights[AVG_SIZE];
byte avgWeightIdx = 0;

void readSystemSettings(void)
{
    float settingCalibrationFactor; // Value used to convert the load cell reading to lbs or kg
    // Look up the calibration factor
    EEPROM.get(LOCATION_CALIBRATION_FACTOR, settingCalibrationFactor);
    if (isnan(settingCalibrationFactor)) // Check if the retrieved float is NaN
    {
        Log.info("No calibration factor found, setting default");
        settingsDetected = false;
        settingCalibrationFactor = 1.0; // Default to 1.0
        EEPROM.put(LOCATION_CALIBRATION_FACTOR, settingCalibrationFactor);
    }
    Log.info("Setting calibration factor: %f", settingCalibrationFactor);
    scale.setCalibrationFactor(settingCalibrationFactor);
}

int tare(String command)
{
    scale.calculateZeroOffset(AVG_SIZE, 5000);
    delay(50);
    scale.calculateZeroOffset(AVG_SIZE, 5000);
    // scale.calibrateAFE(NAU7802_CALMOD_OFFSET);

    // Log.info("Zeroing scale: %ld", scale.getZeroOffset());
    isZeroed = true;
    lastTareTime = millis();
    for (int x = 0; x < AVG_SIZE; x++)
        avgWeights[x] = scale.getWeight();
    return 0;
}

int calibrate(String command)
{
    float weightOnScale = atof(command);
    Log.info("Calibrating with weight... %f", weightOnScale);
    scale.calculateCalibrationFactor(weightOnScale);
    EEPROM.put(LOCATION_CALIBRATION_FACTOR, scale.getCalibrationFactor());
    Log.info("Calibrated! %f", scale.getCalibrationFactor());
    return 0;
}

void readScale(ScaleReading *reading)
{
    if (scale.available() == true)
    {
        int32_t currentReading = scale.getReading();
        float currentWeight = scale.getWeight(true);

        avgWeights[avgWeightIdx++] = currentWeight;
        if (avgWeightIdx == AVG_SIZE)
            avgWeightIdx = 0;

        float avgWeight = 0;
        for (int x = 0; x < AVG_SIZE; x++)
            avgWeight += avgWeights[x];
        avgWeight /= AVG_SIZE;

        reading->raw = currentReading;
        reading->weight = avgWeight;

        // Log.info("Calibration factor: %f, Zero offset: %ld", scale.getCalibrationFactor(), scale.getZeroOffset());
        // Log.info("delta: %ld", scale.getZeroOffset() - reading->raw);
    }
}

bool getIsZeroed()
{
    return isZeroed;
}

unsigned long getTimeSinceTare()
{
    return (millis() - lastTareTime);
}

void initializeScale()
{
    if (scale.begin() == false)
    {
        Log.info("Scale not detected. Please check wiring. Freezing...");
    }
    scale.setSampleRate(NAU7802_SPS_80); // Set sample rate: 10, 20, 40, 80 or 320
    scale.setGain(NAU7802_GAIN_32);      // Gain can be set to 1, 2, 4, 8, 16, 32, 64, or 128.
    scale.setLDO(NAU7802_LDO_3V0);       // Set LDO voltage. 3.0V is the best choice for Qwiic
    scale.calibrateAFE(NAU7802_CALMOD_INTERNAL);
    EEPROM.begin();
    readSystemSettings(); // Load zeroOffset and calibrationFactor from EEPROM

    delay(500);
    // Take 10 readings to flush out readings
    for (uint8_t i = 0; i < 10; i++)
    {
        while (!scale.available())
            delay(1);
        scale.getReading();
    }
}