/**
 * \file buildid.c
 * \brief Compile in build details
 *
 * Copyright (c) 2011 Andi Sidwell
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "buildid.h"

/*
 * Allow the build system to generate version.h (and define
 * the HAVE_VERSION_H preprocessor macro) or get the version via the BUILD_ID
 * preprocessor macro.  If neither is available, use a sensible default.
 */
#ifdef HAVE_VERSION_H
#include "version.h"
#elif defined(BUILD_ID)
#define STR(x) #x
#define XSTR(x) STR(x)
#define VERSION_STRING XSTR(BUILD_ID)
#endif
#ifndef VERSION_STRING
#define VERSION_STRING "4.2.6"
#endif

const char *buildid = VERSION_NAME " " VERSION_STRING;
const char *buildver = VERSION_STRING;

/**
 * Link a copyright message into the executable
 */
const char *copyright =
	"Copyright (c) 1987-2026 Colaboradores de Angband.\n"
	"\n"
	"Este trabajo es software libre; puedes redistribuirlo y/o modificarlo\n"
	"bajo los términos de cualquiera de las siguientes licencias:\n"
	"\n"
	"a) Licencia Pública General de GNU publicada por la Free Software\n"
	"   Foundation, versión 2, o\n"
	"\n"
	"b) Licencia de Angband:\n"
	"   Este software puede ser copiado y distribuido con fines educativos, de\n"
	"   investigación y sin ánimo de lucro, siempre que este copyright y la\n"
	"   declaración se incluyan en todas esas copias. Pueden aplicarse otros\n"
	"   copyrights.\n";

/**
 * English rendering of copyright, used in English mode since copyright
 * above is a compile-time initializer and cannot be routed through _().
 * See do_cmd_version() in ui-command.c for the selection logic.
 */
const char *copyright_en =
	"Copyright (c) 1987-2026 Angband contributors.\n"
	"\n"
	"This work is free software; you can redistribute it and/or modify it\n"
	"under the terms of either:\n"
	"\n"
	"a) the GNU General Public License as published by the Free Software\n"
	"   Foundation, version 2, or\n"
	"\n"
	"b) the Angband licence:\n"
	"   This software may be copied and distributed for educational, research,\n"
	"   and not for profit purposes provided that this copyright and the\n"
	"   statement are included in all such copies. Other copyrights may\n"
	"   apply.\n";