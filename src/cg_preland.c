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

#include <string.h>
#include <stdio.h>

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
	vec3_t landingPos;
	char buf[256];
	vec3_t origin = { 0, 0, 0 };
	vec3_t knockbackDir = { 0, 0, 1 };  // Upward direction from rocket impact
	float damage = 100.0f;  // Rocket damage
	float gravity = 800.0f;  // Default gravity

	if ( !preland.integer ) {
		return;
	}

	// Predict landing position from rocket knockback
	if ( PM_PredictRocketKnockback(origin, knockbackDir, damage, gravity, cg.clientNum,
		(traceFunc_t)trap_CM_BoxTrace, landingPos) ) {
		snprintf(buf, sizeof(buf), "Landing: %.1f %.1f %.1f", landingPos[0], landingPos[1], landingPos[2]);
	} else {
		snprintf(buf, sizeof(buf), "No landing found");
	}

	CG_DrawText(
		100,
		150,
		12,
		buf,
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
