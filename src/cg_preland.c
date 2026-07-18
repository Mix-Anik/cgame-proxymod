// cg_preland.c -- pre-landing prediction for rocket knockback

#include "cg_local.h"
#include "cg_cvar.h"
#include "cg_preland.h"
#include "cg_draw.h"
#include "cg_utils.h"
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
	vec3_t landingPos, groundCheckEnd;
	char buf[256];
	char debugBuf[256];
	vec4_t textColor = { 1.0f, 1.0f, 0.0f, 1.0f };
	const playerState_t *ps = getPs();
	trace_t groundTrace;

	if ( !preland.integer ) {
		return;
	}

	if ( PM_PredictRocketKnockback(ps, (traceFunc_t)trap_CM_BoxTrace, landingPos) ) {
		snprintf(buf, sizeof(buf), "Landing: %.6f %.6f %.6f", landingPos[0], landingPos[1], landingPos[2]);
	} else {
		snprintf(buf, sizeof(buf), "No landing found");
	}

	CG_DrawText(100, 150, 12, buf, textColor, qtrue, qtrue);

	// Debug: check ground plane
	VectorCopy(ps->origin, groundCheckEnd);
	groundCheckEnd[2] -= 0.30f;
	trap_CM_BoxTrace(&groundTrace, ps->origin, groundCheckEnd, NULL, NULL, 0, CONTENTS_SOLID);

	float groundZ = ps->origin[2] + groundTrace.fraction * (groundCheckEnd[2] - ps->origin[2]);
	snprintf(debugBuf, sizeof(debugBuf), "Ground Entity: %d | Ground Z: %.6f", ps->groundEntityNum, groundZ);
	CG_DrawText(100, 165, 12, debugBuf, textColor, qtrue, qtrue);
}
