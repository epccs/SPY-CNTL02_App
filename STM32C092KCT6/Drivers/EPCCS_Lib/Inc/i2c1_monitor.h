#ifndef I2C1_MONITOR_H
#define I2C1_MONITOR_H

// /imon? 8..119 -> start monitoring I2C1 as a slave at the given 7-bit address.
// Output is asynchronous: {"monitor_0x..":[{"data":"0x.."},...]} is printed by
// I2c1MonitorCheck() each time the monitored address is written to.
void I2c1Monitor(void);

// call from the main loop whenever command_done==0, to print any captured monitor data
void I2c1MonitorCheck(void);

// call as soon as a new command line arrives, before dispatch, to cancel any active monitor
void I2c1MonitorCancel(void);

#endif // I2C1_MONITOR_H
