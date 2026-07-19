// bg_predict.c -- trajectory prediction for knockback/splash damage

#include "q_shared.h"
#include "bg_public.h"

// Matches PM_SlideMove's MAX_CLIP_PLANES (bg_slidemove.c) -- the ground plane
// plus up to 4 planes accumulated across bump iterations in a single frame.
#define PRED_MAX_CLIP_PLANES 5

// Simulation frame time (8ms = 125 fps, matches defrag's fixed physics rate)
static float pred_frametime = 0.008f;
static int   pred_maxIterations = 300;      		// Max iterations (~3 seconds at 125fps)
static float pred_groundCheckOffset = 0.25f; 		// Distance to check below player for ground (matches PM_GroundTrace)
static float pred_groundNormalThreshold = 0.7f; 	// Min normal.z (MIN_WALK_NORMAL from bg_public.h)
static float pred_lowVelocityThreshold = 10.0f; 	// Velocity magnitude threshold for terminal state
static int   pred_lowVelIterationThreshold = 100; 	// Iterations before accepting low velocity as landed
// Player bounds from bg_public.h: PLAYER_WIDTH=15, MINS_Z=-24, DEFAULT_HEIGHT=32
static vec3_t pred_playerMins = { -15, -15, -24 };
static vec3_t pred_playerMaxs = { 15, 15, 32 };

// Knockback calculation parameters (from g_combat.c G_Damage)
static float pred_knockbackMax = 200.0f;   			// Max knockback cap (from damage cap in G_Damage)
static float pred_knockbackMultiplier = 1000.0f;	// g_knockback cvar value (default 1000, confirmed on server)
static float pred_playerMass = 200.0f;     			// Player mass, hardcoded in G_Damage

// CPM (df_promode) self-splash rocket-jump boost, confirmed from DeFRaG's
// G_Damage disassembly (oDFs/disasm/G_COMBAT/G_Damage.md): when a player
// splashes themselves with MOD_ROCKET_SPLASH under promode, the horizontal
// (X/Y) components of the normalized knockback direction are multiplied by
// exactly 1.2 before the final magnitude scale -- the vertical component is
// untouched. This is a confirmed game constant, not an approximation.
static float pred_selfSplashHorizBonus = 1.2f;

// How far out to ray-trace for the explosion point. This is a single static
// trace (player and world don't move during it), not a timed simulation of
// the missile's actual flight, so it doesn't need to match real rocket speed
// -- it just needs to be farther than any level extends. 16384 covers the
// engine's max world coordinate range with margin.
static float pred_rocketTraceDistance = 16384.0f;

// Rocket splash damage parameters (from fire_rocket in g_missile.c)
static float pred_rocketMaxDamage = 100.0f;			// Rocket max damage
static float pred_rocketSplashRadius = 120.0f;		// Rocket splash radius
static float pred_muzzleForwardOffset = 14.0f;		// CalcMuzzlePointOrigin forward offset

static float pred_knockbackZBias = 24.0f;			// G_RadiusDamage biases knockback dir upward by this before normalizing
static float pred_knockbackDirEpsilon = 0.001f;		// Below this length, treat knockback dir as "explosion at player origin"
static float pred_gravityAverageWeight = 0.5f;		// Trapezoidal integration weight; not a tunable, always the midpoint of pre/post-gravity velocity

// PMF_TIME_KNOCKBACK / grounded-slide parameters (from oDFs/disasm/BG_PMOVE:
// PM_GroundTrace.md, PM_WalkMove.md, PM_Friction.md). While a knocked-back
// player is grounded, ground friction is skipped and gravity is applied
// directly (instead of the normal walk-move path) for the pm_time window
// G_Damage sets on the hit -- letting a player slide, not fly, across flat
// ground for a self-splash that doesn't kick them fully airborne.
static float pred_pmFriction = 6.0f;				// pm_friction, confirmed unmodified (disasm/README.md 0x14d8)
static float pred_pmStopSpeed = 100.0f;			// pm_stopspeed, confirmed unmodified (disasm/README.md 0x14b8)
static float pred_kickoffDotThreshold = 10.0f;		// PM_GroundTrace "kickoff" dot(velocity,groundNormal) threshold
static float pred_knockbackPmTimeScale = 2.0f;		// pm_time (ms) = knockback * 2, confirmed in G_Damage
static float pred_knockbackPmTimeMin = 50.0f;		// confirmed clamp floor (ms)
static float pred_knockbackPmTimeMax = 200.0f;		// confirmed clamp ceiling (ms)

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

typedef struct {
	qboolean hasGroundPlane;	// pml.groundPlane -- any floor within reach, even a too-steep one
	qboolean walking;			// pml.walking -- true only on a standable floor (routes to PM_WalkMove)
	trace_t  trace;				// the downward ground-check trace; plane.normal valid when hasGroundPlane
} groundState_t;

/*
===================
PM_GroundTraceSim

PmoveSingle's pre-movement ground check: a 0.25-unit downward trace,
classified in the confirmed order from oDFs/disasm/BG_PMOVE/PM_GroundTrace.md
-- allsolid/miss clears both flags, then the "kickoff" test (moving up and
away from the plane fast enough) clears both flags too, and only then is
steepness checked (clears `walking` only; `hasGroundPlane` stays true, since
a too-steep surface still participates in PM_SlideMove's ground-plane clip).
===================
*/
static void PM_GroundTraceSim( const vec3_t currentPos, const vec3_t currentVel, groundState_t *out,
								int clientNum, traceFunc_t trace_func ) {
	vec3_t groundCheckEnd;

	out->hasGroundPlane = qfalse;
	out->walking = qfalse;

	VectorCopy(currentPos, groundCheckEnd);
	groundCheckEnd[2] -= pred_groundCheckOffset;

	trace_func(&out->trace, currentPos, groundCheckEnd, pred_playerMins, pred_playerMaxs, clientNum, MASK_PLAYERSOLID);

	if ( out->trace.allsolid || out->trace.fraction == 1.0f ) {
		return;
	}

	if ( currentVel[2] > 0.0f && DotProduct(currentVel, out->trace.plane.normal) > pred_kickoffDotThreshold ) {
		return;
	}

	out->hasGroundPlane = qtrue;
	out->walking = ( out->trace.plane.normal[2] >= pred_groundNormalThreshold );
}

/*
===================
PM_SlideMoveCore

Faithful port of DeFRaG's confirmed PM_SlideMove (oDFs/disasm/BG_SLIDEMOVE/
PM_SlideMove.md -- "no CPM modifications, standard Q3"): up to 4 bump-clip
iterations against multiple planes within a single frame (handling corners
via the two-plane "crease" slide, and stopping dead on a genuine 3-plane
interaction), with optional half-step gravity integration and an internal
ground-plane clip.

Critically: if any pm_time timer is still counting down (which includes our
PMF_TIME_KNOCKBACK window), the velocity that persists into the next frame
is reset to `primalVelocity` -- its value from before this call's own
clipping/gravity -- regardless of what collision happened during the move.
This is confirmed vanilla Q3 behavior (`if (ps->pm_time) ps->velocity =
primal_velocity;`), not a CPM change. It's why grazing a wall mid-slide
doesn't cost you speed while still inside the knockback window.
===================
*/
static void PM_SlideMoveCore( vec3_t currentPos, vec3_t currentVel, qboolean applyGravity,
							   qboolean hasGroundPlane, vec3_t groundNormal, float gravity,
							   float pmTimeMs, int clientNum, traceFunc_t trace_func ) {
	vec3_t primalVelocity, endVelocity;
	vec3_t planes[PRED_MAX_CLIP_PLANES];
	int numplanes = 0;
	float timeLeft = pred_frametime;
	int bump;

	VectorCopy(currentVel, primalVelocity);
	VectorClear(endVelocity);

	if ( applyGravity ) {
		VectorCopy(currentVel, endVelocity);
		endVelocity[2] -= gravity * pred_frametime;
		currentVel[2] = ( currentVel[2] + endVelocity[2] ) * pred_gravityAverageWeight;
		primalVelocity[2] = endVelocity[2];
		if ( hasGroundPlane ) {
			PM_ClipVelocity(currentVel, groundNormal, currentVel);
		}
	}

	if ( hasGroundPlane ) {
		VectorCopy(groundNormal, planes[numplanes]);
		numplanes++;
	}
	{
		vec3_t velDir;
		VectorCopy(currentVel, velDir);
		VectorNormalize(velDir);
		VectorCopy(velDir, planes[numplanes]);
		numplanes++;
	}

	for ( bump = 0; bump < 4; bump++ ) {
		vec3_t end;
		trace_t trace;
		int i;
		qboolean samePlane = qfalse;

		VectorMA(currentPos, timeLeft, currentVel, end);
		trace_func(&trace, currentPos, end, pred_playerMins, pred_playerMaxs, clientNum, MASK_PLAYERSOLID);

		if ( trace.allsolid ) {
			// Completely trapped: don't build up falling speed, but allow sideways motion next frame
			currentVel[2] = 0.0f;
			return;
		}

		if ( trace.fraction > 0.0f ) {
			VectorCopy(trace.endpos, currentPos);
		}

		if ( trace.fraction == 1.0f ) {
			break;  // moved the entire distance, no clipping needed
		}

		timeLeft -= timeLeft * trace.fraction;

		if ( numplanes >= PRED_MAX_CLIP_PLANES ) {
			// Shouldn't happen in practice; matches the real function's safety clamp
			VectorClear(currentVel);
			return;
		}

		// Same-plane nudge: fixes epsilon issues when re-hitting a plane already in the list
		for ( i = 0; i < numplanes; i++ ) {
			if ( DotProduct(trace.plane.normal, planes[i]) > 0.99f ) {
				VectorAdd(trace.plane.normal, currentVel, currentVel);
				samePlane = qtrue;
				break;
			}
		}
		if ( samePlane ) {
			continue;
		}
		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		// Find a plane the velocity actually enters and clip along it (and,
		// if needed, a second plane it then enters -- sliding along the
		// crease between them; a genuine third-plane interaction stops dead)
		for ( i = 0; i < numplanes; i++ ) {
			vec3_t clipVelocity, endClipVelocity;
			int j;
			float into = DotProduct(currentVel, planes[i]);

			if ( into >= 0.1f ) {
				continue;
			}

			PM_ClipVelocity(currentVel, planes[i], clipVelocity);
			PM_ClipVelocity(endVelocity, planes[i], endClipVelocity);

			for ( j = 0; j < numplanes; j++ ) {
				vec3_t dir;
				float d;
				int k;
				qboolean tripleStop = qfalse;

				if ( j == i ) continue;
				if ( DotProduct(clipVelocity, planes[j]) >= 0.1f ) continue;

				PM_ClipVelocity(clipVelocity, planes[j], clipVelocity);
				PM_ClipVelocity(endClipVelocity, planes[j], endClipVelocity);

				if ( DotProduct(clipVelocity, planes[i]) >= 0.0f ) continue;

				CrossProduct(planes[i], planes[j], dir);
				VectorNormalize(dir);
				d = DotProduct(dir, currentVel);
				VectorScale(dir, d, clipVelocity);

				CrossProduct(planes[i], planes[j], dir);
				VectorNormalize(dir);
				d = DotProduct(dir, endVelocity);
				VectorScale(dir, d, endClipVelocity);

				for ( k = 0; k < numplanes; k++ ) {
					if ( k == i || k == j ) continue;
					if ( DotProduct(clipVelocity, planes[k]) >= 0.1f ) continue;
					tripleStop = qtrue;
					break;
				}
				if ( tripleStop ) {
					VectorClear(currentVel);
					return;
				}
			}

			VectorCopy(clipVelocity, currentVel);
			VectorCopy(endClipVelocity, endVelocity);
			break;
		}
	}

	if ( applyGravity ) {
		VectorCopy(endVelocity, currentVel);
	}

	if ( pmTimeMs > 0.0f ) {
		VectorCopy(primalVelocity, currentVel);
	}
}

/*
===================
PM_PredictLanding

Predicts where a player will come to rest given an initial knockback
velocity, by faithfully re-running PmoveSingle's per-frame structure with no
input: ground-check, drop the pm_time timer, then dispatch to a
PM_WalkMove-equivalent (grounded, standable) or PM_AirMove-equivalent
(everything else) step built on the shared PM_SlideMoveCore above. Returns
qtrue if the player comes to rest, qfalse if the trajectory ends in the air
(void/water) or is blocked before settling.

pm_time is decremented *before* the movement dispatch each iteration, not
after -- matching PmoveSingle's confirmed call order (PM_GroundTrace, then
PM_DropTimers, then the WalkMove/AirMove dispatch). This means the frame on
which the knockback timer expires already sees it as expired for that
frame's own friction/gravity/velocity-reset behavior.
===================
*/
qboolean PM_PredictLanding( const vec3_t origin, const vec3_t velocity, float gravity, float knockback,
							 int clientNum, traceFunc_t trace_func, vec3_t outLanding ) {
	int iteration = 0;
	vec3_t currentPos, currentVel;
	groundState_t ground;
	float pmTimeMs;

	VectorCopy( origin, currentPos );
	VectorCopy( velocity, currentVel );

	pmTimeMs = knockback * pred_knockbackPmTimeScale;
	if ( pmTimeMs < pred_knockbackPmTimeMin ) pmTimeMs = pred_knockbackPmTimeMin;
	if ( pmTimeMs > pred_knockbackPmTimeMax ) pmTimeMs = pred_knockbackPmTimeMax;

	while ( iteration < pred_maxIterations ) {
		PM_GroundTraceSim(currentPos, currentVel, &ground, clientNum, trace_func);

		// PM_DropTimers runs before the movement dispatch each frame
		pmTimeMs -= pred_frametime * 1000.0f;
		if ( pmTimeMs < 0.0f ) pmTimeMs = 0.0f;

		if ( ground.walking ) {
			// ---- PM_WalkMove-equivalent ----
			vec3_t horizVel;
			float speed, newspeed, control, drop, preClipSpeed;

			// PM_Friction: ground-drop term skipped while PMF_TIME_KNOCKBACK is active
			VectorCopy(currentVel, horizVel);
			horizVel[2] = 0.0f;
			speed = VectorLength(horizVel);

			if ( speed < 1.0f ) {
				currentVel[0] = 0.0f;
				currentVel[1] = 0.0f;
			} else {
				drop = 0.0f;
				if ( pmTimeMs <= 0.0f ) {
					control = ( speed < pred_pmStopSpeed ) ? pred_pmStopSpeed : speed;
					drop = control * pred_pmFriction * pred_frametime;
				}
				newspeed = speed - drop;
				if ( newspeed < 0.0f ) newspeed = 0.0f;
				newspeed /= speed;
				currentVel[0] *= newspeed;
				currentVel[1] *= newspeed;
				currentVel[2] *= newspeed;
			}

			// Gravity is only integrated directly in the walk path while
			// still in the knockback window (confirmed: PM_WalkMove.md line 26)
			if ( pmTimeMs > 0.0f ) {
				currentVel[2] -= gravity * pred_frametime;
			}

			// Clip into the ground plane, preserving speed (PM_WalkMove's own
			// pre-StepSlideMove clip -- distinct from, and in addition to,
			// the internal ground clip PM_SlideMoveCore would do if gravity
			// were passed to it, which it isn't for the walk path)
			preClipSpeed = VectorLength(currentVel);
			if ( preClipSpeed > pred_knockbackDirEpsilon ) {
				PM_ClipVelocity(currentVel, ground.trace.plane.normal, currentVel);
				VectorNormalize(currentVel);
				VectorScale(currentVel, preClipSpeed, currentVel);
			}

			// PM_WalkMove returns immediately without calling PM_StepSlideMove
			// at all once horizontal velocity is exactly zero -- the player is
			// at rest, full stop, not even a partial frame of movement occurs.
			if ( currentVel[0] == 0.0f && currentVel[1] == 0.0f ) {
				VectorCopy( currentPos, outLanding );
				return qtrue;
			}

			PM_SlideMoveCore(currentPos, currentVel, qfalse, ground.hasGroundPlane,
							  ground.trace.plane.normal, gravity, pmTimeMs, clientNum, trace_func);
		} else {
			// ---- PM_AirMove-equivalent ----
			// PM_Friction is called here too, but with no water/flight/spectator
			// state modeled and walking=false, its ground-drop term can't apply
			// either way -- it's an unconditional no-op for this simulation.

			if ( ground.hasGroundPlane ) {
				// Too-steep-to-stand surface: AirMove's own pre-StepSlideMove
				// clip, WITHOUT the speed-preserving rescale WalkMove does
				PM_ClipVelocity(currentVel, ground.trace.plane.normal, currentVel);
			}

			PM_SlideMoveCore(currentPos, currentVel, qtrue, ground.hasGroundPlane,
							  ground.trace.plane.normal, gravity, pmTimeMs, clientNum, trace_func);

			// Stuck in a void/water: falling slowly for a long time with no ground in reach
			if ( VectorLength( currentVel ) < pred_lowVelocityThreshold && currentVel[2] < 0 ) {
				if ( iteration > pred_lowVelIterationThreshold ) {
					VectorCopy( currentPos, outLanding );
					return qfalse;
				}
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
PM_CalcMuzzlePoint

Mirrors CalcMuzzlePointOrigin in g_weapon.c (origin + viewheight up + 14
forward, snapped to integer coords) without needing a gentity_t/entityState_t
===================
*/
static void PM_CalcMuzzlePoint( const playerState_t *ps, const vec3_t forward, vec3_t muzzlePoint ) {
	VectorCopy(ps->origin, muzzlePoint);
	muzzlePoint[2] += ps->viewheight;
	VectorMA(muzzlePoint, pred_muzzleForwardOffset, forward, muzzlePoint);
	SnapVector(muzzlePoint);
}

/*
===================
PM_DistanceToBox

Distance from a point to the nearest point on an axis-aligned box. Matches
G_RadiusDamage, which measures splash falloff to the target's absmin/absmax
rather than to its origin -- this matters a lot at the point-blank ranges
rocket jumps happen at.
===================
*/
static float PM_DistanceToBox( const vec3_t point, const vec3_t mins, const vec3_t maxs ) {
	vec3_t v;
	int i;

	for ( i = 0; i < 3; i++ ) {
		if ( point[i] < mins[i] ) {
			v[i] = mins[i] - point[i];
		} else if ( point[i] > maxs[i] ) {
			v[i] = point[i] - maxs[i];
		} else {
			v[i] = 0.0f;
		}
	}
	return VectorLength( v );
}

/*
===================
PM_CalcKnockbackVel

kvel = dir * g_knockback * knockback / mass. dir is the direction from the
explosion to the player, biased 24 units up like G_RadiusDamage does so
players get knocked into the air more, then (for CPM self-splash specifically)
has its horizontal components boosted 1.2x -- both confirmed from DeFRaG's
G_Damage disassembly. Zero if the explosion is at the player's exact origin
(no direction to push).
===================
*/
static void PM_CalcKnockbackVel( const playerState_t *ps, const vec3_t explosionPoint, float damage, vec3_t outKnockbackVel ) {
	vec3_t knockbackDir;
	float knockback;

	VectorSubtract(ps->origin, explosionPoint, knockbackDir);
	knockbackDir[2] += pred_knockbackZBias;
	if ( VectorLength(knockbackDir) < pred_knockbackDirEpsilon ) {
		VectorClear(outKnockbackVel);
		return;
	}
	VectorNormalize(knockbackDir);

	// CPM self-splash: boost horizontal components only, after normalizing
	// and before the final magnitude scale (matches disassembled order)
	knockbackDir[0] *= pred_selfSplashHorizBonus;
	knockbackDir[1] *= pred_selfSplashHorizBonus;

	knockback = damage;
	if ( knockback > pred_knockbackMax ) {
		knockback = pred_knockbackMax;
	}
	VectorScale(knockbackDir, pred_knockbackMultiplier * knockback / pred_playerMass, outKnockbackVel);
}

/*
===================
PM_PredictRocketKnockback

Predicts landing position from rocket knockback.
Traces where rocket hits, calculates splash damage falloff from explosion point.
===================
*/
qboolean PM_PredictRocketKnockback( const playerState_t *ps, traceFunc_t trace_func, vec3_t outLanding,
									 predictDiag_t *outDiag ) {
	vec3_t rocketStart, rocketDir, rocketEnd, explosionPoint;
	trace_t rocketTrace;
	float distance, damage;
	vec3_t knockbackVel;
	vec3_t absmin, absmax;

	// Only predict if player is on ground
	if ( ps->groundEntityNum == ENTITYNUM_NONE ) {
		return qfalse;
	}

	// Trace rocket path from the actual muzzle position, not the player's feet
	AngleVectors(ps->viewangles, rocketDir, NULL, NULL);
	PM_CalcMuzzlePoint(ps, rocketDir, rocketStart);

	VectorMA(rocketStart, pred_rocketTraceDistance, rocketDir, rocketEnd);
	trace_func(&rocketTrace, rocketStart, rocketEnd, NULL, NULL, ps->clientNum, MASK_SHOT);

	// Explosion point is where rocket hits
	VectorMA(rocketStart, rocketTrace.fraction * pred_rocketTraceDistance, rocketDir, explosionPoint);

	VectorAdd(ps->origin, pred_playerMins, absmin);
	VectorAdd(ps->origin, pred_playerMaxs, absmax);
	distance = PM_DistanceToBox(explosionPoint, absmin, absmax);

	// Apply splash damage falloff: damage = max * (1 - distance / radius),
	// truncated to an int like G_RadiusDamage/G_Damage do
	damage = pred_rocketMaxDamage * (1.0f - distance / pred_rocketSplashRadius);
	if ( damage < 0.0f ) {
		damage = 0.0f;
	}
	damage = (float)(int)damage;

	PM_CalcKnockbackVel(ps, explosionPoint, damage, knockbackVel);

	if ( outDiag ) {
		VectorCopy(rocketStart, outDiag->rocketStart);
		VectorCopy(explosionPoint, outDiag->explosionPoint);
		outDiag->distance = distance;
		outDiag->damage = damage;
		VectorCopy(knockbackVel, outDiag->knockbackVel);
	}

	// knockback = min(damage, 200), same cap G_Damage applies before using it
	// both for the kvel scale and for the pm_time = clamp(knockback*2,50,200)
	// duration -- PM_PredictLanding needs the same capped value for its pm_time.
	{
		float knockback = ( damage > pred_knockbackMax ) ? pred_knockbackMax : damage;
		return PM_PredictLanding(ps->origin, knockbackVel, ps->gravity, knockback, ps->clientNum,
								  trace_func, outLanding);
	}
}
