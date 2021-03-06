/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */
#include "quakedef.h"
#include "FileManager.h"

// 01-22-2000 FrikaC Begin PASTE
#ifdef _WIN32
#include <windows.h>
#endif
// 01-22-2000 FrikaC End PASTE

#define		MAXCMDLINE	256
char key_lines[32][MAXCMDLINE];
int key_linepos;
int shift_down = false;
int key_lastpress;

int edit_line = 0;
int history_line = 0;

keydest_t key_dest;

int key_count; // incremented every key event

char *keybindings[256];
bool consolekeys[256]; // if true, can't be rebound while in console
bool menubound[256];   // if true, can't be rebound while in menu
int keyshift[256];     // key to map to if shift held down in console
int key_repeats[256];  // if > 1, it is autorepeating
bool keydown[256];

typedef struct {
	const char *name;
	int keynum;
} keyname_t;

keyname_t keynames[] ={
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},
	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},
	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},
	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},
	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},
	{"MOUSE6", K_MOUSE6},
	{"MOUSE7", K_MOUSE7},
	{"MOUSE8", K_MOUSE8},
	{"MOUSE9", K_MOUSE9},
	{"MOUSE10", K_MOUSE10},
	{"MOUSE11", K_MOUSE11},
	{"MOUSE12", K_MOUSE12},
	{"MOUSE13", K_MOUSE13},
	{"MOUSE14", K_MOUSE14},
	{"MOUSE15", K_MOUSE15},
	{"MOUSE16", K_MOUSE16},
	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},
	{"JOY5", K_JOY5},
	{"JOY6", K_JOY6},
	{"JOY7", K_JOY7},
	{"JOY8", K_JOY8},
	{"JOY9", K_JOY9},
	{"JOY10", K_JOY10},
	{"JOY11", K_JOY11},
	{"JOY12", K_JOY12},
	{"JOY13", K_JOY13},
	{"JOY14", K_JOY14},
	{"JOY15", K_JOY15},
	{"JOY16", K_JOY16},
	{"JOY17", K_JOY17},
	{"JOY18", K_JOY18},
	{"JOY19", K_JOY19},
	{"JOY20", K_JOY20},
	{"JOY21", K_JOY21},
	{"JOY22", K_JOY22},
	{"JOY23", K_JOY23},
	{"JOY24", K_JOY24},
	{"JOY25", K_JOY25},
	{"JOY26", K_JOY26},
	{"JOY27", K_JOY27},
	{"JOY28", K_JOY28},
	{"JOY29", K_JOY29},
	{"JOY30", K_JOY30},
	{"JOY31", K_JOY31},
	{"JOY32", K_JOY32},
	{"PAUSE", K_PAUSE},
	{"SEMICOLON", ';'}, // because a raw semicolon seperates commands
	{NULL, 0}
};

/*
==============================================================================
			LINE TYPING INTO THE CONSOLE
==============================================================================
 */

int repeatkey = 0;

/**
 * Interactive line editing and console scrollback
 */
void Key_Console(int key) {
	const char *cmd;
	// 01-22-2000 FrikaC Begin PASTE
#ifdef _WIN32
	//char	*s;
	int i;
	HANDLE th;
	char *clipText, *textCopied;
#endif
	// 01-22-2000 FrikaC End PASTE

	if (key == K_ENTER) {
		Cbuf_AddText(key_lines[edit_line] + 1); // skip the >
		Cbuf_AddText("\n");
		Con_Printf("%s\n", key_lines[edit_line]);
		edit_line = (edit_line + 1) & 31;
		history_line = edit_line;
		key_lines[edit_line][0] = ']';
		key_linepos = 1;
		if (cls.state == ca_disconnected)
			SCR_UpdateScreen(); // force an update, because the command
		// may take some time
		return;
	}

	if (key == K_TAB) { // command completion
		cmd = Cmd::completeCommand(key_lines[edit_line] + 1);
		if (!cmd)
			cmd = CVar::completeVariable(key_lines[edit_line] + 1);
		if (cmd) {
			strcpy(key_lines[edit_line] + 1, cmd);
			key_linepos = strlen(cmd) + 1;
			key_lines[edit_line][key_linepos] = ' ';
			key_linepos++;
			key_lines[edit_line][key_linepos] = 0;
			return;
		}
	}

	if (key == K_BACKSPACE || key == K_LEFTARROW) {
		if (key_linepos > 1)
			key_linepos--;
		return;
	}

	if (key == K_UPARROW) {
		do {
			history_line = (history_line - 1) & 31;
		} while (history_line != edit_line
				&& !key_lines[history_line][1]);
		if (history_line == edit_line)
			history_line = (edit_line + 1)&31;
		strcpy(key_lines[edit_line], key_lines[history_line]);
		key_linepos = strlen(key_lines[edit_line]);
		return;
	}

	if (key == K_DOWNARROW) {
		if (history_line == edit_line) return;
		do {
			history_line = (history_line + 1) & 31;
		}		while (history_line != edit_line
				&& !key_lines[history_line][1]);
		if (history_line == edit_line) {
			key_lines[edit_line][0] = ']';
			key_linepos = 1;
		} else {
			strcpy(key_lines[edit_line], key_lines[history_line]);
			key_linepos = strlen(key_lines[edit_line]);
		}
		return;
	}

	if (key == K_PGUP || key == K_MWHEELUP) {
		con_backscroll += 4;
		if ((unsigned) con_backscroll > con_totallines - (vid.height >> 3) - 1)
			con_backscroll = con_totallines - (vid.height >> 3) - 1;
		return;
	}

	if (key == K_PGDN || key == K_MWHEELDOWN) {
		con_backscroll -= 4;
		if (con_backscroll < 0)
			con_backscroll = 0;
		return;
	}

	if (key == K_HOME) {
		con_backscroll = con_totallines - (vid.height >> 3) - 1;
		return;
	}

	if (key == K_END) {
		con_backscroll = 0;
		return;
	}

	// 01-22-2000 FrikaC Begin PASTE
#ifdef FALSE_WIN32
	if ((key == 'V' || key == 'v') && GetKeyState(VK_CONTROL) < 0) {
		if (OpenClipboard(NULL)) {
			th = GetClipboardData(CF_TEXT);
			if (th) {
				clipText = GlobalLock(th);
				if (clipText) {
					textCopied = malloc(GlobalSize(th) + 1);
					strcpy(textCopied, clipText);
					/* Substitutes a NULL for every token */strtok(textCopied, "\n\r\b");
					i = strlen(textCopied);
					if (i + key_linepos >= MAXCMDLINE)
						i = MAXCMDLINE - key_linepos;
					if (i > 0) {
						textCopied[i] = 0;
						strcat(key_lines[edit_line], textCopied);
						key_linepos += i;
						;
					}
					free(textCopied);
				}
				GlobalUnlock(th);
			}
			CloseClipboard();
			return;
		}
	}
#endif
	// 01-22-2000 FrikaC End PASTE

	if (key < 32 || key > 127)
		return; // non printable

	if (key_linepos < MAXCMDLINE - 1) {
		key_lines[edit_line][key_linepos] = key;
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
	}

}

//============================================================================

char chat_buffer[32];
bool team_message = false;

void Key_Message(int key) {
	static int chat_bufferlen = 0;

	if (key == K_ENTER) {
		if (team_message)
			Cbuf_AddText("say_team \"");
		else
			Cbuf_AddText("say \"");
		Cbuf_AddText(chat_buffer);
		Cbuf_AddText("\"\n");

		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key == K_ESCAPE) {
		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key < 32 || key > 127)
		return; // non printable

	if (key == K_BACKSPACE) {
		if (chat_bufferlen) {
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if (chat_bufferlen == 31)
		return; // all full

	chat_buffer[chat_bufferlen++] = key;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================

/**
 * Returns a key number to be used to index keybindings[] by looking at the
 * given string.  Single ascii characters return themselves, while the K_*
 *  names are matched up.
 */
int Key_StringToKeynum(char *str) {
	if (!str || !str[0])
		return -1;
	if (!str[1])
		return str[0];

	for (keyname_t *kn = keynames; kn->name; kn++) {
		if (!strcasecmp(str, kn->name))
			return kn->keynum;
	}
	return -1;
}

/**
 * Returns a string (either a single ascii char, or a K_* name) for the given
 *  keynum.
 * FIXME: handle quote special (general escape sequence?)
 */
const char *Key_KeynumToString(int keynum) {
	static char tinystr[2];

	if (keynum == -1)
		return "<KEY NOT FOUND>";
	if (keynum > 32 && keynum < 127) { // printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for (keyname_t *kn = keynames; kn->name; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}

void Key_SetBinding(int keynum, const char *binding) {
	if (keynum == -1)
		return;

	// free old bindings
	if (keybindings[keynum]) {
		MemoryObj::ZFree(keybindings[keynum]);
		keybindings[keynum] = NULL;
	}

	// allocate memory for new binding
	int len = strlen(binding);
	char *newKeyBinding = (char *) MemoryObj::ZAlloc(len + 1);
	strcpy(newKeyBinding, binding);
	keybindings[keynum] = newKeyBinding;
}

void Key_Unbind_f(void) {
	if (CmdArgs::getArgCount() != 2) {
		Con_Printf("unbind <key> : remove commands from a key\n");
		return;
	}

	int key = Key_StringToKeynum(CmdArgs::getArg(1));
	if (key == -1) {
		Con_Printf("\"%s\" isn't a valid key\n", CmdArgs::getArg(1));
		return;
	}

	Key_SetBinding(key, "");
}

void Key_Unbindall_f(void) {
	for (int i = 0; i < 256; i++)
		if (keybindings[i])
			Key_SetBinding(i, "");
}

void Key_Bind_f(void) {
	char cmd[1024];

	int count = CmdArgs::getArgCount();

	if (count != 2 && count != 3) {
		Con_Printf("bind <key> [command] : attach a command to a key\n");
		return;
	}
	int key = Key_StringToKeynum(CmdArgs::getArg(1));
	if (key == -1) {
		Con_Printf("\"%s\" isn't a valid key\n", CmdArgs::getArg(1));
		return;
	}

	if (count == 2) {
		if (keybindings[key])
			Con_Printf("\"%s\" = \"%s\"\n", CmdArgs::getArg(1), keybindings[key]);
		else
			Con_Printf("\"%s\" is not bound\n", CmdArgs::getArg(1));
		return;
	}

	// copy the rest of the command line
	cmd[0] = 0; // start out with a null string
	for (int i = 2; i < count; i++) {
		if (i > 2)
			strcat(cmd, " ");
		strcat(cmd, CmdArgs::getArg(i));
	}

	Key_SetBinding(key, cmd);
}

/**
 * Writes lines containing "bind key value"
 */
void Key_WriteBindings(int handle) {
	for (int i = 0; i < 256; i++) {
		if (keybindings[i]) {
			if (*keybindings[i]) {
				char *temp = va("bind \"%s\" \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
				SystemFileManager::FileWrite(handle, temp, strlen(temp));
			}
		}
	}
}

void Key_Init(void) {
	for (int i = 0; i < 32; i++) {
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	// init ascii characters in console mode
	for (int i = 32; i < 128; i++)
		consolekeys[i] = true;
	consolekeys[K_ENTER] = true;
	consolekeys[K_TAB] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_SHIFT] = true;
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;
	consolekeys['`'] = false;
	consolekeys['~'] = false;

	for (int i = 0; i < 256; i++)
		keyshift[i] = i;
	for (int i = 'a'; i <= 'z'; i++)
		keyshift[i] = i - 'a' + 'A';
	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';

	menubound[K_ESCAPE] = true;
	for (int i = 0; i < 12; i++)
		menubound[K_F1 + i] = true;

	// register our functions
	Cmd::addCmd("bind", Key_Bind_f);
	Cmd::addCmd("unbind", Key_Unbind_f);
	Cmd::addCmd("unbindall", Key_Unbindall_f);
}

/**
 * Called by the system between frames for both key up and key down events
 * Should NOT be called during an interrupt!
 */
void Key_Event(int key, bool down) {
	char *kb;
	char cmd[1024];

	keydown[key] = down;

	if (!down)
		key_repeats[key] = 0;

	key_lastpress = key;
	key_count++;
	if (key_count <= 0) {
		return; // just catching keys for Con_NotifyBox
	}

	// update auto-repeat status
	if (down) {
		key_repeats[key]++;
		if (key != K_BACKSPACE && key != K_PAUSE && key_repeats[key] > 1) {
			return; // ignore most autorepeats
		}

		if (key >= 200 && !keybindings[key])
			Con_Printf("%s is unbound, hit F4 to set.\n", Key_KeynumToString(key));
	}

	if (key == K_SHIFT)
		shift_down = down;

	// handle escape specialy, so the user can never unbind it
	if (key == K_ESCAPE) {
		if (!down)
			return;
		switch (key_dest) {
			case key_message:
				Key_Message(key);
				break;
			case key_menu:
				M_Keydown(key);
				break;
			case key_game:
			case key_console:
				M_ToggleMenu_f();
				break;
			default:
				Sys_Error("Bad key_dest");
		}
		return;
	}

	// key up events only generate commands if the game key binding is
	// a button command (leading + sign).  These will occur even in console mode,
	// to keep the character from continuing an action started before a console
	// switch.  Button commands include the kenum as a parameter, so multiple
	// downs can be matched with ups
	if (!down) {
		kb = keybindings[key];
		if (kb && kb[0] == '+') {
			sprintf(cmd, "-%s %i\n", kb + 1, key);
			Cbuf_AddText(cmd);
		}
		if (keyshift[key] != key) {
			kb = keybindings[keyshift[key]];
			if (kb && kb[0] == '+') {
				sprintf(cmd, "-%s %i\n", kb + 1, key);
				Cbuf_AddText(cmd);
			}
		}
		return;
	}

	// during demo playback, most keys bring up the main menu
	if (cls.demoplayback && down && consolekeys[key] && key_dest == key_game) {
		M_ToggleMenu_f();
		return;
	}

	// if not a consolekey, send to the interpreter no matter what mode is
	if ((key_dest == key_menu && menubound[key])
			|| (key_dest == key_console && !consolekeys[key])
			|| (key_dest == key_game && (!con_forcedup || !consolekeys[key]))) {
		kb = keybindings[key];
		if (kb) {
			if (kb[0] == '+') { // button commands add keynum as a parm
				sprintf(cmd, "%s %i\n", kb, key);
				Cbuf_AddText(cmd);
			} else {
				Cbuf_AddText(kb);
				Cbuf_AddText("\n");
			}
		}
		return;
	}

	if (!down)
		return; // other systems only care about key down events

	if (shift_down) {
		key = keyshift[key];
	}

	switch (key_dest) {
		case key_message:
			Key_Message(key);
			break;
		case key_menu:
			M_Keydown(key);
			break;

		case key_game:
		case key_console:
			Key_Console(key);
			break;
		default:
			Sys_Error("Bad key_dest");
	}
}

void Key_ClearStates(void) {
	for (int i = 0; i < 256; i++) {
		keydown[i] = false;
		key_repeats[i] = 0;
	}
}
