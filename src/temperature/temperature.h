// TMP112 I2C address is 0x48(72)
#define Addr 0x48

typedef struct
{
    float degreesF;
    float degreesC;
} TemperatureReading;

void readTemperature(TemperatureReading *reading);
void initializeTemperature();