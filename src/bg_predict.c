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
// bg_predict.c -- trajectory prediction for knockback/splash damage

#include "q_shared.h"
#include "bg_public.h"

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
	int maxIterations = 300;  // ~3 seconds at 100fps
	int iteration = 0;
	int i;
	vec3_t currentPos, currentVel;
	vec3_t mins = { -15, -15, -24 };
	vec3_t maxs = { 15, 15, 32 };
	float frametime = 0.01f;  // 10ms per simulation frame
	float backoff;
	trace_t trace;
	vec3_t traceEnd;

	VectorCopy( origin, currentPos );
	VectorCopy( velocity, currentVel );

	// Simulate movement until player lands or runs out of iterations
	while ( iteration < maxIterations ) {
		// Check if grounded
		vec3_t groundCheckEnd;
		VectorCopy( currentPos, groundCheckEnd );
		groundCheckEnd[2] -= 0.25f;

		trace_func( &trace, currentPos, mins, maxs, groundCheckEnd, clientNum, MASK_PLAYERSOLID );

		// If we hit ground and velocity is mostly horizontal, we've landed
		if ( trace.fraction < 1.0f && trace.plane.normal[2] >= 0.7f ) {
			// Landed on solid ground
			VectorCopy( currentPos, outLanding );
			return qtrue;
		}

		// Apply gravity
		currentVel[2] -= gravity * frametime;

		// Check if velocity is near zero in all axes (terminal state)
		if ( VectorLength( currentVel ) < 10.0f && currentVel[2] < 0 ) {
			// Very low velocity, still falling - simulate a bit more
			if ( iteration > 100 ) {  // Give it time to fall
				VectorCopy( currentPos, outLanding );
				return qfalse;  // Didn't land, likely in void/water
			}
		}

		// Move player
		VectorMA( currentPos, frametime, currentVel, traceEnd );
		trace_func( &trace, currentPos, mins, maxs, traceEnd, clientNum, MASK_PLAYERSOLID );

		if ( trace.fraction == 0.0f ) {
			// Hit wall immediately, stop prediction
			VectorCopy( currentPos, outLanding );
			return qfalse;
		}

		// Update position
		VectorMA( currentPos, trace.fraction * frametime, currentVel, currentPos );

		// Clip velocity if we hit something
		if ( trace.fraction < 1.0f ) {
			// Hit a wall/obstacle - bounce/clip velocity
			backoff = DotProduct( currentVel, trace.plane.normal );
			if ( backoff < 0 ) {
				backoff *= -1;
			} else {
				backoff = 0;
			}

			for ( i = 0; i < 3; i++ ) {
				currentVel[i] = (currentVel[i] - trace.plane.normal[i] * backoff) * 0.9f;
			}
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

Predicts landing position from rocket knockback at a given position.
Handles knockback calculation using Q3 formula.
===================
*/
qboolean PM_PredictRocketKnockback( vec3_t origin, vec3_t knockbackDir, float damage,
									 float gravity, int clientNum, traceFunc_t trace_func,
									 vec3_t outLanding ) {
	vec3_t knockbackVel;
	float knockback;
	float g_knockback = 1.0f;  // Knockback multiplier (can be read from cvar)
	float mass = 200.0f;       // Player mass

	// Calculate knockback using Q3 formula: kvel = dir * g_knockback * knockback / mass
	knockback = damage;
	if ( knockback > 200.0f ) {
		knockback = 200.0f;
	}
	VectorScale(knockbackDir, g_knockback * knockback / mass, knockbackVel);

	// Use the main prediction function
	return PM_PredictLanding(origin, knockbackVel, gravity, clientNum, trace_func, outLanding);
}
