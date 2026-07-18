/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

This file is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

This file is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
===========================================================================
*/
//
// cg_preland.c -- pre-landing prediction for rocket knockback

#include "cg_local.h"
#include "cg_cvar.h"
#include "bg_public.h"

static vmCvar_t cl_preland;

static cvarTable_t preland_cvars[] = {
	{ &cl_preland, "cl_preland", "0", CVAR_ARCHIVE },
};

static size_t preland_cvars_size = ARRAY_LEN(preland_cvars);

void CG_PrelandInit( void ) {
	init_cvars(preland_cvars, preland_cvars_size);
}

void CG_PrelandUpdate( void ) {
	update_cvars(preland_cvars, preland_cvars_size);
}

void CG_PredictLanding( void ) {
	// Placeholder for landing prediction
	// To be integrated when cgame proxy architecture is better understood
	if ( !cl_preland.integer ) {
		return;
	}
}
