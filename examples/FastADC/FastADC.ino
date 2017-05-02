#include <FastADC.h>

FastADC(analog, 2, true /*init*/); // instantiate FastADC singleton instance and wire it into ADC ISR

#define DECAY_RATE          1  // takes 255 cycles (~56ms for 224us channel sample period) to drop 1.1V

static int16_t peak_rssi = 0;
static uint8_t raw_rssi = 0;

void negativePeakHold(uint16_t result, uint16_t sampleTimeMicros)
{
    if (result == 1023) return; // Ignore overflows i.e >= our 1.1V reference
    raw_rssi = result >> 2; // reduce to 8 bit precision to match OSD config`
    peak_rssi = max(peak_rssi - DECAY_RATE, 255 - raw_rssi); // hold negative (inverted) peak or decay to find peak
}

void setup() {
    Serial.begin(57600); // initialize serial communication at 57600 bits per second:
    // analog.init(); // optionally initialize CPU registers here if Arduino internal init() is causing trouble
    analog.reference(2, INTERNAL); // replaces Arduino `analogReference(INTERNAL);`
    analog.handle(3, INTERNAL, negativePeakHold);
}

void loop() {
    static float osd_rssi = 0;
    osd_rssi = peak_rssi * .1 + osd_rssi * .9;    // low pass filtering (smoothing)

    int sensorValue = analog.read(2); // replaces Arduino `analogRead(2);`

    // debug output
    Serial.print("ADMUX: ");
    Serial.print(ADMUX, HEX);
    Serial.print(", ADC: ");
    Serial.print(ADC);
    Serial.print(", handler count: ");
    Serial.print(analog.getDebug());
    Serial.print(", chan 2 val: ");
    Serial.print(sensorValue);
    Serial.print(", chan 3 val: ");
    Serial.println((int)osd_rssi);

    delay(1);        // delay in between reads for stability
}
