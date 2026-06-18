#ifndef CS_IO_H
#define CS_IO_H

// CS1..CS5 current source enable outputs (CS5 also drives CS6 from the same pin).
// Output only: a HIGH enables the 22 mA current source, LOW disables it.
// /iowrt 1..5,HIGH|LOW -> JSON {"CSn":"HIGH|LOW"}
void CsWrite(void);

// /iotog 1..5 -> JSON {"CSn":"HIGH|LOW"}
void CsToggle(void);

#endif // CS_IO_H
