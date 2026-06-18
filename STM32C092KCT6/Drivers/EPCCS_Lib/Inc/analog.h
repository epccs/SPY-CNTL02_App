#ifndef ANALOG_H
#define ANALOG_H

// start ADC1 free-running with DMA into the background buffer; call once at
// boot, after HAL_ADCEx_Calibration_Start(&hadc1)
void AnalogInit(void);

// /analog? [1..6[,...]] -> {"ADC1":"<mV>",...}, only the given channels (all six
// if no args), repeats every 2s until a new command arrives
void Analogf(void);

// /adc? [1..6[,...]] -> {"ADC1":"<raw>",...}, only the given channels (all six
// if no args), repeats every 2s until a new command arrives
void Analogd(void);

// call from the main loop whenever command_done==0, to fire the pending repeat
void AnalogRepeatCheck(void);

// call as soon as a new command line arrives, before dispatch, to cancel any pending repeat
void AnalogRepeatCancel(void);

#endif // ANALOG_H
