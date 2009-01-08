// main.c

#include <stdio.h>
#include <time.h>
#include "getopt.h"
#include "pt_ioctl.h"
#include "i2c_comm.h"
#include "data/clean_C.h"
#include "data/clean_M.h"
#include "data/clean_Y.h"
#include "data/clean_K.h"
#include "data/clean_I.h"

static void printUsage(char* argv0) {
	printf("Usage %s [options]\n\nbasic commands:\n\n"
			" -w             = print wiring information\n"
			" -p <port>      = set LPT port (1, 2 or 3)\n"
			" -i             = view chip information\n"
			" -z             = zero out page counter\n\n"
			"advanced commands (for debugging):\n\n"
			" -b <file name> = backup EEPROM\n"
			" -r <file name> = restore EEPROM\n"
			" -n             = don't auto-save backup\n"
			" -f             = force incompatible write\n"
			" -s             = scan the I2C bus\n"
			, argv0);
}

static void printWiring() {
	printf("Connecting catridge to parallel port:\n"
			"\n"
			"25-pin LPT connector at the PC:\n"
			"        -----------------------------\n"
			"     13 \\ x x x x x x x x x x x x x / 1\n"
			"      25 \\ x x x x x x x x x x x x / 14\n"
			"           -----------------------\n"
			"\n"
			"Toner cartridge connector:\n"
			" +--------------------------------------------------------+\n"
			" |       / \\                                 .            |\n"
			" |       \\ /               +---------------+            O +\n"
			" |                         | [D]  [G]  [C] |             /\n"
			" +------------------------------------------------------+\n"
			"\n"
			"        D = Data    <connect-to>  LPT pin 16\n"
			"        G = Ground  <connect-to>  LPT pin 25\n"
			"        C = Clock   <connect-to>  LPT pin 17\n"
			"\n"
			"Image transfer belt:\n"
			" |                                                         |\n"
			"++---------------------------------------------------------+\n"
			"                 |                        |\n"
			"                 |     |  DCG++++  |      |\n"
			"                 |     \\  +++++++  |      |\n"
			"                 +------------------------+\n"
			"\n"
			);
}

int int4(BYTE* buf) {
	return (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|(buf[3]);
}

int main(int argc, char** argv) {
	int i, c;
	char* readFname = NULL;
	char* writeFname = NULL;
	BYTE buf[512];
	char ready = 0;
	int port = 1;
	int rc = 1;
	int force = 0;
	int nobackup = 0;
	int zeroOut = 0;
	int scan = 0;
	time_t t;
	struct tm* tm;

	printf("SSEBR For Windows version 2.0\n"
			"Sad Samsung CLP-510 EEPROM Backup/Restore utility\n\n");
	time(&t);
	tm = localtime(&t);

	while ((c = getopt (argc, argv, "hfiwnsp:b:r:z")) > 0) {
		switch (c) {
		case 'h':
			break;
		case 'w':
			printWiring();
			return 1;
		case 'f':
			force = 1;
			break;
		case 'i':
			ready = 1;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'b':
			readFname = optarg;
			ready = 1;
			break;
		case 'r':
			writeFname = optarg;
			ready = 1;
			break;
		case 'z':
			zeroOut = 1;
			ready = 1;
			break;
		case 's':
			scan = 1;
			ready = 1;
		case 'n':
			nobackup = 1;
			break;
		case '?':
			return 1;
		default:
			fprintf(stderr, "%s: invalid arguments\n", argv[0]);
			return 1;
		}
	}
	if (!ready) {
		printUsage(argv[0]);
		return 1;
	}

	if (!timerInit(1000000L) || !timerStart())
		return 1;

	if (!OpenPortTalk())
		return 1;

	switch (port) {
	case 1:
		i2c_setBasePort(0x378);
		break;
	case 2:
		i2c_setBasePort(0x278);
		break;
	case 3:
		i2c_setBasePort(0x3bc);
		break;
	default:
		fprintf(stderr, "%s: invalid port number.\n", argv[0]);
		goto ex1;
	}

	if (scan) {
		printf("Scanning for I2C devices... press Ctrl-C to abort.\n");
		do {
			i2c_charge(100);
			for (i = 0; i < 32; i++) {
				// 0111 1100
				i2c_select_chip(i << 2);
				// check comm.
				if (i2c_wait_init(0)) {
					printf("Detected device at ID 0x%02X...\n", (i<<2));
					SleepEx(500, 0);
				}
			}
		} while (1);
		rc = 0;
		goto ex1;
	}

	// charge capacitor
	i2c_charge(250);

	printf("Accessing cartridge chip via port LPT%d\n", port);

	// detect chip
	int chipMax = 4;
	static char* chipNames[] = {"Unknown", "ImageBelt", "Yellow", "Magenta", "Cyan", "Black"};
	static char imageTypes[] = { 0, 'i', 'y', 'm', 'c', 'k'}; // stored at offset 0x28 and sometimes 0xE0
	static int     chipIDs[] = { 0,   0,   0,   1,   2,   3};
	static int   extUpdate[] = { 0,   0,   1,   1,   1,   1}; // extended update
	static BYTE* cleanData[] = { 0, clean_I, clean_Y, clean_M, clean_C, clean_K };

	int chipID;
	for (chipID = 0; chipID < chipMax; chipID++) {
		i2c_select_chip(0x20 | ((chipID & 3) << 2));
		// check comm.
		if (i2c_wait_init(1))
			break;
	}
	if (chipID >= chipMax) {
		printf("Error: no response from the chip\n");
		goto ex1;
	}

	// read chip contents
	rc = i2c_read_bytes(0, buf, sizeof(buf));
	if (rc < sizeof(buf)) {
		printf("Error reading data at offset %d\n", rc);
		goto ex1;
	}
//	BYTE doubleCapacity = buf[0x4c]; // 0=No, 1=Yes (black only)
	int pageCount = int4(buf+0x88); // stored at offset 0x88 (BE)
	char imageType = buf[0x28];

	int imageTypeID;
	for (imageTypeID = 1; imageTypeID < sizeof(imageTypes); imageTypeID++) {
		if (imageTypes[imageTypeID] == imageType)
			break;
	}
	if (imageTypeID >= sizeof(imageTypes))
		imageTypeID = 0;
	printf("Chip type: '%c' (%s)\n", imageType, chipNames[imageTypeID]);
	if (imageTypeID && chipID != chipIDs[imageTypeID])
		printf("Warning: color stored in cartridge '%c' doesn't match cartridge color\n", imageType);

	printf("Page count: %d\n", pageCount);

	if (readFname) {
		FILE* f;
		rc = 1;
		printf("Saving EEPROM to file '%s'\n", readFname);
		f = fopen(readFname, "wb");
		if (!f) {
			printf("Error writing file '%s'\n", readFname);
			goto ex1;
		}
		fwrite(buf, sizeof(buf), 1, f);
		fclose(f);
		printf("Done.\n");
		rc = 0;
	}
	if (writeFname) {
		BYTE buf2[sizeof(buf)];
		FILE* f;
		int page;
		int pageSize = 8;
		rc = 1;
		printf("Writing EEPROM from file '%s'\n", writeFname);
		f = fopen(writeFname, "rb");
		if (!f) {
			printf("Error reading file '%s'\n", writeFname);
			goto ex1;
		}
		fread(buf2, sizeof(buf2), 1, f);
		fclose(f);

		rc = 0;
		for (page = 0; page < sizeof(buf2) && !rc; page += pageSize) {
			for (i = 0; i < pageSize; i++) {
				if (buf[page + i] != buf2[page + i]) {
					if (!i2c_write_page(page + i, buf2 + page + i, pageSize - i)) {
						printf("Error writing data at offset %d\n", page + i);
						rc = 1;
					}
					break;
				}
			}
		}
		if (!rc)
			printf("Done.\n");
	}
	if (zeroOut) {
		BYTE* buf2 = cleanData[imageTypeID];
		int unknown = 0;
		rc = 1;
		if (!buf2) {
			unknown = 1;
			buf2 = clean_I;
			if (!force) {
				fprintf(stderr, "Unable to reset page counter of unknown chip\n");
				goto ex1;
			}
		}
		if (!readFname && !nobackup) {
			FILE* f;
			char backupFname[100];
			snprintf(backupFname, sizeof(backupFname), "%s_%04d-%02d-%02d_%02d-%02d-%02d.bin", chipNames[imageTypeID], tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
			printf("Saving EEPROM backup to %s\n", backupFname);
			f = fopen(backupFname, "wb");
			if (!f) {
				printf("Error writing file '%s'\n", backupFname);
				goto ex1;
			}
			if (!fwrite(buf, sizeof(buf), 1, f)) {
				printf("Error writing file '%s'\n", backupFname);
				fclose(f);
				goto ex1;
			}
			fclose(f);
		}

		printf("Zeroing out page counters\n");
		if (memcmp(buf + 0xf0, buf2 + 0xf0, 16)) {
			printf("%s: unsupported chip type\n", force ? "Warning" : "Error");
			if (!force)
				goto ex1;
		}
		if (extUpdate[imageTypeID]) {
			static int offsets[] = {0x58, 0x68, 0x78, 0x88, 0x90, 0xA0};
			static int   sizes[] = {   4,    4,    4,    4,    4,    6};

			for (i = 0; i < sizeof(offsets)/sizeof(offsets[0]); i++) {
				if (!i2c_write_page(offsets[i], buf2 + offsets[i], sizes[i])) {
					printf("Error writing data at offset %d\n", offsets[i]);
					goto ex1;
				}
			}
		} else {
			static BYTE zeros[] = { 0, 0, 0, 0 };
			if (!i2c_write_page(0x88, zeros, 4)) {
				printf("Error writing data at offset %d\n", 0x88);
				goto ex1;
			}
		}
		printf("Done.\n");
		rc = 0;
	}

ex1:
	ClosePortTalk();

	return rc;
}
