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
// sv_edict.c -- entity dictionary

#include "quakedef.h"
#include "CRC.h"

dprograms_t *progs;
dfunction_t *pr_functions;
char *pr_strings;
ddef_t *pr_fielddefs;
ddef_t *pr_globaldefs;
dstatement_t *pr_statements;
globalvars_t *pr_global_struct;
float *pr_globals; // same as pr_global_struct
int pr_edict_size; // in bytes

unsigned short pr_crc;

int type_size[8] = {1, sizeof (string_t) / 4, 1, 3, 1, 1, sizeof (func_t) / 4, sizeof (void *) / 4};

ddef_t *ED_FieldAtOfs(int ofs);
bool ED_ParseEpair(void *base, ddef_t *key, char *s);
char *PR_GetString(int num);
int PR_SetString(char *s);

CVar pr_checkextension("pr_checkextension", "1"); //LH's checkextensions
CVar nomonsters("nomonsters", "0");
CVar gamecfg("gamecfg", "0");
CVar scratch1("scratch1", "0");
CVar scratch2("scratch2", "0");
CVar scratch3("scratch3", "0");
CVar scratch4("scratch4", "0");
CVar savedgamecfg("savedgamecfg", "0", true);
CVar saved1("saved1", "0", true);
CVar saved2("saved2", "0", true);
CVar saved3("saved3", "0", true);
CVar saved4("saved4", "0", true);

#define	MAX_FIELD_LEN	64
#define GEFV_CACHESIZE	2

typedef struct {
    ddef_t *pcache;
    char field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache gefvCache[GEFV_CACHESIZE] = {
    {NULL, ""},
    {NULL, ""}};

/**
 * Sets everything to NULL
 */
void ED_ClearEdict(edict_t *e) {
    memset(&e->v, 0, progs->entityfields * 4);
    e->free = false;
}

/**
 * Either finds a free edict, or allocates a new one.
 *
 * Try to avoid reusing an entity that was recently freed, because it can cause
 * the client to think the entity morphed into something else instead of being
 * removed and recreated, which can cause interpolated angles and bad trails.
 */
edict_t *ED_Alloc(void) {
    int i;
    edict_t *e;

    for (i = svs.maxclients + 1; i < sv.num_edicts; i++) {
        e = EDICT_NUM(i);
        // the first couple seconds of server time can involve a lot of
        // freeing and allocating, so relax the replacement policy
        if (e->free && (e->freetime < 2 || sv.time - e->freetime > 0.5)) {
            ED_ClearEdict(e);
            return e;
        }
    }

    if (i == MAX_EDICTS)
        Sys_Error("ED_Alloc: no free edicts");

    sv.num_edicts++;
    e = EDICT_NUM(i);
    ED_ClearEdict(e);

    return e;
}

/**
 * Marks the edict as free
 *
 * FIXME: walk all entities and NULL out references to this entity
 */
void ED_Free(edict_t *ed) {
    SV_UnlinkEdict(ed); // unlink from world bsp

    ed->free = true;
    ed->v.model = 0;
    ed->v.takedamage = 0;
    ed->v.modelindex = 0;
    ed->v.colormap = 0;
    ed->v.skin = 0;
    ed->v.frame = 0;
    VectorCopy(vec3_origin, ed->v.origin);
    VectorCopy(vec3_origin, ed->v.angles);
    ed->v.nextthink = -1;
    ed->v.solid = 0;

    ed->freetime = sv.time;
}

//===========================================================================

ddef_t *ED_GlobalAtOfs(int ofs) {
    ddef_t *def;
    int i;

    for (i = 0; i < progs->numglobaldefs; i++) {
        def = &pr_globaldefs[i];
        if (def->ofs == ofs)
            return def;
    }
    return NULL;
}

ddef_t *ED_FieldAtOfs(int ofs) {
    for (int i = 0; i < progs->numfielddefs; i++) {
        ddef_t *def = &pr_fielddefs[i];
        if (def->ofs == ofs)
            return def;
    }
    return NULL;
}

ddef_t *ED_FindField(const char *name) {
    for (int i = 0; i < progs->numfielddefs; i++) {
        ddef_t *def = &pr_fielddefs[i];
        if (!strcmp(PR_GetString(def->s_name), name))
            return def;
    }
    return NULL;
}

ddef_t *ED_FindGlobal(char *name) {
    for (int i = 0; i < progs->numglobaldefs; i++) {
        ddef_t *def = &pr_globaldefs[i];
        if (!strcmp(PR_GetString(def->s_name), name))
            return def;
    }
    return NULL;
}

dfunction_t *ED_FindFunction(char *name) {
    for (int i = 0; i < progs->numfunctions; i++) {
        dfunction_t *func = &pr_functions[i];
        if (!strcmp(PR_GetString(func->s_name), name))
            return func;
    }
    return NULL;
}

eval_t *GetEdictFieldValue(edict_t *ed, const char *field) {
    ddef_t *def = NULL;
    static int rep = 0;

    for (int i = 0; i < GEFV_CACHESIZE; i++) {
        if (!strcmp(field, gefvCache[i].field)) {
            def = gefvCache[i].pcache;
            goto Done;
        }
    }

    def = ED_FindField(field);

    if (strlen(field) < MAX_FIELD_LEN) {
        gefvCache[rep].pcache = def;
        strcpy(gefvCache[rep].field, field);
        rep ^= 1;
    }

Done:
    if (!def)
        return NULL;

    return (eval_t *) ((char *) &ed->v + def->ofs * 4);
}

/**
 * Returns a string describing *data in a type specific manner
 */
char *PR_ValueString(etype_t type, eval_t *val) {
    static char line[256];
    ddef_t *def;
    dfunction_t *f;

    type = (etype_t) (type &~DEF_SAVEGLOBAL);

    switch (type) {
        case ev_string:
            sprintf(line, "%s", PR_GetString(val->string));
            break;
        case ev_entity:
            sprintf(line, "entity %i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
            break;
        case ev_function:
            f = pr_functions + val->function;
            sprintf(line, "%s()", PR_GetString(f->s_name));
            break;
        case ev_field:
            def = ED_FieldAtOfs(val->_int);
            sprintf(line, ".%s", PR_GetString(def->s_name));
            break;
        case ev_void:
            sprintf(line, "void");
            break;
        case ev_float:
            sprintf(line, "%5.1f", val->_float);
            break;
        case ev_vector:
            sprintf(line, "'%5.1f %5.1f %5.1f'", val->vector[0], val->vector[1], val->vector[2]);
            break;
        case ev_pointer:
            sprintf(line, "pointer");
            break;
        default:
            sprintf(line, "bad type %i", type);
            break;
    }

    return line;
}

/**
 * Returns a string describing *data in a type specific manner
 *
 * Easier to parse than PR_ValueString
 */
char *PR_UglyValueString(etype_t type, eval_t *val) {
    static char line[256];
    ddef_t *def;
    dfunction_t *f;

    type = (etype_t) (type&~DEF_SAVEGLOBAL);

    switch (type) {
        case ev_string:
            sprintf(line, "%s", PR_GetString(val->string));
            break;
        case ev_entity:
            sprintf(line, "%i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
            break;
        case ev_function:
            f = pr_functions + val->function;
            sprintf(line, "%s", PR_GetString(f->s_name));
            break;
        case ev_field:
            def = ED_FieldAtOfs(val->_int);
            sprintf(line, "%s", PR_GetString(def->s_name));
            break;
        case ev_void:
            sprintf(line, "void");
            break;
        case ev_float:
            sprintf(line, "%f", val->_float);
            break;
        case ev_vector:
            sprintf(line, "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
            break;
        default:
            sprintf(line, "bad type %i", type);
            break;
    }

    return line;
}

/**
 * Returns a string with a description and the contents of a global, padded to
 * 20 field width
 */
char *PR_GlobalString(int ofs) {
    static char line[128];

    void *val = (void *) &pr_globals[ofs];
    ddef_t *def = ED_GlobalAtOfs(ofs);
    if (!def) {
        sprintf(line, "%i(? ? ?)", ofs);
    } else {
        char *s = PR_ValueString((etype_t) def->type, (eval_t *) val);
        sprintf(line, "%i(%s)%s", ofs, PR_GetString(def->s_name), s);
    }

    for (size_t i = strlen(line); i < 20; i++)
        strcat(line, " ");

    strcat(line, " ");

    return line;
}

char *PR_GlobalStringNoContents(int ofs) {
    static char line[128];

    ddef_t *def = ED_GlobalAtOfs(ofs);
    if (!def) {
        sprintf(line, "%i(? ? ?)", ofs);
    } else {
        sprintf(line, "%i(%s)", ofs, PR_GetString(def->s_name));
    }

    for (size_t i = strlen(line); i < 20; i++)
        strcat(line, " ");

    strcat(line, " ");

    return line;
}

/**
 * For debugging
 */
void ED_Print(edict_t *ed) {
    int j;

    if (ed->free) {
        Con_Printf("FREE\n");
        return;
    }

    Con_Printf("\nEDICT %i:\n", NUM_FOR_EDICT(ed));
    for (int i = 1; i < progs->numfielddefs; i++) {
        ddef_t *def = &pr_fielddefs[i];
        char *name = PR_GetString(def->s_name);
        if (name[strlen(name) - 2] == '_')
            continue; // skip _x, _y, _z vars

        int *v = (int *) ((char *) &ed->v + def->ofs * 4);
        // if the value is still all 0, skip the field
        int type = def->type & ~DEF_SAVEGLOBAL;

        for (j = 0; j < type_size[type]; j++)
            if (v[j])
                break;

        if (j == type_size[type])
            continue;

        Con_Printf("%s", name);
        for (size_t l = strlen(name); l < 15; l++)
            Con_Printf(" ");

        Con_Printf("%s\n", PR_ValueString((etype_t) def->type, (eval_t *) v));
    }
}

/**
 * For savegames
 */
void ED_Write(FILE *f, edict_t *ed) {
    int j;

    fprintf(f, "{\n");

    if (ed->free) {
        fprintf(f, "}\n");
        return;
    }

    for (int i = 1; i < progs->numfielddefs; i++) {
        ddef_t *def = &pr_fielddefs[i];
        char *name = PR_GetString(def->s_name);
        if (name[strlen(name) - 2] == '_')
            continue; // skip _x, _y, _z vars

        int *v = (int *) ((char *) &ed->v + def->ofs * 4);

        // if the value is still all 0, skip the field
        int type = def->type & ~DEF_SAVEGLOBAL;
        for (j = 0; j < type_size[type]; j++)
            if (v[j])
                break;

        if (j == type_size[type])
            continue;

        fprintf(f, "\"%s\" ", name);
        fprintf(f, "\"%s\"\n", PR_UglyValueString((etype_t) def->type, (eval_t *) v));
    }

    fprintf(f, "}\n");
}

void ED_PrintNum(int ent) {
    ED_Print(EDICT_NUM(ent));
}

/**
 * For debugging, prints all the entities in the current server
 */
void ED_PrintEdicts(void) {
    Con_Printf("%i entities\n", sv.num_edicts);
    for (int i = 0; i < sv.num_edicts; i++)
        ED_PrintNum(i);
}

/**
 * For debugging, prints a single edicy
 */
void ED_PrintEdict_f(void) {
    int i = atoi(CmdArgs::getArg(1));
    if (i >= sv.num_edicts) {
        Con_Printf("Bad edict number\n");
        return;
    }
    ED_PrintNum(i);
}

/**
 * For debugging
 */
void ED_Count(void) {
    int active, models, solid, step;

    active = models = solid = step = 0;
    for (int i = 0; i < sv.num_edicts; i++) {
        edict_t *ent = EDICT_NUM(i);
        if (ent->free)
            continue;
        active++;
        if (ent->v.solid)
            solid++;
        if (ent->v.model)
            models++;
        if (ent->v.movetype == MOVETYPE_STEP)
            step++;
    }

    Con_Printf("num_edicts:%3i\n", sv.num_edicts);
    Con_Printf("active    :%3i\n", active);
    Con_Printf("view      :%3i\n", models);
    Con_Printf("touch     :%3i\n", solid);
    Con_Printf("step      :%3i\n", step);
}

/*
==============================================================================
                    ARCHIVING GLOBALS
FIXME: need to tag constants, doesn't really work
==============================================================================
 */
void ED_WriteGlobals(FILE *f) {
    fprintf(f, "{\n");
    for (int i = 0; i < progs->numglobaldefs; i++) {
        ddef_t *def = &pr_globaldefs[i];
        int type = def->type;
        if (!(def->type & DEF_SAVEGLOBAL))
            continue;
        type &= ~DEF_SAVEGLOBAL;

        if (type != ev_string && type != ev_float && type != ev_entity)
            continue;

        char *name = PR_GetString(def->s_name);
        fprintf(f, "\"%s\" ", name);
        fprintf(f, "\"%s\"\n", PR_UglyValueString((etype_t) type, (eval_t *) & pr_globals[def->ofs]));
    }
    fprintf(f, "}\n");
}

void ED_ParseGlobals(char *data) {
    char keyname[64];
    ddef_t *key;

    while (1) {
        // parse key
        data = COM_Parse(data);
        if (com_token[0] == '}')
            break;
        if (!data)
            Sys_Error("ED_ParseEntity: EOF without closing brace");

        strcpy(keyname, com_token);

        // parse value
        data = COM_Parse(data);
        if (!data)
            Sys_Error("ED_ParseEntity: EOF without closing brace");

        if (com_token[0] == '}')
            Sys_Error("ED_ParseEntity: closing brace without data");

        key = ED_FindGlobal(keyname);
        if (!key) {
            Con_Printf("'%s' is not a global\n", keyname);
            continue;
        }

        if (!ED_ParseEpair((void *) pr_globals, key, com_token))
            Host_Error("ED_ParseGlobals: parse error");
    }
}

//============================================================================

char *ED_NewString(char *string) {
    char *newString, *new_p;
    int i, l;

    l = strlen(string) + 1;
    newString = (char *) Hunk_Alloc(l);
    new_p = newString;

    for (i = 0; i < l; i++) {
        if (string[i] == '\\' && i < l - 1) {
            i++;
            if (string[i] == 'n')
                *new_p++ = '\n';
            else
                *new_p++ = '\\';
        } else
            *new_p++ = string[i];
    }

    return newString;
}

/**
 * Can parse either fields or globals returns false if error
 */
bool ED_ParseEpair(void *base, ddef_t *key, char *s) {
    int i;
    char string[128];
    ddef_t *def;
    char *v, *w;
    void *d;
    dfunction_t *func;

    d = (void *) ((int *) base + key->ofs);

    switch (key->type & ~DEF_SAVEGLOBAL) {
        case ev_string:
            *(string_t *) d = PR_SetString(ED_NewString(s));
            break;

        case ev_float:
            *(float *) d = atof(s);
            break;

        case ev_vector:
            strcpy(string, s);
            v = string;
            w = string;
            for (i = 0; i < 3; i++) {
                while (*v && *v != ' ')
                    v++;
                *v = 0;
                ((float *) d)[i] = atof(w);
                w = v = v + 1;
            }
            break;

        case ev_entity:
            *(int *) d = EDICT_TO_PROG(EDICT_NUM(atoi(s)));
            break;

        case ev_field:
            def = ED_FindField(s);
            if (!def) {
                Con_Printf("Can't find field %s\n", s);
                return false;
            }
            *(int *) d = G_INT(def->ofs);
            break;

        case ev_function:
            func = ED_FindFunction(s);
            if (!func) {
                Con_Printf("Can't find function %s\n", s);
                return false;
            }
            *(func_t *) d = func - pr_functions;
            break;

        default:
            break;
    }
    return true;
}

/**
 * Parses an edict out of the given string, returning the new position ed should
 * be a properly initialized empty edict.
 *
 * Used for initial level load and for savegames.
 */
char *ED_ParseEdict(char *data, edict_t *ent) {
    ddef_t *key;
    bool anglehack;
    bool init;
    char keyname[256];
    int n;

    init = false;

    // clear it
    if (ent != sv.edicts) // hack
        memset(&ent->v, 0, progs->entityfields * 4);

    // go through all the dictionary pairs
    while (1) {
        // parse key
        data = COM_Parse(data);
        if (com_token[0] == '}')
            break;
        if (!data)
            Sys_Error("ED_ParseEntity: EOF without closing brace");

        // anglehack is to allow QuakeEd to write single scalar angles
        // and allow them to be turned into vectors. (FIXME...)
        if (!strcmp(com_token, "angle")) {
            strcpy(com_token, "angles");
            anglehack = true;
        } else
            anglehack = false;

        // FIXME: change light to _light to get rid of this hack
        if (!strcmp(com_token, "light"))
            strcpy(com_token, "light_lev"); // hack for single light def

        strcpy(keyname, com_token);

        // another hack to fix heynames with trailing spaces
        n = strlen(keyname);
        while (n && keyname[n - 1] == ' ') {
            keyname[n - 1] = 0;
            n--;
        }

        // parse value
        data = COM_Parse(data);
        if (!data)
            Sys_Error("ED_ParseEntity: EOF without closing brace");

        if (com_token[0] == '}')
            Sys_Error("ED_ParseEntity: closing brace without data");

        init = true;

        // keynames with a leading underscore are used for utility comments,
        // and are immediately discarded by quake
        if (keyname[0] == '_')
            continue;

        key = ED_FindField(keyname);
        if (!key) {
            Con_Printf("'%s' is not a field\n", keyname);
            continue;
        }

        if (anglehack) {
            char temp[32];
            strcpy(temp, com_token);
            sprintf(com_token, "0 %s 0", temp);
        }

        if (!ED_ParseEpair((void *) &ent->v, key, com_token))
            Host_Error("ED_ParseEdict: parse error");
    }

    if (!init)
        ent->free = true;

    return data;
}


/**
 * The entities are directly placed in the array, rather than allocated with
 * ED_Alloc, because otherwise an error loading the map would have entity number
 * references out of order.
 *
 * Creates a server's entity / program execution context by parsing textual
 * entity definitions out of an ent file.
 *
 * Used for both fresh maps and savegame loads.  A fresh map would also need to
 * call ED_CallSpawnFunctions () to let the objects initialize themselves.
 */
//qmb :globot
void BotInit(void);

void ED_LoadFromFile(char *data) {
    edict_t *ent;
    int inhibit;
    dfunction_t *func;

    ent = NULL;
    inhibit = 0;
    pr_global_struct->time = sv.time;

    // parse ents
    while (1) {
        // parse the opening brace
        data = COM_Parse(data);
        if (!data)
            break;
        if (com_token[0] != '{')
            Sys_Error("ED_LoadFromFile: found %s when expecting {", com_token);

        if (!ent)
            ent = EDICT_NUM(0);
        else
            ent = ED_Alloc();
        data = ED_ParseEdict(data, ent);

        // remove things from different skill levels or deathmatch
        if (deathmatch.getBool()) {
            if (((int) ent->v.spawnflags & SPAWNFLAG_NOT_DEATHMATCH)) {
                ED_Free(ent);
                inhibit++;
                continue;
            }
        } else if ((current_skill == 0 && ((int) ent->v.spawnflags & SPAWNFLAG_NOT_EASY))
                || (current_skill == 1 && ((int) ent->v.spawnflags & SPAWNFLAG_NOT_MEDIUM))
                || (current_skill >= 2 && ((int) ent->v.spawnflags & SPAWNFLAG_NOT_HARD))) {
            ED_Free(ent);
            inhibit++;
            continue;
        }

        // immediately call spawn function
        if (!ent->v.classname) {
            Con_Printf("No classname for:\n");
            ED_Print(ent);
            ED_Free(ent);
            continue;
        }

        // look for the spawn function
        func = ED_FindFunction(PR_GetString(ent->v.classname));

        if (!func) {
            Con_Printf("No spawn function for:\n");
            ED_Print(ent);
            ED_Free(ent);
            continue;
        }

        pr_global_struct->self = EDICT_TO_PROG(ent);
        //qmb :globots
        if (!strcmp(PR_GetString(ent->v.classname), "worldspawn")) {
            BotInit();
        }
        PR_ExecuteProgram(func - pr_functions);
    }

    Con_DPrintf("%i entities inhibited\n", inhibit);
}

void PR_LoadProgs(void) {
    // flush the non-C variable lookup cache
    for (int i = 0; i < GEFV_CACHESIZE; i++)
        gefvCache[i].field[0] = 0;

    progs = (dprograms_t *) COM_LoadHunkFile("progs.dat");
    if (!progs)
        Sys_Error("PR_LoadProgs: couldn't load progs.dat");
    Con_DPrintf("Programs occupy %iK.\n", com_filesize / 1024);

    CRC crc;
    crc.process((byte *)progs, com_filesize);
    pr_crc = crc.getResult();

    // byte swap the header
    for (unsigned int i = 0; i<sizeof (*progs) / 4; i++)
        ((int *) progs)[i] = LittleLong(((int *) progs)[i]);

    if (progs->version != PROG_VERSION)
        Sys_Error("progs.dat has wrong version number (%i should be %i)", progs->version, PROG_VERSION);
    if (progs->crc != PROGHEADER_CRC)
        Sys_Error("progs.dat system vars have been modified, progdefs.h is out of date");

    pr_functions = (dfunction_t *) ((byte *) progs + progs->ofs_functions);
    pr_strings = (char *) progs + progs->ofs_strings;
    pr_globaldefs = (ddef_t *) ((byte *) progs + progs->ofs_globaldefs);
    pr_fielddefs = (ddef_t *) ((byte *) progs + progs->ofs_fielddefs);
    pr_statements = (dstatement_t *) ((byte *) progs + progs->ofs_statements);

    pr_global_struct = (globalvars_t *) ((byte *) progs + progs->ofs_globals);
    pr_globals = (float *) pr_global_struct;

    pr_edict_size = progs->entityfields * 4 + sizeof (edict_t) - sizeof (entvars_t);

    // byte swap the lumps
    for (int i = 0; i < progs->numstatements; i++) {
        pr_statements[i].op = LittleShort(pr_statements[i].op);
        pr_statements[i].a = LittleShort(pr_statements[i].a);
        pr_statements[i].b = LittleShort(pr_statements[i].b);
        pr_statements[i].c = LittleShort(pr_statements[i].c);
    }

    for (int i = 0; i < progs->numfunctions; i++) {
        pr_functions[i].first_statement = LittleLong(pr_functions[i].first_statement);
        pr_functions[i].parm_start = LittleLong(pr_functions[i].parm_start);
        pr_functions[i].s_name = LittleLong(pr_functions[i].s_name);
        pr_functions[i].s_file = LittleLong(pr_functions[i].s_file);
        pr_functions[i].numparms = LittleLong(pr_functions[i].numparms);
        pr_functions[i].locals = LittleLong(pr_functions[i].locals);
    }

    for (int i = 0; i < progs->numglobaldefs; i++) {
        pr_globaldefs[i].type = LittleShort(pr_globaldefs[i].type);
        pr_globaldefs[i].ofs = LittleShort(pr_globaldefs[i].ofs);
        pr_globaldefs[i].s_name = LittleLong(pr_globaldefs[i].s_name);
    }

    for (int i = 0; i < progs->numfielddefs; i++) {
        pr_fielddefs[i].type = LittleShort(pr_fielddefs[i].type);
        if (pr_fielddefs[i].type & DEF_SAVEGLOBAL)
            Sys_Error("PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
        pr_fielddefs[i].ofs = LittleShort(pr_fielddefs[i].ofs);
        pr_fielddefs[i].s_name = LittleLong(pr_fielddefs[i].s_name);
    }

    for (int i = 0; i < progs->numglobals; i++)
        ((int *) pr_globals)[i] = LittleLong(((int *) pr_globals)[i]);
}

#ifdef JAVA
void JVM_Init(void);
#endif

void PR_Init(void) {
    Cmd::addCmd("edict", ED_PrintEdict_f);
    Cmd::addCmd("edicts", ED_PrintEdicts);
    Cmd::addCmd("edictcount", ED_Count);
    Cmd::addCmd("profile", PR_Profile_f);
    CVar::registerCVar(&pr_checkextension); //LH's checkextensions
    CVar::registerCVar(&nomonsters);
    CVar::registerCVar(&gamecfg);
    CVar::registerCVar(&scratch1);
    CVar::registerCVar(&scratch2);
    CVar::registerCVar(&scratch3);
    CVar::registerCVar(&scratch4);
    CVar::registerCVar(&savedgamecfg);
    CVar::registerCVar(&saved1);
    CVar::registerCVar(&saved2);
    CVar::registerCVar(&saved3);
    CVar::registerCVar(&saved4);
#ifdef JAVA
    JVM_Init();
#endif
}

edict_t *EDICT_NUM(int n) {
    if (n < 0 || n >= sv.max_edicts)
        Sys_Error("EDICT_NUM: bad number %i", n);
    return (edict_t *) ((byte *)sv.edicts + (n) * pr_edict_size);
}

int NUM_FOR_EDICT(edict_t *e) {
    ptrdiff_t b = (byte *)e - (byte *)sv.edicts;
    b = b / pr_edict_size;

    if (b < 0 || b >= sv.num_edicts)
        Sys_Error("NUM_FOR_EDICT: bad pointer");
    return b;
}

/*----------------------*/
#define MAX_PRSTR 1024
char *pr_strtbl[MAX_PRSTR];
int num_prstr;

char *PR_GetString(int num) {
    if (num < 0) {
        //Con_DPrintf("GET:%d == %s\n", num, pr_strtbl[-num]);
        return pr_strtbl[-num];
    }
    return pr_strings + num;
}

int PR_SetString(char *s) {
    if (s - pr_strings < 0) {
        int i;
        for (i = 0; i <= num_prstr; i++)
            if (pr_strtbl[i] == s)
                break;

        if (i < num_prstr)
            return -i;

        if (num_prstr == MAX_PRSTR - 1)
            Sys_Error("MAX_PRSTR");

        num_prstr++;
        pr_strtbl[num_prstr] = s;
        //Con_DPrintf("SET:%d == %s\n", -num_prstr, s);
        return -num_prstr;
    }
    return (int) (s - pr_strings);
}
