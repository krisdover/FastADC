// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Author: Kris Dover
// Email: krisdover@gmail.com
// 
// FastADC - An interrupt-driven ADC alternative for Arduino
//
#ifndef __FASTADC_H
#define __FASTADC_H

#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef NULL
const int NULL = 0;
#endif

#define ADC_CLOCK_PERIOD_MICROS     (1000000 * 128 / F_CPU) // get ADC period for 128 prescalar (8us @16Mhz CPU)

typedef void (*HANDLER)(uint16_t, uint16_t);

template<int N> class FastADC {
private:
    struct ChannelResult {
        uint8_t type_vRef_channel;
        union {
            uint16_t result;
            HANDLER handler;
        };
    };

    static const uint16_t samplePeriodMicros = ADC_CLOCK_PERIOD_MICROS * 14; // 13 ADC clock cycles per conversion plus 1 cycle gap (112us @16Mhz)
    ChannelResult perChannelResults[N];
    uint8_t numActiveChannels;
    uint16_t channelSamplePeriodMicros;
    uint32_t debug;

    FastADC(bool initialized): numActiveChannels(0), debug(0) { if (initialized) init(); }
    FastADC(const FastADC<N>& copy);
    FastADC<N>& operator=(const FastADC<N>& copy);

    int activateChannel(uint8_t channel, uint8_t vRef, bool isCallbackChannel);
    int findChannel(uint8_t channel) const;
    int findNextChannel(int afterChannelIndex) const;

    void encodeChannelAt(int i, uint8_t channel, uint8_t vRef, bool isCallbackChannel)
    {
        perChannelResults[i].type_vRef_channel = (channel & 0x0f) | (vRef<<4 & 0x30) | isCallbackChannel<<7;
    }
    uint8_t decodeChannelAt(int i) { return (perChannelResults[i].type_vRef_channel & 0x0f); }
    uint8_t decodeVRefAt(int i) { return (perChannelResults[i].type_vRef_channel>>4 & 0x03); }
    bool isCallbackChannelAt(int i) { return (perChannelResults[i].type_vRef_channel >> 7); }
public:
    static FastADC<N>& getInstance(bool initialized) {
        static FastADC<N> instance(initialized);
        return instance;
    }
    void init() const;
    int read(uint8_t channel) const;
    bool reference(uint8_t channel, uint8_t vRef);
    bool handle(uint8_t channel, uint8_t vRef, HANDLER handler);
    void handleResultThenNextChannel();
    uint32_t getDebug() { return debug; }
};

template<int N>
void FastADC<N>::init() const
{
    sei();

    ADCSRA = 0;
    ADCSRA |= _BV(ADPS0);   // set ADPS0 bit for 128 prescalar (125khz ADC clock @16Mhz)
    ADCSRA |= _BV(ADPS1);   // set ADPS1 bit for 128 prescalar
    ADCSRA |= _BV(ADPS2);   // set ADPS2 bit for 128 prescalar
    ADCSRA |= _BV(ADEN);    // enable ADC
    ADCSRA |= _BV(ADIE);    // enable ADC interrupt
    ADCSRA |= _BV(ADATE);   // enable auto-trigger
    ADCSRB |= _BV(ADTS0);   // set ADTS0 bit for auto-trigger source: Timer/Counter1 Compare Match B
    ADCSRB |= _BV(ADTS2);   // set ADTS2 bit for auto-trigger source: Timer/Counter1 Compare Match B

    OCR1B   = samplePeriodMicros * 16 / 8; // set Compare Match B register for 100us timer (@16MHz, 8 prescaler)
    TCCR1A  = 0;            // initialize TCCR1A
    TCCR1B  = 0;            // initialize TCCR1B
    TCCR1B |= _BV(WGM12);   // turn on CTC mode
    TCCR1B |= _BV(CS11);    // set CS11 bit for 8 prescaler
}

template<int N>
void FastADC<N>::handleResultThenNextChannel()
{
    debug++;
    const uint16_t result = ADC;
    const uint8_t channel = ADMUX & 0x0f;
    const int channelIndex = findChannel(channel);
    if (channelIndex >= 0) {
        if (isCallbackChannelAt(channelIndex)) {
            perChannelResults[channelIndex].handler(result, channelSamplePeriodMicros);
        } else {
            perChannelResults[channelIndex].result = result;
        }
    }
    int nextChannelIndex = findNextChannel(channelIndex);
    if (nextChannelIndex >= 0) {
        const uint8_t nextChannel = decodeChannelAt(nextChannelIndex);
        const int vRef = decodeVRefAt(nextChannelIndex);
        ADMUX = vRef<<6 | (nextChannel & 0x0f);
    }
    TIFR1 |= _BV(OCF1B); // clear Compare Match B interrupt to start next conversion
}

template<int N>
int FastADC<N>::read(const uint8_t channel) const
{
    const int channelIndex = findChannel(channel);
    if (channelIndex >= 0) {
        if (!isCallbackChannelAt(channelIndex)) return perChannelResults[channelIndex].result;
    }

    return -1; // read failed, channel not activated
}

template<int N>
bool FastADC<N>::reference(const uint8_t channel, const uint8_t vRef)
{
    const int channelIndex = findChannel(channel);
    if (channelIndex >= 0) {
        encodeChannelAt(channelIndex, channel, vRef, false);  // overwrite channel
    } else if (activateChannel(channel, vRef, false) < 0) {
        return false; // registration failed, channel not activated
    }

    return true;
}

template<int N>
bool FastADC<N>::handle(const uint8_t channel, const uint8_t vRef, const HANDLER handler)
{
    int channelIndex = findChannel(channel);
    if(channelIndex >= 0) {
        encodeChannelAt(channelIndex, channel, vRef, true);  // overwrite channel
    } else {
        channelIndex = activateChannel(channel, vRef, true);
        if (channelIndex < 0) {
            return false; // registration failed, channel not activated
        }
    }

    perChannelResults[channelIndex].handler = handler;
    return true;
}

template<int N>
int FastADC<N>::activateChannel(const uint8_t channel, const uint8_t vRef, const bool isCallbackChannel)
{
    if (numActiveChannels < N) {
        int nextActiveIndex = numActiveChannels++;
        encodeChannelAt(nextActiveIndex, channel, vRef, isCallbackChannel);
        channelSamplePeriodMicros = samplePeriodMicros * numActiveChannels;
        return nextActiveIndex;
    }

    return -1;
}

template<int N>
int FastADC<N>::findChannel(const uint8_t channel) const
{
    for (int i = 0; i < numActiveChannels; i++) {
        if (decodeChannelAt(i) == channel) {
            return i;
        }
    }
    return -1; // search failed to find channel index
}

template<int N>
int FastADC<N>::findNextChannel(const int afterChannelIndex) const
{
    if (numActiveChannels == 0) return -1; // if no active channels, indicate error

    // Note: if current active channel was not found i.e. channelIndex = -1, first channel is next
    return (afterChannelIndex + 1) % numActiveChannels;
}

#define FastADC(var, channels, initialized) \
static FastADC<channels>& var = FastADC<channels>::getInstance(initialized); \
ISR(ADC_vect) { \
    var.handleResultThenNextChannel(); \
} \
struct hack

#endif // __FASTADC_H
