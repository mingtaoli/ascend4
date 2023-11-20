/*
ASCEND modelling environment
Copyright (C) 2004-2012 John Pye

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef FPROPS_SOLVE_PX_H
#define FPROPS_SOLVE_PX_H

#include "rundata.h"

int fprops_region_px(double p, double x, const PureFluid *fluid, FpropsError *err);
FluidState2 fprops_solve_px(double p, double x, const PureFluid *fluid, FpropsError *err);

#endif

