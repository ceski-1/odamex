// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2020 by The Odamex Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//		Default Config File.
//
//-----------------------------------------------------------------------------


#include <cstdio>
#include <ctime>
#include <string>
#include <sstream>
#include <vector>

#include "c_bind.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomtype.h"
#include "m_argv.h"
#include "m_fileio.h"
#include "m_misc.h"
#include "i_system.h"
#include "version.h"

// Used to identify the version of the game that saved
// a config file to compensate for new features that get
// put into newer configfiles.
static CVAR (configver, CONFIGVERSIONSTR, "", CVARTYPE_STRING, CVAR_ARCHIVE | CVAR_NOENABLEDISABLE)

EXTERN_CVAR (cl_name)
EXTERN_CVAR (sv_maxplayers)

extern OResFiles wadfiles;

/**
 * Get configuration file path.  This file contains commands to set all
 * archived cvars, bind commands to keys, and set other general game
 * information.
 *
 * @author Randy Heit
 * @return The filename of the configuration file path.
 */
std::string M_GetConfigPath(void)
{
	const char *p = Args.CheckValue("-config");

	if (p)
		return p;

	return M_GetUserFileName("odamex.cfg");
}

// [RH] Don't write a config file if M_LoadDefaults hasn't been called.
bool DefaultsLoaded;

/**
 * Save a configuration file.
 *
 * @author Randy Heit
 * @param Optional: The filename to save the current configuration to.
 */
void STACK_ARGS M_SaveDefaults(std::string filename)
{
	FILE *f;

	if (!DefaultsLoaded)
		return;

	std::string configfile;
	if (!filename.empty())
	{
		M_AppendExtension(filename, ".cfg", true);
		configfile = filename;
	}
	else
	{
		configfile = M_GetConfigPath();
	}

	// Make sure the user hasn't changed configver
	configver.Set(CONFIGVERSIONSTR);

	if ((f = fopen(configfile.c_str(), "w")))
	{
		fprintf(f, "// Generated by Odamex " DOTVERSIONSTR " - don't hurt anything\n\n");

		// Archive all cvars marked as CVAR_ARCHIVE
		fprintf(f, "// --- Console variables ---\n\n");
		cvar_t::C_ArchiveCVars(f);

		// Archive all active key bindings
		fprintf(f, "// --- Key Bindings ---\n\n");
		fprintf(f, "unbindall\n");
		Bindings.ArchiveBindings(f);
		DoubleBindings.ArchiveBindings(f);

		fprintf(f, "\n// --- Automap Bindings ---\n\n");
		fprintf(f, "unambind all\n");
		AutomapBindings.ArchiveBindings(f);

		// Archive all aliases
		fprintf(f, "\n// --- Aliases ---\n\n");
		DConsoleAlias::C_ArchiveAliases(f);

		fclose(f);

		Printf(PRINT_HIGH, "Configuration saved to %s.\n", configfile.c_str());
	}
}

BEGIN_COMMAND (savecfg)
{
	if (argc > 1)
		M_SaveDefaults(argv[1]);
	else
		M_SaveDefaults();
}
END_COMMAND (savecfg)

extern int cvar_defflags;

/**
 * Load a configuration file from the default configuration file.
 *
 * @author Randy Heit
 */
void M_LoadDefaults(void)
{
	// Set default key bindings. These will be overridden
	// by the bindings in the config file if it exists.
	C_BindDefaults();

	std::string cmd = "exec " + C_QuoteString(M_GetConfigPath());

	cvar_defflags = CVAR_ARCHIVE;
	AddCommandString(cmd);
	cvar_defflags = 0;

	AddCommandString("alias ? help");	

	DefaultsLoaded = true;
}

const char* GetShortGameModeString()
{
	if (sv_gametype == GM_COOP)
		return multiplayer ? "COOP" : "SOLO";
	else if (sv_gametype == GM_DM && sv_maxplayers <= 2)
		return "DUEL";
	else if (sv_gametype == GM_DM)
		return "DM";
	else if (sv_gametype == GM_TEAMDM)
		return "TDM";
	else if (sv_gametype == GM_CTF)
		return "CTF";

	return "";
}

// Expands tokens that could be passed to a filename
std::string M_ExpandTokens(const std::string &str)
{
	if (str.empty()) {
		return std::string();
	}

	std::ostringstream buffer;

	for (size_t i = 0;i < str.size();i++) {
		// End of the string.  Just copy the last character
		// and end the loop.
		if (i == str.size() - 1) {
			buffer << str[i];
			break;
		}

		// If it's not a formatting token, copy it and move on.
		if (str[i] != '%') {
			buffer << str[i];
			continue;
		}

		switch (str[i + 1])
		{
			case 'd':
			{
				// Date
				time_t now = time(NULL);
				char date[11] = {0};
				strftime(date, sizeof(date), "%Y%m%d", localtime(&now));
				buffer << date;
				break;
			}
			case 't':
			{
				// Time
				time_t now = time(NULL);
				char date[9] = {0};
				strftime(date, sizeof(date), "%H%M%S", localtime(&now));
				buffer << date;
				break;
			}
			case 'n':
				buffer << cl_name.cstring();
				break;
			case 'g':
			{
				buffer << GetShortGameModeString();
				break;
			}
			case 'w':
				if (wadfiles.size() == 2) {
					// We're playing an IWAD map
					buffer << wadfiles[1].getBasename();
				} else if (wadfiles.size() > 2) {
					// We're playing a PWAD map
					buffer << wadfiles[2].getBasename();
				}
				break;
			case 'm':
				buffer << level.mapname;
				break;
			case 'r':
				buffer << "g" << GitShortHash();
				break;
			case '%':
				// Literal percent
				buffer << '%';
				break;
		}
		// Skip format character
		i++;
	}

	return buffer.str();
}

// Find a free filename that isn't taken
bool M_FindFreeName(std::string &filename, const std::string &extension)
{
	std::string unmodified = filename + '.' + extension;
	if (!M_FileExists(unmodified)) {
		// Name requires no modification.
		filename = unmodified;
		return true;
	}

	int i;

	for (i=1; i <= 9999;i++) {
		std::ostringstream buffer;
		buffer << filename << '.' << i << "." << extension;
		if (!M_FileExists(buffer.str())) {
			// File doesn't exist.
			filename = buffer.str();
			break;
		}
	}

	if (i == 10000)
		return false;
	else
		return true;
}

VERSION_CONTROL (m_misc_cpp, "$Id$")



