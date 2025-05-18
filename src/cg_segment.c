#include "cg_segment.h"

#include "cg_cvar.h"
#include "cg_draw.h"
#include "cg_local.h"
#include "cg_utils.h"
#include "help.h"

#include <stdlib.h>

static vmCvar_t segment;
static vmCvar_t segment_time_xh;

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

vec4_t text_rgba = { 1, 1, 1, 1 };

void draw_segment(void)
{
  if (!segment.integer) return;

  //ParseVec(segment_time_xh.string, jump_.text_xh, 2);
  CG_DrawText(
    100,
    100,
    12,
    vaf("%i", 999),
    text_rgba,
    qtrue,
    qtrue
  );
}
