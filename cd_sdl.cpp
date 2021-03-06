/*
  Some of this may not work. I'm not overly familiar with SDL, I just sort
  of podged this together from the SDL headers, and the other cd-rom code.

  Mark Baker <homer1@together.net>
 */

#include <SDL/SDL.h>
#include "quakedef.h"

static bool cdValid = false;
static bool initialized = false;
static bool enabled = true;
static bool playLooping = false;
static SDL_CD *cd_id;
static float cdvolume = 1.0;

static void CD_f();

static void CDAudio_Eject() {
	if (!cd_id || !enabled)
		return;

	if (SDL_CDEject(cd_id))
		Con_DPrintf("Unable to eject CD-ROM tray.\n");
}

void CDAudio_Play(byte track, bool looping) {
	CDstatus cd_stat;

	if (!cd_id || !enabled)
		return;

	cd_stat = SDL_CDStatus(cd_id);

	if (!cdValid) {
		if (!CD_INDRIVE(cd_stat) || (!cd_id->numtracks))
			return;

		cdValid = true;
	}

	if ((track < 1) || (track >= cd_id->numtracks)) {
		Con_DPrintf("CDAudio: Bad track number: %d\n", track);
		return;
	}

	track--; /* Convert track from person to SDL value */
	if (cd_stat == CD_PLAYING) {
		if (cd_id->cur_track == track)
			return;

		CDAudio_Stop();
	}

	if (SDL_CDPlay(cd_id, cd_id->track[track].offset,
			cd_id->track[track].length)) {
		Con_DPrintf("CDAudio_Play: Unable to play track: %d\n", track + 1);
		return;
	}

	playLooping = looping;
}

void CDAudio_Stop() {
	if (!cd_id || !enabled)
		return;

	int cdstate = SDL_CDStatus(cd_id);
	if ((cdstate != CD_PLAYING) && (cdstate != CD_PAUSED))
		return;

	if (SDL_CDStop(cd_id))
		Con_DPrintf("CDAudio_Stop: Failed to stop track.\n");
}

void CDAudio_Pause() {
	if (!cd_id || !enabled)
		return;
	if (SDL_CDStatus(cd_id) != CD_PLAYING) return;

	if (SDL_CDPause(cd_id))
		Con_DPrintf("CDAudio_Pause: Failed to pause track.\n");
}

void CDAudio_Resume() {
	if (!cd_id || !enabled) return;
	if (SDL_CDStatus(cd_id) != CD_PAUSED) return;

	if (SDL_CDResume(cd_id))
		Con_DPrintf("CDAudio_Resume: Failed tp resume track.\n");
}

void CDAudio_Update() {
	if (!cd_id || !enabled)
		return;

	if (bgmvolume.getFloat() != cdvolume) {
		if (cdvolume) {
			bgmvolume.set(0.0f);
			CDAudio_Pause();
		} else {
			bgmvolume.set(1.0f);
			CDAudio_Resume();
		}
		cdvolume = bgmvolume.getFloat();
		return;
	}

	if (playLooping && (SDL_CDStatus(cd_id) != CD_PLAYING)
			&& (SDL_CDStatus(cd_id) != CD_PAUSED))
		CDAudio_Play(cd_id->cur_track + 1, true);
}

int CDAudio_Init() {
	if ((cls.state == ca_dedicated) || COM_CheckParm("-nocdaudio"))
		return -1;

	cd_id = SDL_CDOpen(0);
	if (!cd_id) {
		Con_Printf("CDAudio_Init: Unable to open default CD-ROM drive: %s\n", SDL_GetError());
		return -1;
	}

	initialized = true;
	enabled = true;
	cdValid = true;

	if (!CD_INDRIVE(SDL_CDStatus(cd_id))) {
		Con_Printf("CDAudio_Init: No CD in drive.\n");
		cdValid = false;
	}
	if (!cd_id->numtracks) {
		Con_Printf("CDAudio_Init: CD contains no audio tracks.\n");
		cdValid = false;
	}
	Cmd::addCmd("cd", CD_f);
	Con_Printf("CD Audio Initialized.\n");
	return 0;
}

void CDAudio_Shutdown() {
	if (!cd_id)
		return;

	CDAudio_Stop();
	SDL_CDClose(cd_id);
	cd_id = NULL;
}

static void CD_f() {
	if (CmdArgs::getArgCount() < 2) return;

	char *command = CmdArgs::getArg(1);
	if (!strcasecmp(command, "on")) {
		enabled = true;
	}
	if (!strcasecmp(command, "off")) {
		if (!cd_id) return;
		int cdstate = SDL_CDStatus(cd_id);
		if ((cdstate == CD_PLAYING) || (cdstate == CD_PAUSED))
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (!strcasecmp(command, "play")) {
		CDAudio_Play(atoi(CmdArgs::getArg(2)), false);
		return;
	}
	if (!strcasecmp(command, "loop")) {
		CDAudio_Play(atoi(CmdArgs::getArg(2)), true);
		return;
	}
	if (!strcasecmp(command, "stop")) {
		CDAudio_Stop();
		return;
	}
	if (!strcasecmp(command, "pause")) {
		CDAudio_Pause();
		return;
	}
	if (!strcasecmp(command, "resume")) {
		CDAudio_Resume();
		return;
	}
	if (!strcasecmp(command, "eject")) {
		CDAudio_Eject();
		return;
	}
	if (!strcasecmp(command, "info")) {
		if (!cd_id) return;
		int cdstate = SDL_CDStatus(cd_id);
		Con_Printf("%d tracks\n", cd_id->numtracks);
		if (cdstate == CD_PLAYING)
			Con_Printf("Currently %s track %d\n",
				playLooping ? "looping" : "playing",
				cd_id->cur_track + 1);
		else
			if (cdstate == CD_PAUSED)
			Con_Printf("Paused %s track %d\n",
				playLooping ? "looping" : "playing",
				cd_id->cur_track + 1);
		return;
	}
}
