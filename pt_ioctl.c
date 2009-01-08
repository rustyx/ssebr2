/******************************************************************************/
/*                                                                            */
/*                          IoExample for PortTalk V2.1                       */
/*                        Version 2.1, 12th January 2002                      */
/*                          http://www.beyondlogic.org                        */
/*                                                                            */
/* Copyright © 2002 Craig Peacock. Craig.Peacock@beyondlogic.org              */
/* Any publication or distribution of this code in source form is prohibited  */
/* without prior written permission of the copyright holder. This source code */
/* is provided "as is", without any guarantee made as to its suitability or   */
/* fitness for any particular use. Permission is herby granted to modify or   */
/* enhance this sample code to produce a derivative program which may only be */
/* distributed in compiled object form only.                                  */
/******************************************************************************/

#include <stdio.h>
#include "pt_ioctl.h"
#include "porttalk_IOCTL.h"

HANDLE PortTalk_Handle; /* Handle for PortTalk Driver */
static void InstallPortTalkDriver(SC_HANDLE SchSCManager);
static int StartPortTalkDriver(void);

void outportb(unsigned short PortAddress, unsigned char byte) {
	unsigned int error;
	DWORD BytesReturned;
	unsigned char Buffer[3];
	unsigned short * pBuffer;
	pBuffer = (unsigned short *) &Buffer[0];
	*pBuffer = PortAddress;
	Buffer[2] = byte;

	error = DeviceIoControl(PortTalk_Handle, IOCTL_WRITE_PORT_UCHAR, &Buffer,
			3, NULL, 0, &BytesReturned, NULL);

	if (!error)
		fprintf(stderr, "Error occured during outportb while talking to PortTalk driver: %lx\n",
				GetLastError());
}

unsigned char inportb(unsigned short PortAddress) {
	unsigned int error;
	DWORD BytesReturned;
	unsigned char Buffer[3];
	unsigned short * pBuffer;
	pBuffer = (unsigned short *) &Buffer;
	*pBuffer = PortAddress;

	error = DeviceIoControl(PortTalk_Handle, IOCTL_READ_PORT_UCHAR, &Buffer, 2,
			&Buffer, 1, &BytesReturned, NULL);

	if (!error)
		fprintf(stderr, "Error occured during inportb while talking to PortTalk driver: %lx\n",
				GetLastError());
	return (Buffer[0]);
}

int OpenPortTalk(void) {
	/* Open PortTalk Driver. If we cannot open it, try installing and starting it */
	PortTalk_Handle = CreateFile("\\\\.\\PortTalk", GENERIC_READ, 0, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (PortTalk_Handle == INVALID_HANDLE_VALUE) {
		/* Start or Install PortTalk Driver */
		StartPortTalkDriver();
		/* Then try to open once more, before failing */
		PortTalk_Handle = CreateFile("\\\\.\\PortTalk", GENERIC_READ, 0, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (PortTalk_Handle == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "Couldn't access PortTalk Driver.\n\n");
			return 0;
		}
	}
	return 1;
}

void ClosePortTalk(void) {
	CloseHandle(PortTalk_Handle);
}

static int StartPortTalkDriver(void) {
	SC_HANDLE SchSCManager;
	SC_HANDLE schService;
	BOOL ret;
	DWORD err;
	int i;

	/* Open Handle to Service Control Manager */
	SchSCManager = OpenSCManager(NULL, /* machine (NULL == local) */
		NULL, /* database (NULL == default) */
		SC_MANAGER_ALL_ACCESS); /* access required */

	if (SchSCManager == NULL) {
		DWORD err = GetLastError();
		switch (err) {
		case ERROR_ACCESS_DENIED:
			fprintf(stderr, "Access denied. Administrator access is required to access the LPT port.\n");
			break;
		default:
			fprintf(stderr, "Unable to access service control manager. Error = %08lX\n", err);
		}
		return 0;
	}

	for (i = 0; i < 2; i++) {
		/* Open a Handle to the PortTalk Service Database */
		schService = OpenService(SchSCManager, /* handle to service control manager database */
			"PortTalk", /* pointer to name of service to start */
			SERVICE_ALL_ACCESS); /* type of access to service */

		if (!schService && !i) {
			switch (GetLastError()) {
			case ERROR_ACCESS_DENIED:
				fprintf(stderr, "Access denied. Administrator access is required to access the LPT port.\n");
				return 0;
			default:
//			case ERROR_INVALID_NAME:
//			case ERROR_SERVICE_DOES_NOT_EXIST:
//				fprintf(stderr, "PortTalk: The PortTalk driver does not exist. Installing driver.\n");
//				fprintf(stderr, "PortTalk: This can take up to 30 seconds on some machines . .\n");
				InstallPortTalkDriver(SchSCManager);
				break;
			}
		} else
			break;
	}
	if (!schService) {
		fprintf(stderr, "Unable to access PortTalk driver. Error = %08lX\n", GetLastError());
		CloseServiceHandle(SchSCManager);
		return 0;
	}

	/* Start the PortTalk Driver. Errors will occur here if PortTalk.SYS file doesn't exist */

	ret = StartService(schService, /* service identifier */
		0, /* number of arguments */
		NULL); /* pointer to arguments */

	if (!ret) {
		err = GetLastError();
		switch (err) {
		case ERROR_ACCESS_DENIED:
			fprintf(stderr, "Access denied. Administrator access is required to access the LPT port.\n");
			break;
		case ERROR_SERVICE_ALREADY_RUNNING:
			break;
		default:
			fprintf(stderr, "Unable to start PortTalk driver. Error = %08lX\n", err);
			DeleteService(schService);
			CloseServiceHandle(schService);
			CloseServiceHandle(SchSCManager);
			return 0;
		}
	}

	/* Close handle to Service Control Manager */
	CloseServiceHandle(schService);
	CloseServiceHandle(SchSCManager);
	return 1;
}

static void InstallPortTalkDriver(SC_HANDLE SchSCManager) {
	SC_HANDLE schService;
	CHAR DriverFileName[1024];

	/* Get Current Directory. Assumes PortTalk.SYS driver is in this directory.    */
	/* Doesn't detect if file exists, nor if file is on removable media - if this  */
	/* is the case then when windows next boots, the driver will fail to load and  */
	/* a error entry is made in the event viewer to reflect this */

	/* Get System Directory. This should be something like c:\windows\system32 or  */
	/* c:\winnt\system32 with a Maximum Character lenght of 20. As we have a       */
	/* buffer of 80 bytes and a string of 24 bytes to append, we can go for a max  */
	/* of 55 bytes */

	if (!GetSystemDirectory(DriverFileName, 768)) {
		fprintf(stderr, "Failed to get system directory. It may be too long or contain invalid characters.\n");
		return;
	}

	/* Append our Driver Name */
	lstrcat(DriverFileName, "\\drivers\\porttalk.sys");

	/* Copy Driver to System32/drivers directory. This fails if the file doesn't exist. */

	if (!CopyFile("porttalk.sys", DriverFileName, FALSE)) {
		fprintf(stderr, "Unable to copy porttalk.sys to %s\n", DriverFileName);
		return;
	}

	/* Create Service/Driver - This adds the appropriate registry keys in */
	/* HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services - It doesn't  */
	/* care if the driver exists, or if the path is correct.              */

	schService = CreateService(SchSCManager, /* SCManager database */
		"PortTalk", /* name of service */
		"PortTalk", /* name to display */
		SERVICE_ALL_ACCESS, /* desired access */
		SERVICE_KERNEL_DRIVER, /* service type */
		SERVICE_DEMAND_START, /* start type */
		SERVICE_ERROR_NORMAL, /* error control type */
		"system32\\drivers\\porttalk.sys", /* service's binary */
		NULL, /* no load ordering group */
		NULL, /* no tag identifier */
		NULL, /* no dependencies */
		NULL, /* LocalSystem account */
		NULL /* no password */
	);

	if (schService == NULL) {
		DWORD err = GetLastError();
		switch (err) {
		case ERROR_SERVICE_EXISTS:
			break;
		case ERROR_ACCESS_DENIED:
			fprintf(stderr, "Access denied. Administrator access is required to access the LPT port.\n");
			break;
		default:
			fprintf(stderr, "Unable to create PortTalk service. Error = %08lX\n", err);
		}
	} else {
		/* Close Handle to Service Control Manager */
		CloseServiceHandle(schService);
	}

}
