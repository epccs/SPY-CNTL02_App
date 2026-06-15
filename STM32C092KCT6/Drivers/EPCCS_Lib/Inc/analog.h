#ifndef ANALOG_H
#define ANALOG_H

// /analog? -> {"ADC1":"<mV>",...,"ADC6":"<mV>"}, repeats every 2s until a new command arrives
void Analogf(void);

// /adc? -> {"ADC1":"<raw>",...,"ADC6":"<raw>"}, repeats every 2s until a new command arrives
void Analogd(void);

// call from the main loop whenever command_done==0, to fire the pending repeat
void AnalogRepeatCheck(void);

// call as soon as a new command line arrives, before dispatch, to cancel any pending repeat
void AnalogRepeatCancel(void);

#endif // ANALOG_H
