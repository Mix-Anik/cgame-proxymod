// bg_predict.c -- trajectory prediction for knockback/splash damage

#include "q_shared.h"
#include "bg_public.h"

// Prediction simulation parameters
static float pred_frametime = 0.008f;       		// Simulation frame time (8ms = 125 fps)
static int   pred_maxIterations = 300;      		// Max iterations (~3 seconds at 100fps)
static float pred_groundCheckOffset = 0.25f; 		// Distance to check below player for ground
static float pred_groundNormalThreshold = 0.7f; 	// Min normal.z (MIN_WALK_NORMAL from bg_public.h)
static float pred_lowVelocityThreshold = 10.0f; 	// Velocity magnitude threshold for terminal state
static int   pred_lowVelIterationThreshold = 100; 	// Iterations before accepting low velocity as landed
// Player bounds from bg_public.h: PLAYER_WIDTH=15, MINS_Z=-24, DEFAULT_HEIGHT=32
static vec3_t pred_playerMins = { -15, -15, -24 };
static vec3_t pred_playerMaxs = { 15, 15, 32 };

// Knockback calculation parameters (from g_combat.c)
static float pred_knockbackMax = 200.0f;   			// Max knockback cap (from damage cap in G_Damage)
static float pred_knockbackMultiplier = 1.0f;		// g_knockback cvar value (default 1.0)
static float pred_playerMass = 200.0f;     			// Player mass for knockback calculation

// Rocket splash damage parameters
static float pred_rocketMaxDamage = 100.0f;			// Rocket max damage
static float pred_rocketSplashRadius = 150.0f;		// Rocket splash radius

/*
===================
PM_ClipVelocity

Clip velocity against a surface (from Q3's PM_ClipVelocity)
===================
*/
static void PM_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out ) {
	float backoff;
	float change;
	int i;
	float overbounce = 1.001f;  // OVERCLIP from Q3

	backoff = DotProduct(in, normal);

	if ( backoff < 0 ) {
		backoff *= overbounce;
	} else {
		backoff /= overbounce;
	}

	for ( i = 0; i < 3; i++ ) {
		change = normal[i] * backoff;
		out[i] = in[i] - change;
	}
}

/*
===================
PM_IsGrounded

Check if player is grounded at current position
===================
*/
static qboolean PM_IsGrounded( vec3_t currentPos, trace_t *outTrace, traceFunc_t trace_func, int clientNum ) {
	vec3_t groundCheckEnd;

	VectorCopy(currentPos, groundCheckEnd);
	groundCheckEnd[2] -= pred_groundCheckOffset;

	trace_func(outTrace, currentPos, pred_playerMins, pred_playerMaxs, groundCheckEnd, clientNum, MASK_PLAYERSOLID);

	// Invalid state: player in solid geometry
	if ( outTrace->allsolid ) {
		return qfalse;
	}

	// Check if we hit ground
	return (outTrace->fraction < 1.0f && outTrace->plane.normal[2] >= pred_groundNormalThreshold);
}

/*
===================
PM_PredictLanding

Predicts where a player will land given an initial knockback velocity.
Returns qtrue if prediction succeeded (player landed), qfalse if trajectory ended in air.
===================
*/
typedef void (*traceFunc_t)( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs,
							  const vec3_t end, int passEntityNum, int contentMask );

qboolean PM_PredictLanding( vec3_t origin, vec3_t velocity, float gravity,
							 int clientNum, traceFunc_t trace_func, vec3_t outLanding ) {
	int iteration = 0;
	vec3_t currentPos, currentVel;
	trace_t trace;
	vec3_t traceEnd;

	VectorCopy( origin, currentPos );
	VectorCopy( velocity, currentVel );

	// Simulate movement until player lands or runs out of iterations
	while ( iteration < pred_maxIterations ) {
		// Check if grounded
		if ( PM_IsGrounded(currentPos, &trace, trace_func, clientNum) ) {
			VectorCopy( currentPos, outLanding );
			return qtrue;
		}

		// Apply gravity
		currentVel[2] -= gravity * pred_frametime;

		// Check if velocity is near zero in all axes (terminal state)
		if ( VectorLength( currentVel ) < pred_lowVelocityThreshold && currentVel[2] < 0 ) {
			// Very low velocity, still falling - simulate a bit more
			if ( iteration > pred_lowVelIterationThreshold ) {
				VectorCopy( currentPos, outLanding );
				return qfalse;  // Didn't land, likely in void/water
			}
		}

		// Move player
		VectorMA( currentPos, pred_frametime, currentVel, traceEnd );
		trace_func( &trace, currentPos, pred_playerMins, pred_playerMaxs, traceEnd, clientNum, MASK_PLAYERSOLID );

		if ( trace.fraction == 0.0f ) {
			// Hit wall immediately, stop prediction
			VectorCopy( currentPos, outLanding );
			return qfalse;
		}

		// Update position
		VectorMA( currentPos, trace.fraction * pred_frametime, currentVel, currentPos );

		// Clip velocity if we hit something
		if ( trace.fraction < 1.0f ) {
			PM_ClipVelocity(currentVel, trace.plane.normal, currentVel);
		}

		iteration++;
	}

	// Max iterations reached
	VectorCopy( currentPos, outLanding );
	return qfalse;
}

/*
===================
PM_PredictRocketKnockback

Predicts landing position from rocket knockback.
Traces where rocket hits, calculates splash damage falloff from explosion point.
===================
*/
qboolean PM_PredictRocketKnockback( playerState_t *ps, traceFunc_t trace_func, vec3_t outLanding ) {
	vec3_t rocketDir, rocketEnd, explosionPoint;
	trace_t rocketTrace;
	float distance, damage, knockback;
	vec3_t knockbackDir;
	vec3_t knockbackVel;

	// Trace rocket path from player's weapon position
	AngleVectors(ps->viewangles, rocketDir, NULL, NULL);
	VectorMA(ps->origin, 8192.0f, rocketDir, rocketEnd);  // Long trace distance
	trace_func(&rocketTrace, ps->origin, NULL, NULL, rocketEnd, ps->clientNum, MASK_SHOT);

	// Explosion point is where rocket hits
	VectorMA(ps->origin, rocketTrace.fraction * 8192.0f, rocketDir, explosionPoint);

	// Calculate distance from player to explosion
	distance = Distance(ps->origin, explosionPoint);

	// Apply splash damage falloff: damage = max * (1 - distance / radius)
	damage = pred_rocketMaxDamage * (1.0f - distance / pred_rocketSplashRadius);
	if ( damage < 0.0f ) {
		damage = 0.0f;
	}

	// Direction from explosion to player (knockback direction)
	VectorSubtract(ps->origin, explosionPoint, knockbackDir);
	VectorNormalize(knockbackDir);

	// Calculate knockback using Q3 formula: kvel = dir * g_knockback * knockback / mass
	knockback = damage;
	if ( knockback > pred_knockbackMax ) {
		knockback = pred_knockbackMax;
	}
	VectorScale(knockbackDir, pred_knockbackMultiplier * knockback / pred_playerMass, knockbackVel);

	// Use the main prediction function with player's current state
	return PM_PredictLanding(ps->origin, knockbackVel, ps->gravity, ps->clientNum,
							  trace_func, outLanding);
}
