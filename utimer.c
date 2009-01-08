// utimer.c

#include <windows.h>
#include <stdio.h>
#include "utimer.h"

static long long freqDivisor;
static LARGE_INTEGER timerLast, timerCurrent;

int timerInit(long freq) {
	timerCurrent.QuadPart = 0;
	QueryPerformanceFrequency(&timerCurrent);
	if (timerCurrent.QuadPart < freq * 2) {
		fprintf(stderr, "A high-performance timer is not available on this system.\n");
		return 0;
	}
	freqDivisor = timerCurrent.QuadPart / freq;
	return 1;
}

long long timerFreqDivisor() {
	return freqDivisor;
}

int timerStart() {
	if (!QueryPerformanceCounter(&timerCurrent)) {
		fprintf(stderr, "QueryPerformanceCounter failed\n");
		return 0;
	}
	timerLast.QuadPart = timerCurrent.QuadPart;
	return 1;
}

void timerStep() {
	do {
		QueryPerformanceCounter(&timerCurrent);
	} while (timerCurrent.QuadPart - timerLast.QuadPart < freqDivisor);
	timerLast.QuadPart = timerCurrent.QuadPart;
}

void timerWait(int ticks) {
	timerStart();
	while (ticks-- > 0)
		timerStep();
}
