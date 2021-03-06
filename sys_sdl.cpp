
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifndef __WIN32__
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#endif

#include <SDL/SDL.h>
#include "quakedef.h"
#include "FileManager.h"

bool isDedicated;

int noconinput = 0;

CVar sys_nostdout("sys_nostdout", "0");

/**
 * Prints out to the stdout the console text.
 *
 * Removes any colour control codes for cleaner output.
 */
void Sys_Printf(const char *fmt, ...) {
	va_list argptr;
	char text[1024];
	char output[1024];

	int inputPos, outputPos, inputLen;

	va_start(argptr, fmt);
	vsnprintf(text, 1024, fmt, argptr);
	va_end(argptr);

	inputLen = strlen(text) + 1;
	outputPos = 0;
	for (inputPos = 0; inputPos < inputLen; inputPos++) {
		if (text[inputPos] == '&' && text[inputPos + 1] == 'r') {
			//skip the code
			inputPos += 2;
		}
		if (text[inputPos] == '&' && text[inputPos + 1] == 'c') {
			inputPos += 4;
		} else {
			output[outputPos++] = text[inputPos];
		}
	}

	fprintf(stdout, "%s", output);
}

void Sys_Quit(void) {
	Host_Shutdown();
	exit(0);
}

void Sys_Init(void) {
	Math_Init();
}

void Sys_Error(const char *error, ...) {
	va_list argptr;
	char string[1024];

	va_start(argptr, error);
	vsprintf(string, error, argptr);
	va_end(argptr);
	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown();
	exit(1);
}

void Sys_DebugLog(char *file, char *fmt, ...) {
	va_list argptr;
	static char data[1024];
	FILE *fp;

	va_start(argptr, fmt);
	vsprintf(data, fmt, argptr);
	va_end(argptr);
	fp = fopen(file, "a");
	fwrite(data, strlen(data), 1, fp);
	fclose(fp);
}

double Sys_FloatTime(void) {
#ifdef __WIN32__
	static int starttime = 0;

	if (!starttime)
		starttime = clock();

	return (clock() - starttime)*1.0 / 1024;
#else
	struct timeval tp;
	struct timezone tzp;
	static int secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase) {
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000000.0;
	}

	return (tp.tv_sec - secbase) +tp.tv_usec / 1000000.0;
#endif
}

#ifdef WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	return main(__argc, __argv);
}
#endif

int main(int c, char **v) {
	double time, oldtime, newtime;
	quakeparms_t parms;
	extern int vcrFile;
	extern int recording;
	int value;

	value = COM_CheckParm("-mem");
	if (value)
		parms.memsize = (int) (atof(com_argv[value + 1]) * 1024 * 1024);
	else
		parms.memsize = 16 * 1024 * 1024;

	parms.membase = malloc(parms.memsize);
	parms.basedir = ".";

	COM_InitArgv(c, v);
	parms.argc = com_argc;
	parms.argv = com_argv;

	Sys_Init();
	Host_Init(&parms);
	CVar::registerCVar(&sys_nostdout);

	oldtime = Sys_FloatTime() - 0.1;
	while (1) {
		// find time spent rendering last frame
		newtime = Sys_FloatTime();
		time = newtime - oldtime;

		if (cls.state == ca_dedicated) { // play vcrfiles at max speed
			if (time < sys_ticrate.getFloat() && (vcrFile == -1 || recording)) {
				SDL_Delay(1);
				continue; // not time to run a server only tic yet
			}
			time = sys_ticrate.getFloat();
		}

		if (time > sys_ticrate.getFloat()*2)
			oldtime = newtime;
		else
			oldtime += time;

		Host_Frame(time);
	}
}

char *Sys_ConsoleInput(void) {
	return 0;
}
