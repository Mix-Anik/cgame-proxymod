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
#include "cg_preland.h"
#include "cg_draw.h"
#include "bg_public.h"

static vmCvar_t preland;

static cvarTable_t preland_cvars[] = {
	{ &preland, "mdd_preland", "0", CVAR_ARCHIVE },
};

void init_preland( void ) {
	init_cvars(preland_cvars, ARRAY_LEN(preland_cvars));
}

void update_preland( void ) {
	update_cvars(preland_cvars, ARRAY_LEN(preland_cvars));
}

void draw_preland( void ) {
	vec4_t textColor = { 1.0f, 1.0f, 0.0f, 1.0f };  // Yellow

	if ( !preland.integer ) {
		return;
	}

	// TODO: Implement actual prediction here
	// For now, just show a placeholder
	CG_DrawText(
		100,
		150,
		12,
		"Preland: [feature ready]",
		textColor,
		qtrue,
		qtrue
	);
}

void CG_PredictLanding( void ) {
	// Placeholder for landing prediction
	// To be integrated when cgame proxy architecture is better understood
	if ( !preland.integer ) {
		return;
	}
}
