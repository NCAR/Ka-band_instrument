#ifndef _TXMOND_H_
#define _TXMOND_H_

#include <paths.h>

/* #define PID_FILE _PATH_VARRUN "txmond.pid" */
#define PID_FILE "/tmp/txmond.pid"

/* Status byte 1 */

#define UNIT_ON         0x01
#define COOLDOWN        0x02
#define WARMUP          0x04
#define STANDBY         0x08
#define HV_RUNUP        0x10
#define FAULT_SUM       0x20

/* Status byte 2 */

#define REV_POWER       0x01
#define SAFETY_INLCK    0x02
#define REMOTE_MODE     0x04
#define HVPS_ON         0x08
#define BLOWER          0x10
#define MAG_AV_CUR      0x20

/* Status byte 3 */

#define HVPS_OV         0x01
#define HVPS_UV         0x02
#define WG_PRESSURE     0x04
#define HVPS_OC         0x08
#define PIP             0x20

#define STX (0x02)
#define ETX (0x03)

/* Txmon commands */
enum {
	CMD_INVALID,
	CMD_POWERON,
	CMD_POWEROFF,
	CMD_STANDBY,
	CMD_OPERATE,
	CMD_RESET,
	CMD_STATUS
};

const char TXMON_CMD_FIFO[16] = "/tmp/txmon_cmd";
const char TXMON_INFO_FIFO[16] = "/tmp/txmon_info";

#endif
