# I2C1_debug

USART1 (APP_TX1/APP_RX1, PA9/PA10) connects to a THVD1406 RS485 transceiver on
the HOST485 pair, exactly as in `USART1_streaming` — see that project's
README for the idle-line DMA receive design, the `/address/command
arg1,arg2` parser (`EPCCS_Lib/parse_huart1`), and the `/id?` command. This
project reuses that infrastructure unchanged and adds commands to exercise
**I2C1** (SDA1/SCL1, PB7/PB8) — the bus between the Application and Manager
MCUs — both as a master and as a monitorable slave.

The command set and JSON shapes are modeled on
[Gravimetric's i2c0-debug](https://github.com/epccs/Gravimetric/tree/master/Applications/i2c0-debug),
and match the `I2C1_debug` project in `SPY-CNTL02_Mgr` — this is the
Application-MCU side of the same debug tool. The hardware differs slightly:
the App MCU also has SPI1 (R-Pi SPI0), USART2/3/4 (DMX1-3 and HOST485-adjacent
UARTs) and a 6-channel ADC1, all inherited unchanged from `USART1_streaming`
and not exercised by this project's commands.

## Master commands — EPCCS_Lib/i2c1_cmd

`STM32C092KCT6/Drivers/EPCCS_Lib/Src/i2c1_cmd.c` implements `/iscan?`,
`/iaddr`, `/ibuff`/`/ibuff?`, `/iwrite` and `/iread?` using the blocking HAL
master API (`HAL_I2C_IsDeviceReady`, `HAL_I2C_Master_Transmit`,
`HAL_I2C_Master_Receive`). Each command runs to completion in one call —
unlike the AVR reference, which used a `command_done` state machine to drive
the async TWI driver across multiple main-loop passes.

Module state: a 7-bit `master_address` (set by `/iaddr`) and a
`txBuffer[32]`/`txBuffer_index` shared by `/ibuff`, `/iwrite` and `/iread?`.

### /iscan?

Scan 7-bit addresses `0x08`..`0x77` and report every address that ACKs.

```
/0/iscan?
{"scan":[{"addr":"0x29"}]}
```

An empty bus returns `{"scan":[]}`.

### /iaddr 1..127

Set the 7-bit address used by `/iwrite`, `/iread?` and `/iscan?`'s siblings.
Also clears the tx buffer.

```
/0/iaddr 41
{"master_address":"0x29"}
```

### /ibuff [0..255[,...]] and /ibuff?

Append up to 5 bytes (one per argument) to the tx buffer, or with no
arguments just show its current contents. Each value must be `0..255`;
out-of-range or non-numeric arguments are rejected with
`{"err":"<cmd>Arg<N>_OutOfRng"}` / `{"err":"<cmd>Arg<N>_NaN"}`. A 33rd byte
is rejected with `{"err":"<cmd>OVF"}` and resets the buffer.

```
/0/ibuff?
{"txBuffer[0]":[]}
/0/ibuff 2,0
{"txBuffer[2]":[{"data":"0x2"},{"data":"0x0"}]}
```

### /iwrite

Write the tx buffer to `master_address` with a STOP condition. On success
the buffer is cleared:

```
/0/iwrite
{"txBuffer":"wrt_success"}
```

On failure, `{"error":"wrt_addr_nack"}` (no ACK on the address byte),
`{"error":"wrt_timeout"}`, or `{"error":"wrt_error"}` for anything else. The
tx buffer is left intact so the write can be retried.

### /iread? 1..32

Read `1..32` bytes from `master_address`.

If the tx buffer is empty, this is a plain read:

```
/0/iread? 2
{"rxBuffer":[{"data":"0x2"},{"data":"0x30"}]}
```

If the tx buffer holds bytes (e.g. an SMBus command code written with
`/ibuff`), they are written first, then the read is performed:

```
/0/ibuff 2,0
{"txBuffer[2]":[{"data":"0x2"},{"data":"0x0"}]}
/0/iread? 2
{"txBuffer":"wrt_success","rxBuffer":[{"data":"0x2"},{"data":"0x30"}]}
```

Failures during either step report `{"error":"wrt_*"}` / `{"error":"rd_*"}`
as in `/iwrite`.

**Repeated-start note**: the AVR reference issues the write without a STOP
and lets the TWI ISR generate a hardware repeated-START before the read.
This port instead does a blocking write **with** STOP followed by a separate
blocking read (a fresh START). This is simpler and works for the SMBus-style
devices this tool targets, but it is not a true repeated-start — a device
that requires the bus to stay held between the command and the read (no
intervening STOP) will not work with `/iread?`.

## Slave monitor — EPCCS_Lib/i2c1_monitor

`STM32C092KCT6/Drivers/EPCCS_Lib/Src/i2c1_monitor.c` implements `/imon?`,
which puts I2C1 into slave-listen mode at a given address so writes from
another master on the bus (e.g. the Manager MCU) can be observed.

### /imon? 8..119 (0x08..0x77)

```
/0/imon? 28
```

This command prints nothing itself. It re-programs `hi2c1.Init.OwnAddress1`
to `addr << 1` and calls `HAL_I2C_Init` + `HAL_I2C_EnableListen_IT`. From
then on, every time a master writes to that address, the bytes received are
printed asynchronously between commands:

```
{"monitor_0x1C":[{"data":"0x2"},{"data":"0x0"}]}
{"monitor_0x1C":[{"data":"0x0"},{"data":"0x0"}]}
```

If a master reads from the monitored address, the most recently received
bytes are echoed back (or a single `0x00` if nothing has been received yet).

A new `/imon?` while one is already active first cancels the old one and
retargets to the new address. Sending **any** other command line cancels the
monitor (`I2c1MonitorCancel`, called from the main loop before every
dispatch) — this includes commands addressed to other devices on the shared
RS485 bus, matching the AVR reference's "any character received" behavior.

### Implementation notes

- `HAL_I2C_AddrCallback` arms `HAL_I2C_Slave_Seq_Receive_IT` (master write)
  or `HAL_I2C_Slave_Seq_Transmit_IT` (master read) with `I2C_LAST_FRAME`.
- With `I2C_LAST_FRAME`, every transaction — whether the master transfers the
  full 32 bytes or STOPs early — ends in `HAL_I2C_ListenCpltCallback`, which
  is the single point that re-arms `HAL_I2C_EnableListen_IT` for the next
  transaction.
- A short write (the common case) also raises `HAL_I2C_ErrorCallback` with
  `HAL_I2C_ERROR_AF`; `pBuffPtr - mon_rxbuf` gives the number of bytes
  actually received so they can still be captured.
- `I2c1MonitorCheck()` (called from the main loop whenever `command_done ==
  0`) prints one `{"monitor_0x..":[...]}` line per captured write and is the
  only place output happens — so it interleaves with command responses
  rather than blocking them.
- `I2c1MonitorCancel()` clears `OAR1`'s `OA1EN` bit directly so I2C1 stops
  ACKing the monitor address immediately, even if `HAL_I2C_DisableListen_IT`
  can't be called mid-transaction.

I2C1 uses a single combined event+error interrupt (`I2C1_IRQn`), enabled in
`MX_I2C1_Init()` and serviced by `I2C1_IRQHandler` in `stm32c0xx_it.c`, which
calls both `HAL_I2C_EV_IRQHandler` and `HAL_I2C_ER_IRQHandler`.

## Verification

```bash
cd I2C1_debug
make
```

Hardware verification (scanning/talking to a real I2C1 device, and exercising
`/imon?` with the Manager MCU as a second I2C master on SDA1/SCL1) is the
next step and is not covered by this build-only check.
