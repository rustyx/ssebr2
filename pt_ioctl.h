#include <windows.h>
#include <winioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

int OpenPortTalk(void);
void ClosePortTalk(void);

void outportb(unsigned short PortAddress, unsigned char byte);
unsigned char inportb(unsigned short PortAddress);

#define    inp(PortAddress)         inportb(PortAddress)
#define    outp(PortAddress, Value) outportb(PortAddress, Value)

#ifdef __cplusplus
}
#endif
