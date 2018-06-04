/***************************************************************************
This software will calulate the moving average.
Thanks to B.P.Welford (1962) [Donald Knuth's Art of Computer Programming, Vol 2, page 232, 3rd edition]

Copyright (C) 2018  Marco Scarlino <marco.scarlino@students-live.uniroma2.it>.
movingAverage (consisting of movingAverage.cpp and movingAverage.h) is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public License
as published by the Free Software Foundation; either version 3
of the License, or (at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************/

#ifndef _MOVING_AVERAGE_H
#define _MOVING_AVERAGE_H

#include <stdint.h>

#ifndef NSAMPLES_MAX
#define NSAMPLES_MAX 20 
#endif

extern void welford_one_pass(long double* mean, long double sample, uint16_t nsamples);
void moving_average(long double* mean, long double sample, uint16_t nsamples);

#endif
