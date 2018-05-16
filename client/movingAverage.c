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

#include "movingAverage.h"
#define NSAMPLES_MAX 112; // Number of blocks in 2 hours (60*60*2/64)


/* @method      :- welfordOnePass
+  @param       :-
+               	double* mean 		:- actual mean
+               	double sample           :- actual value that have to be added to mean
+               	int nsamples		:- number of samples
+  @return      :- void
+  @description :- Discrete Welford's one pass algorithm */
void welfordOnePass(double* mean,double sample,int nsamples){
	//*mean=*mean+(sample-*mean)/(double)(nsamples);
}

/* @method      :- movingAverage
+  @param       :-
+               	double* mean 		:- actual mean
+               	double sample           :- actual value that have to be added to mean
+               	int nsamples		:- number of samples
+  @return      :- void
+  @description :- calculate moving average in a window of NSAMPLES_MAX samples */
void movingAverage(double* mean,double sample,int nsamples){
/*
	if(nsamples>=NSAMPLES_MAX){
		return wellfordOnePass(*mean, sample, NSAMPLES_MAX);
	}
	else{
		return wellfordOnePass(*mean, sample, nsamples);
	}
*/
}
