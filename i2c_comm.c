// i2c_comm.c

#include <stdio.h>
#include "pt_ioctl.h"
#include "utimer.h"
#include "i2c_comm.h"

/*******************************************
 * 25 pin D-SUB FEMALE connector at the PC
 *    -----------------------------
 * 13 \ x x x x x x x x x x x x x / 1
 *  25 \ x x x x x x x x x x x x / 14
 *       -----------------------
 *
 *   Parallel (PC) connector
 *  --------------------------
 *  pin 1       --->  /STROBE
 *  pin 2...9   --->   D0 - D7
 *  pin 10     <---   /ACK
 *  pin 11     <---    BUSY
 *  pin 12     <---    PE
 *  pin 13     <---    SEL
 *  pin 14      --->  /AUTOFD
 *  pin 15     <---   /ERROR
 *  pin 16      --->  /INIT
 *  pin 17      --->  /SELIN
 *  pin 18...25 ---    GND
 *

 +--------------------------------------------------------+
 |       / \                                 .            |
 |       \ /               +---------------+            O +
 |                         | [D]  [G]  [C] |             /
 +------------------------------------------------------+

        D = Data   = LPT pin 16
        C = Clock  = LPT pin 17
        G = Ground = LPT pins 20-25

 ***************************************/
static int basePort = 0x378, controlPort = 0x37A;

// data bit = INIT         control 2 (0x04)
// clock bit = SELECT(inv) control 3 (0x08)

void i2c_setBasePort(int port) {
	basePort = port;
	controlPort = basePort + 2;
	outp(basePort, 0xff);
	outp(basePort+1, 0xff);
	outp(basePort+2, 0xff);
}
static int i2c_get() {
	return (inp(controlPort) >> 2) & 1;
}
static void i2c_set(BYTE clk, BYTE data) {
	outp(controlPort, (data<<2) | ((clk^1)<<3));
}
#define SHORT 1
#define NORM 2
#define I2C_WRITE 0x80
#define I2C_READ 0x81
static int chipID = 0;

void i2c_start() {
	int i;
	for (i = 10; i; i--) {
		i2c_set(0, 1);
		timerWait(SHORT);
		i2c_set(1, 1);
		timerWait(NORM);
		if (i2c_get())
			break;
	}
	if (!i)
		printf("i2c_start failed\n");
	i2c_set(1, 0);
	timerWait(NORM);
	i2c_set(0, 0);
	timerWait(SHORT);
}
void i2c_stop() {
	i2c_set(0, 0);
	timerWait(SHORT);
	i2c_set(1, 0);
	timerWait(NORM);
	i2c_set(1, 1);
	timerWait(NORM);
}
int i2c_recv_bit() {
	i2c_set(0, 1);
	timerWait(SHORT);
	i2c_set(1, 1);
	timerWait(NORM);
	int bit = i2c_get();
	i2c_set(0, 1);
	timerWait(SHORT);
	return bit;
}
void i2c_send_bit(int bit) {
	bit &= 1;
	i2c_set(0, bit);
	timerWait(SHORT);
	i2c_set(1, bit);
	timerWait(NORM);
	i2c_set(0, bit);
	timerWait(SHORT);
}
/* ack: 1 = ok, 0 = no */
int i2c_recv_ack() {
	return !i2c_recv_bit();
}
int i2c_send_byte(int b) {
	int i;
	for (i = 7; i >= 0; i--) {
		i2c_send_bit(b >> i);
	}
	return i2c_recv_ack();
}
int i2c_recv_byte(int ack) {
	int i, b = 0;
	for (i = 0; i < 8; i++) {
		b = (b << 1) | i2c_recv_bit();
	}
	if (ack && !i2c_recv_ack())
		return -1;
	return b;
}
int i2c_write_byte(int addr, int b) {
	i2c_start();
	if (!i2c_send_byte(I2C_WRITE | chipID | ((addr >> 7) & 0x0e))) {
		printf("i2c_write_byte step 1 failed\n");
		return 0;
	}
	if (!i2c_send_byte(addr & 0xff)) {
		printf("i2c_write_byte step 2 failed\n");
		return 0;
	}
	int rc = i2c_send_byte(b);
	i2c_stop();
	rc &= i2c_wait_init(1);
	return rc;
}
int i2c_read_addr() {
	i2c_start();
	if (i2c_send_byte(I2C_READ | chipID)) {
		printf("i2c_read_addr step 1 failed\n");
		return -1;
	}
	int rc = i2c_recv_byte(0);
	i2c_stop();
	return rc;
}
int i2c_read_byte(int addr) {
	i2c_start();
	if (!i2c_send_byte(I2C_WRITE | chipID | ((addr >> 7) & 0x0e))) {
		printf("i2c_read_byte step 1 failed\n");
		return -1;
	}
	if (!i2c_send_byte(addr & 0xff)) {
		printf("i2c_read_byte step 2 failed\n");
		return -1;
	}
	i2c_start();
	i2c_send_byte(I2C_READ | chipID | ((addr >> 7) & 0x0e));
	int rc = i2c_recv_byte(0);
	i2c_stop();
	return rc;
}
void i2c_select_chip(int id) {
	chipID = id;
}
int i2c_wait_init(int retry) {
	int j, rc;
	for (j = retry ? 5 : 1; j; j--) {
		i2c_start();
		rc = i2c_send_byte(I2C_WRITE | chipID);
		i2c_stop();
		if (rc)
			break;
		SleepEx(2, 0);
	}
	return rc;
}
void i2c_charge(DWORD ms) {
	i2c_set(1, 1);
	SleepEx(ms, 0);
}
int i2c_read_bytes(int addr, BYTE* b, int n) {
	i2c_start();
	int i, rc;
	if (!i2c_send_byte(I2C_WRITE | chipID | ((addr >> 7) & 0x0e))) {
		printf("i2c_read_bytes step 1 failed\n");
		return 0;
	}
	if (!i2c_send_byte(addr & 0xff)) {
		printf("i2c_read_bytes step 2 failed\n");
		return 0;
	}
	i2c_start();
	i2c_send_byte(I2C_READ | chipID | ((addr >> 7) & 0x0e));
	for (i = 0; i < n; i++) {
		if (i)
			i2c_send_bit(0);
		rc = i2c_recv_byte(0);
		if (rc < 0)
			break;
		else
			b[i] = (BYTE)rc;
	}
	i2c_stop();
	return i;
}
int i2c_write_page(int addr, BYTE* b, int n) {
	i2c_start();
	int i, rc = 0;
	if (!i2c_send_byte(I2C_WRITE | chipID | ((addr >> 7) & 0x0e))) {
		printf("i2c_write_page step 1 failed\n");
		return 0;
	}
	if (!i2c_send_byte(addr & 0xff)) {
		printf("i2c_write_page step 2 failed\n");
		return 0;
	}
	for (i = 0; i < n; i++) {
		rc = i2c_send_byte(b[i]);
		if (!rc)
			break;
	}
	i2c_stop();
	rc &= i2c_wait_init(1);
	return rc;
}
