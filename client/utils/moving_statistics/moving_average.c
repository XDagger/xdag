/***************************************************************************
This software will calculate the moving average.
Thanks to B.P.Welford (1962) [Donald Knuth's Art of Computer Programming, Vol 2, page 232, 3rd edition]

Copyright (C) 2018  Marco Scarlino <marco.scarlino@students-live.uniroma2.it>.
moving_average (consisting of moving_average.c and moving_average.h) is free software; 
you can redistribute it and/or modify it.
If you modify this file, you have to release the source code of this file included all of the modifications apported.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
***************************************************************************/

#include "moving_average.h"

// this function update the (old) mean, adding a new sample to this mean.
// it recalculate the mean without keep an array of all old samples
static long double welford_one_pass(long double mean, long double sample, uint16_t nsamples)
{
	if(nsamples) {
		mean = mean + (sample - mean) / (long double)(nsamples);
	}
	return mean;
}

static double welford_one_pass_double(double mean, double sample, uint16_t nsamples)
{
        if(nsamples) {
                mean = mean + (sample - mean) / (double)(nsamples);
        }
        return mean;
}


// Moving average explanation: SX represent the sample, --------------- represent out window (4h in our case), TX represent the time (task time in our case)
// 			S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 ...
// T1			--------------			mean=(S1+S2+S3)/3
// T2			   --------------		mean=(S2+S3+S4)/3
// T3			      --------------		mean=(S3+S4+S5)/3
// and so on.
// moving_average is an approximation, thus it does (mathematically, semantic level):
// 			S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 ...
// T1			--------------			mean=(S1+S2+S3)/3
// T2			   --------------		mean=(S1+S2+S3+S4 -mean/3)/3
// T3			      --------------		mean=(S1+S2+S3+S4+S5 -mean/3 -mean/3)/3
// and so on. Computationally it doesn't start from S1 each time (in fact you can see that it only keep the mean, not every S1,S2,.. etc)

// this function will call normal welford_one_pass at the start, but when number of
// samples will arrive at NSAMPLES_MAX (that represent our 4h mean) it will force 
// welford_one_pass to remove a ideal sample (with mean value) from the mean calculation
// and will add the new sample in the mean, in the same time.
long double moving_average(long double mean, long double sample, uint16_t nsamples)
{
	if(nsamples < 2) {
		mean = sample;
	}
	if(nsamples >= NSAMPLES_MAX) {
		mean = welford_one_pass(mean, sample, NSAMPLES_MAX);
	} else {
		mean = welford_one_pass(mean, sample, nsamples);
	}
	return mean;
}

double moving_average_double(double mean, double sample, uint16_t nsamples)
{
        if(nsamples < 2) {
                mean = sample;
        }
        if(nsamples >= NSAMPLES_MAX) {
                mean = welford_one_pass_double(mean, sample, NSAMPLES_MAX);
        } else {
                mean = welford_one_pass_double(mean, sample, nsamples);
        }
        return mean;
}

