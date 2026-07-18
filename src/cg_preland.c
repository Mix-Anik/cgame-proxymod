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

// Check if player has rocket launcher
static qboolean CG_HasRocket( void ) {
	playerState_t *ps = &cg.snap->ps;
	return ps->weapon == WP_ROCKET_LAUNCHER && ps->ammo[WP_ROCKET_LAUNCHER] > 0;
}

void CG_PredictLanding( void ) {
	vec3_t landingPos;
	playerState_t *ps;
	pmove_t pm;
	vec3_t knockbackVel;
	static vec3_t lastViewangles = { 0, 0, 0 };

	if ( !cl_preland.integer ) {
		return;
	}

	ps = &cg.snap->ps;

	if ( !CG_HasRocket() ) {
		return;
	}

	// Only predict if view angles have changed
	if ( VectorCompare(ps->viewangles, lastViewangles) ) {
		return;
	}
	VectorCopy(ps->viewangles, lastViewangles);

	// Setup pmove structure for prediction
	memset(&pm, 0, sizeof(pm));
	pm.trace = CG_Trace;
	pm.pointcontents = CG_PointContents;

	// For now, use a simple knockback velocity estimate
	// In a real implementation, you'd calculate based on rocket damage and direction
	vec3_t forward;
	AngleVectors(ps->viewangles, forward, NULL, NULL);

	// Estimate knockback: 100 damage rocket
	float knockback = 100.0f;
	VectorScale(forward, -knockback / 2.0f, knockbackVel);

	// Predict where player would land
	if ( PM_PredictLanding(ps->origin, knockbackVel, ps->gravity, cg.clientNum, &pm, landingPos) ) {
		CG_Printf("Landing at: %.1f %.1f %.1f\n", landingPos[0], landingPos[1], landingPos[2]);
	} else {
		CG_Printf("No solid landing found\n");
	}
}
