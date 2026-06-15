#ifndef I2C1_CMD_H
#define I2C1_CMD_H

// shared with i2c1_monitor.c
#define I2C1_BUFFER_LENGTH 32
#define I2C1_TIMEOUT_MS 100

// /iscan? -> {"scan":[{"addr":"0x.."},...]}
void I2c1Scan(void);

// /iaddr 1..127 -> {"master_address":"0x.."}, also clears the tx buffer
void I2c1Address(void);

// /ibuff [0..255[,...]] and /ibuff? -> {"txBuffer[N]":[{"data":"0x.."},...]}
void I2c1TxBuffer(void);

// /iwrite -> {"txBuffer":"wrt_success"} or {"error":"wrt_*"}
void I2c1Write(void);

// /iread? 1..32 -> {"rxBuffer":[{"data":"0x.."},...]} or {"error":"rd_*"}
void I2c1Read(void);

#endif // I2C1_CMD_H
