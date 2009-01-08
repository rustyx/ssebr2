// i2c_comm.h
#include "pt_ioctl.h"
#include "utimer.h"

#ifdef __cplusplus
extern "C" {
#endif

void i2c_setBasePort(int port);
void i2c_select_chip(int id);
void i2c_start();
void i2c_stop();
int i2c_recv_bit();
void i2c_send_bit(int bit);
/* ack: 1 = ok, 0 = no */
int i2c_recv_ack();
int i2c_send_byte(int b);
int i2c_recv_byte(int ack);
int i2c_write_byte(int addr, int b);
int i2c_read_byte(int addr);
int i2c_wait_init(int retry);
void i2c_charge(DWORD ms);
int i2c_read_bytes(int addr, BYTE* b, int n);
int i2c_write_page(int addr, BYTE* b, int n);

#ifdef __cplusplus
}
#endif
