/***************************************************************************
This software will calulate the moving average.
Thanks to B.P.Welford (1962) [Donald Knuth's Art of Computer Programming, Vol 2, page 232, 3rd edition]

Copyright (C) 2018  Marco Scarlino <marco.scarlino@students-live.uniroma2.it>.
moving_average (consisting of moving_average.c and moving_average.h) is free software; you can redistribute it and/or
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

#include "moving_average.h"


void welford_one_pass(long double* mean, long double sample, uint16_t nsamples){
	if(nsamples)
        	*mean=*mean+(sample-*mean)/(long double)(nsamples);
}


void moving_average(long double* mean, long double sample, uint16_t nsamples){
	if(nsamples<2)
		*mean=sample;
	if(nsamples>=NSAMPLES_MAX){
		welford_one_pass(mean, sample, NSAMPLES_MAX);
	}
	else{
		welford_one_pass(mean, sample, nsamples);
	}
}
