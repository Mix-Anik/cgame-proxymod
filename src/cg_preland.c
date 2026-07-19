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
	vec3_t landingPos;
	char buf[256];
	char debugBuf[768];
	vec4_t textColor = { 1.0f, 1.0f, 0.0f, 1.0f };
	const playerState_t *ps = getPs();
	predictDiag_t diag = { 0 };
	qboolean predictSuccess;
	qboolean grounded = (ps->groundEntityNum != ENTITYNUM_NONE);

	if ( !preland.integer ) {
		return;
	}

	predictSuccess = PM_PredictRocketKnockback(ps, (traceFunc_t)trap_CM_BoxTrace, landingPos, &diag);
	if ( predictSuccess ) {
		snprintf(buf, sizeof(buf), "Landing: %.6f %.6f %.6f", landingPos[0], landingPos[1], landingPos[2]);
	} else {
		snprintf(buf, sizeof(buf), "No landing found");
	}

	CG_DrawText(100, 150, 12, buf, textColor, qtrue, qtrue);

	// diag is only meaningful when we were actually grounded (PM_PredictRocketKnockback
	// bails out before touching it otherwise)
	if ( preland.integer >= 2 && grounded ) {
		snprintf(debugBuf, sizeof(debugBuf),
			"^3[PRELAND]^7 Origin: (%.3f %.3f %.3f) View: (%.3f %.3f %.3f) | Muzzle: (%.3f %.3f %.3f) | Explosion: (%.3f %.3f %.3f) | Dist: %.3f | Damage: %.3f | KnockVel: (%.3f %.3f %.3f) mag=%.3f | Gravity: %d | Landing: (%.3f %.3f %.3f) success=%d\n",
			ps->origin[0], ps->origin[1], ps->origin[2],
			ps->viewangles[0], ps->viewangles[1], ps->viewangles[2],
			diag.rocketStart[0], diag.rocketStart[1], diag.rocketStart[2],
			diag.explosionPoint[0], diag.explosionPoint[1], diag.explosionPoint[2],
			diag.distance, diag.damage,
			diag.knockbackVel[0], diag.knockbackVel[1], diag.knockbackVel[2], VectorLength(diag.knockbackVel),
			ps->gravity,
			landingPos[0], landingPos[1], landingPos[2], predictSuccess);
		trap_Print(debugBuf);
	}
}
