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
// cg_preland.h -- pre-landing prediction header

#ifndef CG_PRELAND_H
#define CG_PRELAND_H

void init_preland( void );
void update_preland( void );
void draw_preland( void );
void CG_PredictLanding( void );

#endif // CG_PRELAND_H
