#include "Particle.h"
#include "scale/scale.h"
#include <loadcell-compensation-v2_inferencing.h>

SYSTEM_MODE(AUTOMATIC);

#define SYS_DELAY_MS 100

ScaleReading scaleReading = {0.0, 0};
char buf[128];

SerialLogHandler logHandler(LOG_LEVEL_ERROR);

/* Forward declerations ---------------------------------------------------- */
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr);
void setup();
void loop();

static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

/**
 * @brief      Copy raw feature data in out_ptr
 *             Function called by inference library
 *
 * @param[in]  offset   The offset
 * @param[in]  length   The length
 * @param      out_ptr  The out pointer
 *
 * @return     0
 */
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr)
{
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

void print_inference_result(ei_impulse_result_t result);

/**
 * @brief      Particle setup function
 */
void setup()
{
    // put your setup code here, to run once:

    // Wait for serial to make it easier to see the serial logs at startup.
    waitFor(Serial.isConnected, 15000);
    delay(2000);

    ei_printf("Edge Impulse inference runner for Particle devices\r\n");

    initializeScale();
    for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i++)
    {
        features[i] = 0.0;
    }
}

/**
 * @brief      Particle main function
 */

int idx = 0;
float knownWeightValue = 0.0;
void loop()
{
    readScale(&scaleReading);
    unsigned long timeSinceTare = getTimeSinceTare();
    features[idx] = scaleReading.weight;
    idx = (idx + 1) % EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;

    if (Serial.available() > 0)
    {
        String incomingString = Serial.readStringUntil('\n');
        incomingString.trim();
        String cmd = incomingString.substring(0, 1);
        String val = incomingString.substring(1);
        if (cmd == "t" || cmd == "T")
        {
            tare("");
        }
        else if (cmd == "c" || cmd == "C")
        {
            calibrate(val);
        }
        else if (cmd == "s" || cmd == "S") // Start the data collection and provide an expected weight
        {
            knownWeightValue = atof(val);
        }
    }

    if (sizeof(features) / sizeof(float) != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE)
    {
        ei_printf("The size of your 'features' array is not correct. Expected %d items, but had %d\n",
                  EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, sizeof(features) / sizeof(float));
        return;
    }

    ei_impulse_result_t result = {0};

    // the features are stored into flash, and we don't want to load everything into RAM
    signal_t features_signal;
    features_signal.total_length = sizeof(features) / sizeof(features[0]);
    features_signal.get_data = &raw_feature_get_data;

    // invoke the impulse
    EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);
    if (res != EI_IMPULSE_OK)
    {
        ei_printf("ERR: Failed to run classifier (%d)\n", res);
        return;
    }
    float compensated_weight = scaleReading.weight + result.classification[0].value;
    float compensated_error = knownWeightValue - compensated_weight;
    float raw_error = knownWeightValue - scaleReading.weight;
    snprintf(buf, sizeof(buf), "%ld,%ld,%f,%f,%f,%f", timeSinceTare, scaleReading.raw, scaleReading.weight, raw_error, compensated_weight, compensated_error);
    Serial.println(buf);

    delay(SYS_DELAY_MS);
}

void print_inference_result(ei_impulse_result_t result)
{
    ei_printf("Timing: DSP %d ms, inference %d ms, anomaly %d ms\r\n",
              result.timing.dsp,
              result.timing.classification,
              result.timing.anomaly);

    ei_printf("Predictions:\r\n");
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++)
    {
        ei_printf("  %s: ", ei_classifier_inferencing_categories[i]);
        ei_printf("%.5f\r\n", result.classification[i].value);
    }
}
