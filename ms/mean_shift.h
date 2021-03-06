#ifndef MEAN_SHIFT_H
#define MEAN_SHIFT_H

// Copyright 2013 Tom SF Haines

// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

//   http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.



#include "kernels.h"
#include "spatial.h"
#include "balls.h"



// Note: All of the below functions require that feature vectors (fv) are given in transformed space, i.e. have been multiplied by the vector in the data matrix.

// Returns the total weight within the given data matrix - normally each exemplar is weighted as 1 and this is the exmeplar count, but that is not always the case...
float calc_weight(DataMatrix * dm);



// Returns the normalising constant required to be passed into some of the below functions - usually cached. config is for the kernel, weight is the output of calc_weight...
float calc_norm(DataMatrix * dm, const Kernel * kernel, KernelConfig config, float weight);



// This calculates the probability of a given feature vector, as defined by the kernel density estimate defined by the provided spatial and kernel (with an associated alpha). You also provide the normalising multiplier, as that can be cached to save repeated calculation, and quality to define the search range around the kernel. The norm parameter must be the kernel normalising constant divided by the weight of the samples and factoring in the scale change - as calc_norm does. Note that this is strange as it expects fv in scaled space and then outputs a probability in unscaled space!..
float prob(Spatial spatial, const Kernel * kernel, KernelConfig config, const float * fv, float norm, float quality);



// This draws a sample from the distribution - you provide the usual indexing structure, which contains the data matrix, the kernel to use and an index into the philox rng, which it then uses to deterministically draw into out. out must be long enough to store the # of dimensions within the data matrix...
void draw(DataMatrix * dm, const Kernel * kernel, KernelConfig config, PhiloxRNG * rng, float * out);



// Calculates the log probability of all the items in the data set, leave one out style - allows for model comparison so you can optimise any of the parameters, such as scale or kernel type. Parameters match up with the prob function, except it gets the feature vectors from spatial and returns a negative log probability. It includes three extra parameters - a minimum probability to assign to any given exemplar, to limit the damage of outliers, and a sample clamp - if less than the exemplar count it will randomly select this many (with repetition) and return that instead. In this case the provided rng is needed...
// (Note that it does not correctly adjust the total weight for each exemplar probability calculation, which technically can bias things a bit if they all have different weights, but not really enough to worry about, and it would prevent the use of the norm optimisation.)
float loo_nll(Spatial spatial, const Kernel * kernel, KernelConfig config, float norm, float quality, float limit, int sample_clamp, PhiloxRNG * rng);



// Calculates and returns an approximation of the entropy of the distribution, using the samples that the datamatrix contains as a sample from the distribution so its super efficient to calculate. Has the same sample_clamp/rng idea as loo_nll...
float entropy(Spatial spatial, const Kernel * kernel, KernelConfig config, float norm, float quality, int sample_clamp, PhiloxRNG * rng);


// Calculates the Kullback-Leibler divergance of q from p D(P||Q), as in the average number of extra nats required to encode draws from p given an encoder that assumes draws from q. Same approach as entropy, using the samples in the KDE as both samples and to define the distribution. Note that the constraint the the KL-divergance be positive is broken by this estimate - you can get negative values out. What to do about this is left to the user. limit is a clamp on how low probability of q values are allowed to get, to avoid divide by zero. Supports the bootystrap optimisation of loo_nll...
float kl_divergence(Spatial spatial_p, const Kernel * kernel_p, KernelConfig config_p, float norm_p, float quality_p, Spatial spatial_q, const Kernel * kernel_q, KernelConfig config_q, float norm_q, float quality_q, float limit, int sample_clamp, PhiloxRNG * rng);



// Given a kernel (with an alpha parameter), spatial indexing data structure and a feature vector this updates that feature vector to be its mean shift converged point. A temporary vector of the same length as the feature must also be provided. The quality parameter goes from 0 to 1, and maps to the low and high spatial ranges provided by the kernel. There is also an epsilon parameter - it stops when movement drops below it, typically something like 1e-3 is good. The iteration cap ensures that no infinite loops cna occur if epsilon is too low. If the spatial has an ignore entry in the feature vector it uses that as a weight (Will call the get method of spatial.)...
void mode(Spatial spatial, const Kernel * kernel, KernelConfig config, float * fv, float * temp, float quality, float epsilon, int iter_cap);


// Like mode, except its trying to find the relevant cluster in a provided Balls object to merge with, noting that if it doesn't do so it will create a new ball. Returns the index of the ball it reaches. Parameters match up with those for mode and cluster - this really is a half-way house between them (though no path shortening), for when you want to cluster data using a different set of exemplars to define the desnity estimate...
int mode_merge(Spatial spatial, const Kernel * kernel, KernelConfig config, Balls balls, float * fv, float * temp, float quality, float epsilon, int iter_cap, float merge_range, int check_step);

// Given a Spatial, a Kernel (with its alpha parameter) and an (empty) Balls this assigns modes to every single point in the data matrix contained within the Spatial - after running the Balls object contains the modes, and the output array, aligned with the exemplar index of the data matrix, contains the indices of the modes for each data point (check_step is how many iterations to do between checking if its intersected a hyper-sphere that indicates convergance - exists because that check is much slower than doing a bunch of iterations.) Note that if spatial has an ignored vector then the same vector must be ignored by balls...
void cluster(Spatial spatial, const Kernel * kernel, KernelConfig config, Balls balls, int * out, float quality, float epsilon, int iter_cap, float ident_dist, float merge_range, int check_step);



// Given that a clustering has occured this takes a feature vector and calculates to which cluster it belongs, or returns -1 if its does not belong to any of them...
int assign_cluster(Spatial spatial, const Kernel * kernel, KernelConfig config, Balls balls, float * fv, float * temp, float quality, float epsilon, int iter_cap, int check_step);



// This uses subspace constrained mean shift to project a given feature vector to a manifold. The degrees parameter indicates the degrees of freedom of the surface to converge to - 0 is standard mean shift (Don't do this - its much faster to do the normal thing), 1 will extract lines (1D surfaces), 2 will extract a 2D manifold and so on. Requires the spatial to define the density estimate, plus the feature vector, which is modified in place until it converges - output is in effect a point on the manifold. Also requires four temporaries - grad, for the gradient (Same size as fv); hess, for the hessian (Size of fv squared); eigen_vec, for the eigen vectors of the hessian (Size of fv squared); and eigen_val, for the eigen values (Size of fv). Additionally there are various parameters, for determining accuracy when evaluating the kernels and detecting convergance. The always_hessian parameter should be non-zero for the correct algorithm, but if 0 then it just calculates the hessain once at the start, which saves a lot of time and still works for clean data. Note that this is coded to only work with the unit isotropic Gaussian kernel, hence the kernel is not a parameter...
void manifold(Spatial spatial, int degrees, float * fv, float * grad, float * hess, float * eigen_val, float * eigen_vec, float quality, float epsilon, int iter_cap, int always_hessian);



#endif
