// utimer.h

#ifdef __cplusplus
extern "C" {
#endif

int timerInit(long freq);
long long timerFreqDivisor();
int timerStart();
void timerStep();
void timerWait(int ticks);

#ifdef __cplusplus
}
#endif
