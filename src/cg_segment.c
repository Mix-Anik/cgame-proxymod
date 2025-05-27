#include "cg_segment.h"

#include "cg_cvar.h"
#include "cg_draw.h"
#include "cg_local.h"
#include "cg_utils.h"
#include "help.h"

#include <stdlib.h>

vec4_t text_rgba = { 1, 1, 1, 1 };
// typedef struct
// {
//   // timestamps for computation
//   uint32_t t_jumpPreGround;
//   uint32_t t_groundTouch;

//   // state machine
//   state_t lastState;

//   // draw data
//   int32_t postDelay;
//   int32_t preDelay;
//   int32_t fullDelay;

//   vec4_t graph_xywh;
//   vec2_t text_xh;

//   vec4_t graph_rgba;
//   vec4_t graph_rgbaPostJump;
//   vec4_t graph_rgbaOnGround;
//   vec4_t graph_rgbaPreJump;
//   vec4_t graph_outline_rgba;
//   vec4_t text_rgba;
// } segment_t;

static vmCvar_t segment;
static vmCvar_t segment_time_xh;

// static segment_t segment_;

static cvarTable_t segment_cvars[] = {
  { &segment, "mdd_segment", "0", CVAR_ARCHIVE_ND },
  { &segment_time_xh, "mdd_segment_time_xh", "6 12", CVAR_ARCHIVE_ND },
};

static help_t segment_help[] = {
  {
    segment_cvars + 1,
    X | H,
    {
      "mdd_segment_time_xh X X",
    },
  },
};

void init_segment(void)
{
  init_cvars(segment_cvars, ARRAY_LEN(segment_cvars));
  init_help(segment_help, ARRAY_LEN(segment_help));
}

void update_segment(void)
{
  update_cvars(segment_cvars, ARRAY_LEN(segment_cvars));
}

void draw_segment(void)
{
  if (!segment.integer) return;

  //ParseVec(segment_time_xh.string, jump_.text_xh, 2);
  CG_DrawText(
    100,
    100,
    12,
    vaf("%i", cg.physicsTime),
    text_rgba,
    qtrue,
    qtrue
  );
}
