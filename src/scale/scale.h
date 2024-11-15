#include "Particle.h"
#include "../nau7802/nau7802.h"

#define EEPROM_SIZE 100
#define LOCATION_CALIBRATION_FACTOR 0 // Float, requires 4 bytes of EEPROM
#define LOCATION_ZERO_OFFSET 10       // Must be more than 4 away from previous spot. int32_t, requires 4 bytes of EEPROM

#define AVG_SIZE 25

typedef struct
{
    float weight;
    int32_t raw;
} ScaleReading;

void readScale(ScaleReading *reading);
void initializeScale();
unsigned long getTimeSinceTare();
bool getIsZeroed();
int tare(String command);
int calibrate(String command);