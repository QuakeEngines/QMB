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

#ifndef __KEYS__
#define __KEYS__

//
// these are the key numbers that should be passed to Key_Event
//
#define	K_TAB			9
#define	K_ENTER			13
#define	K_ESCAPE		27
#define	K_SPACE			32

// normal keys should be passed as lowercased ascii

#define	K_BACKSPACE		127
#define	K_UPARROW		128
#define	K_DOWNARROW		129
#define	K_LEFTARROW		130
#define	K_RIGHTARROW	131

#define	K_ALT			132
#define	K_CTRL			133
#define	K_SHIFT			134
#define	K_F1			135
#define	K_F2			136
#define	K_F3			137
#define	K_F4			138
#define	K_F5			139
#define	K_F6			140
#define	K_F7			141
#define	K_F8			142
#define	K_F9			143
#define	K_F10			144
#define	K_F11			145
#define	K_F12			146
#define	K_INS			147
#define	K_DEL			148
#define	K_PGDN			149
#define	K_PGUP			150
#define	K_HOME			151
#define	K_END			152

#define K_PAUSE			255

// SDL supports 9 buttons
// mouse buttons generate virtual keys
#define	K_MOUSE1		200
#define	K_MOUSE2		202
#define	K_MOUSE3		201
#define	K_MWHEELUP		203
#define	K_MWHEELDOWN	204
#define	K_MOUSE6		205
#define	K_MOUSE7		206
#define	K_MOUSE8		207
#define	K_MOUSE9		208
#define	K_MOUSE10		209
#define	K_MOUSE11		210
#define	K_MOUSE12		211
#define	K_MOUSE13		212
#define	K_MOUSE14		213
#define	K_MOUSE15		214
#define	K_MOUSE16		215

// joystick buttons
#define K_JOY1		220
#define K_JOY2		221
#define K_JOY3		222
#define K_JOY4		223
#define K_JOY5		224
#define K_JOY6		225
#define K_JOY7		226
#define K_JOY8		227
#define K_JOY9		228
#define K_JOY10		229
#define K_JOY11		230
#define K_JOY12		231
#define K_JOY13		232
#define K_JOY14		233
#define K_JOY15		234
#define K_JOY16		235
#define K_JOY17		236
#define K_JOY18		237
#define K_JOY19		238
#define K_JOY20		239
#define K_JOY21		240
#define K_JOY22		241
#define K_JOY23		242
#define K_JOY24		243
#define K_JOY25		244
#define K_JOY26		245
#define K_JOY27		246
#define K_JOY28		247
#define K_JOY29		248
#define K_JOY30		249
#define K_JOY31		250
#define K_JOY32		251

typedef enum {key_game, key_console, key_message, key_menu} keydest_t;

extern keydest_t	key_dest;
extern char *keybindings[256];
extern	int		key_repeats[256];
extern	int		key_count;			// incremented every key event
extern	int		key_lastpress;

void Key_Event (int key, bool down);
void Key_Init (void);
void Key_WriteBindings (int handle);
void Key_SetBinding (int keynum, const char *binding);
void Key_ClearStates (void);

#endif
