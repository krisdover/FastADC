# FastADC
`FastADC` is an interrupt driven alternative to Arduino's `analogRead()` ADC abstraction routine.
It allows ADC samples to be made on multiple channels at regular deterministic intervals, which is
basic requirement for doing any kind of digital signal processing. This kind of processing is very
difficult to achieve using `analogRead()` within Arduino's main `loop()` which tends to have less
deterministic iteration periods due to other synchronous activities taking place therein e.g. Serial I/O.

# Background
This library was borne out of a need to process a complex Receiver Signal Strength Indicator (RSSI) waveform taken from the A7105 transceiver chip of a cheap 6-channel RC receiver, in order to display RSSI information on the Micro KV OSD (aka MinimOSD) Arduino-based Atmega328p platform. The existing firmware already relied on `analogRead()` to process flight battery voltage/current readings and simple RSSI waveforms. However the A7105's complex RSSI signal needed to be preprocessed by a *negative peak hold* analog circuit which I figured could be easily implemented as a DSP routine. I soon discovered that this could not be achieve using `analogRead()` in the existing main `loop()` as the loop times were too slow and inconsistent, and the sampling needed to be done at close to the Nyquist-Shannon rate of `2 x B` - where B is the highest frequency component of the waveform. The obvious solution was to use interrupt driven ADC sampling, and this library was implemented.

## How it works
1. Each channel to be sampled is registered via the `FastADC.reference()` or `FASTADC.handle()`
API functions.

2. `FastADC` registers an `ISR` which, for each successive ADC conversion:
    1. Either:
        1. For channels registered via `reference()` - stores the ADC result in an internal data structure for later retrieval. _**Note**: The internal
        data structure only stores one result per a channel so successive results are overwritten._

        2. For channels registered via `handle()` - passes
the result to the supplied handler.

    2. Then changes the ADC channel (`MUX`) and the reference source (`REFS`) for the next conversion so that each channel is sampled in a round-robin manner.

## AVR resources used
`FastADC` requires exclusive control of the ADC module so it cannot be used alongside
`analogRead()` or `analogReference()` in the same Arduino sketch, which is expected since
it's meant to replace these.

`FastADC` uses the *Auto Trigger* ADC configuration with *Timer/Counter1 Compare Match B* as the trigger source so it also requires exclusive access to this 16-bit Timer. _**Note**: This is one of the Timers used for Arduino's phase-correct PWM output so use of Arduino's `analogWrite()` or `toneBegin()` routines with pins associated with this timer e.g. `OC1A` or `OC1B` is not compatible with this library_

Alternatively if configurable sample rates are not desired, the library can be made to use the ADC in a pseudo *Free Running mode* configuration, whereby it performs back-to-back conversions in *Single Conversion mode*, manually starting subsequent conversion at the end of the `ISR` routine. This conversion frees *Timer/Counter1* to be used for other purposes at the cost of the sample period being fixed to match the ADC conversion time (roughly 13 ADC clock cycles on the Atmega328p). _**Note**: The actual Free Running mode* could not be used because the ADC hardware starts subsequent conversions before the ISR has a chance to specify the next channel or reference for the conversion._

## Usage example
See [FastADC.ino](examples/FastADC/FastADC.ino) in the project.

## Caveats
Unlike Arduino's `analogRead()`, this library does not support specifying ADC channels using Arduino board-specific pin numbers. So any you will need to explicitly convert any such pin existing numbers to their corresponding ADC channels (as defined for the MCU) when replacing existing usages of `analogRead()` with `analog.read()` or `analog.handle()`.
