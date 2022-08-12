/*
icc propagate-toz-test.C -o propagate-toz-test.exe -fopenmp -O3
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <alpaka/example/ExampleDefaultAcc.hpp>
#include <alpaka/alpaka.hpp>
#include <functional>
#include <iostream>
#include <cstdint>
#include <chrono>
#include <iomanip>

#define FIXED_RSEED

#ifndef bsize
#define bsize 32
#endif

#ifndef ntrks
#define ntrks 9600
#endif

#define nb    (ntrks/bsize)

#ifndef nevts
#define nevts 100
#endif

#define smear 0.1

#ifndef NITER
#define NITER 5
#endif

#ifndef nlayer
#define nlayer 20
#endif

#ifndef num_streams 
#define num_streams 1
#endif

#ifndef threadsperblocky
#define threadsperblocky 32
#endif

#ifndef threadsperblockx
#define threadsperblockx 1
#endif

#ifndef blockspergridy
#define blockspergridy 20
#endif

#ifndef blockspergridx
#define blockspergridx 15
#endif


ALPAKA_FN_ACC size_t PosInMtrx(size_t i, size_t j, size_t D) {
  return i*D+j;
}

ALPAKA_FN_ACC size_t SymOffsets33(size_t i) {
  const size_t offs[9] = {0, 1, 3, 1, 2, 4, 3, 4, 5};
  return offs[i];
}

ALPAKA_FN_ACC size_t SymOffsets66(size_t i) {
  const size_t offs[36] = {0, 1, 3, 6, 10, 15, 1, 2, 4, 7, 11, 16, 3, 4, 5, 8, 12, 17, 6, 7, 8, 9, 13, 18, 10, 11, 12, 13, 14, 19, 15, 16, 17, 18, 19, 20};
  return offs[i];
}

struct ATRK {
  float par[6];
  float cov[21];
  int q;
//  int hitidx[22];
};

struct AHIT {
  float pos[3];
  float cov[6];
};

struct MP1I {
  int data[1*bsize];
};

struct MP22I {
  int data[22*bsize];
};

struct MP3F {
  float data[3*bsize];
};

struct MP6F {
  float data[6*bsize];
};

struct MP3x3 {
  float data[9*bsize];
};
struct MP3x6 {
  float data[18*bsize];
};

struct MP3x3SF {
  float data[6*bsize];
};

struct MP6x6SF {
  float data[21*bsize];
};

struct MP6x6F {
  float data[36*bsize];
};

struct MPTRK {
  MP6F    par;
  MP6x6SF cov;
  MP1I    q;
//  MP22I   hitidx;
};

struct MPHIT {
  MP3F    pos;
  MP3x3SF cov;
};

float randn(float mu, float sigma) {
  float U1, U2, W, mult;
  static float X1, X2;
  static int call = 0;
  if (call == 1) {
    call = !call;
    return (mu + sigma * (float) X2);
  } do {
    U1 = -1 + ((float) rand () / RAND_MAX) * 2;
    U2 = -1 + ((float) rand () / RAND_MAX) * 2;
    W = pow (U1, 2) + pow (U2, 2);
  }
  while (W >= 1 || W == 0); 
  mult = sqrt ((-2 * log (W)) / W);
  X1 = U1 * mult;
  X2 = U2 * mult; 
  call = !call; 
  return (mu + sigma * (float) X1);
}

ALPAKA_FN_ACC MPTRK* bTk(MPTRK* tracks, size_t ev, size_t ib) {
   return &(tracks[ib + nb*ev]);
}
 
ALPAKA_FN_ACC const MPTRK* bTk(const MPTRK* tracks, size_t ev, size_t ib) {
   return &(tracks[ib + nb*ev]);
}
 
ALPAKA_FN_ACC float q(const MP1I* bq, size_t it){
  return (*bq).data[it];
}

ALPAKA_FN_ACC float par(const MP6F* bpars, size_t it, size_t ipar){
  return (*bpars).data[it + ipar*bsize];
}
ALPAKA_FN_ACC float x    (const MP6F* bpars, size_t it){ return par(bpars, it, 0); }
ALPAKA_FN_ACC float y    (const MP6F* bpars, size_t it){ return par(bpars, it, 1); }
ALPAKA_FN_ACC float z    (const MP6F* bpars, size_t it){ return par(bpars, it, 2); }
ALPAKA_FN_ACC float ipt  (const MP6F* bpars, size_t it){ return par(bpars, it, 3); }
ALPAKA_FN_ACC float phi  (const MP6F* bpars, size_t it){ return par(bpars, it, 4); }
ALPAKA_FN_ACC float theta(const MP6F* bpars, size_t it){ return par(bpars, it, 5); }

ALPAKA_FN_ACC float par(const MPTRK* btracks, size_t it, size_t ipar){
  return par(&(*btracks).par,it,ipar);
}
ALPAKA_FN_ACC float x    (const MPTRK* btracks, size_t it){ return par(btracks, it, 0); }
ALPAKA_FN_ACC float y    (const MPTRK* btracks, size_t it){ return par(btracks, it, 1); }
ALPAKA_FN_ACC float z    (const MPTRK* btracks, size_t it){ return par(btracks, it, 2); }
ALPAKA_FN_ACC float ipt  (const MPTRK* btracks, size_t it){ return par(btracks, it, 3); }
ALPAKA_FN_ACC float phi  (const MPTRK* btracks, size_t it){ return par(btracks, it, 4); }
ALPAKA_FN_ACC float theta(const MPTRK* btracks, size_t it){ return par(btracks, it, 5); }

ALPAKA_FN_ACC float par(const MPTRK* tracks, size_t ev, size_t tk, size_t ipar){
  size_t ib = tk/bsize;
  const MPTRK* btracks = bTk(tracks, ev, ib);
  size_t it = tk % bsize;
  return par(btracks, it, ipar);
}
ALPAKA_FN_ACC float x    (const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 0); }
ALPAKA_FN_ACC float y    (const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 1); }
ALPAKA_FN_ACC float z    (const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 2); }
ALPAKA_FN_ACC float ipt  (const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 3); }
ALPAKA_FN_ACC float phi  (const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 4); }
ALPAKA_FN_ACC float theta(const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 5); }

ALPAKA_FN_ACC void setpar(MP6F* bpars, size_t it, size_t ipar, float val){
  (*bpars).data[it + ipar*bsize] = val;
}
ALPAKA_FN_ACC void setx    (MP6F* bpars, size_t it, float val){ setpar(bpars, it, 0, val); }
ALPAKA_FN_ACC void sety    (MP6F* bpars, size_t it, float val){ setpar(bpars, it, 1, val); }
ALPAKA_FN_ACC void setz    (MP6F* bpars, size_t it, float val){ setpar(bpars, it, 2, val); }
ALPAKA_FN_ACC void setipt  (MP6F* bpars, size_t it, float val){ setpar(bpars, it, 3, val); }
ALPAKA_FN_ACC void setphi  (MP6F* bpars, size_t it, float val){ setpar(bpars, it, 4, val); }
ALPAKA_FN_ACC void settheta(MP6F* bpars, size_t it, float val){ setpar(bpars, it, 5, val); }

ALPAKA_FN_ACC void setpar(MPTRK* btracks, size_t it, size_t ipar, float val){
  setpar(&(*btracks).par,it,ipar,val);
}
ALPAKA_FN_ACC void setx    (MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 0, val); }
ALPAKA_FN_ACC void sety    (MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 1, val); }
ALPAKA_FN_ACC void setz    (MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 2, val); }
ALPAKA_FN_ACC void setipt  (MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 3, val); }
ALPAKA_FN_ACC void setphi  (MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 4, val); }
ALPAKA_FN_ACC void settheta(MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 5, val); }
 
ALPAKA_FN_ACC const MPHIT* bHit(const MPHIT* hits, size_t ev, size_t ib) {
  return &(hits[ib + nb*ev]);
}
ALPAKA_FN_ACC const MPHIT* bHit(const MPHIT* hits, size_t ev, size_t ib,size_t lay) {
  return &(hits[lay + (ib*nlayer) +(ev*nlayer*nb)]);
}

ALPAKA_FN_ACC float pos(const MP3F* hpos, size_t it, size_t ipar){
  return (*hpos).data[it + ipar*bsize];
}
ALPAKA_FN_ACC float x(const MP3F* hpos, size_t it)    { return pos(hpos, it, 0); }
ALPAKA_FN_ACC float y(const MP3F* hpos, size_t it)    { return pos(hpos, it, 1); }
ALPAKA_FN_ACC float z(const MP3F* hpos, size_t it)    { return pos(hpos, it, 2); }

ALPAKA_FN_ACC float pos(const MPHIT* hits, size_t it, size_t ipar){
  return pos(&(*hits).pos,it,ipar);
}
ALPAKA_FN_ACC float x(const MPHIT* hits, size_t it)    { return pos(hits, it, 0); }
ALPAKA_FN_ACC float y(const MPHIT* hits, size_t it)    { return pos(hits, it, 1); }
ALPAKA_FN_ACC float z(const MPHIT* hits, size_t it)    { return pos(hits, it, 2); }

ALPAKA_FN_ACC float pos(const MPHIT* hits, size_t ev, size_t tk, size_t ipar){
  size_t ib = tk/bsize;
  //[DEBUG by Seyong on Dec. 28, 2020] add 4th argument(nlayer-1) to bHit() below.
  const MPHIT* bhits = bHit(hits, ev, ib, nlayer-1);
  size_t it = tk % bsize;
  return pos(bhits,it,ipar);
}
ALPAKA_FN_ACC float x(const MPHIT* hits, size_t ev, size_t tk)    { return pos(hits, ev, tk, 0); }
ALPAKA_FN_ACC float y(const MPHIT* hits, size_t ev, size_t tk)    { return pos(hits, ev, tk, 1); }
ALPAKA_FN_ACC float z(const MPHIT* hits, size_t ev, size_t tk)    { return pos(hits, ev, tk, 2); }

MPTRK* prepareTracks(ATRK inputtrk) {
  MPTRK* result;
  cudaMallocHost((void**)&result,nevts*nb*sizeof(MPTRK));
    //cudaMallocManaged((void**)&result,nevts*nb*sizeof(MPTRK)); //fixme, align?
  //cudaMemAdvise(result,nevts*nb*sizeof(MPTRK),cudaMemAdviseSetPreferredLocation,cudaCpuDeviceId);
  // store in element order for bunches of bsize matrices (a la matriplex)
  for (size_t ie=0;ie<nevts;++ie) {
    for (size_t ib=0;ib<nb;++ib) {
      for (size_t it=0;it<bsize;++it) {
      	//par
      	for (size_t ip=0;ip<6;++ip) {
      	  result[ib + nb*ie].par.data[it + ip*bsize] = (1+smear*randn(0,1))*inputtrk.par[ip];
      	}
      	//cov
      	for (size_t ip=0;ip<21;++ip) {
      	  result[ib + nb*ie].cov.data[it + ip*bsize] = (1+smear*randn(0,1))*inputtrk.cov[ip];
      	}
      	//q
      	result[ib + nb*ie].q.data[it] = inputtrk.q-2*ceil(-0.5 + (float)rand() / RAND_MAX);//fixme check
      }
    }
  }
  return result;
}

MPHIT* prepareHits(AHIT inputhit) {
  MPHIT* result;
  cudaMallocHost((void**)&result,nlayer*nevts*nb*sizeof(MPHIT));
    //cudaMallocManaged((void**)&result,nlayer*nevts*nb*sizeof(MPHIT));  //fixme, align?
  //cudaMemAdvise(result,nlayer*nevts*nb*sizeof(MPHIT),cudaMemAdviseSetPreferredLocation,cudaCpuDeviceId);
  // store in element order for bunches of bsize matrices (a la matriplex)
  for (size_t lay=0;lay<nlayer;++lay) {
    for (size_t ie=0;ie<nevts;++ie) {
      for (size_t ib=0;ib<nb;++ib) {
        for (size_t it=0;it<bsize;++it) {
        	//pos
        	for (size_t ip=0;ip<3;++ip) {
        	  result[lay+nlayer*(ib + nb*ie)].pos.data[it + ip*bsize] = (1+smear*randn(0,1))*inputhit.pos[ip];
        	}
        	//cov
        	for (size_t ip=0;ip<6;++ip) {
        	  result[lay+nlayer*(ib + nb*ie)].cov.data[it + ip*bsize] = (1+smear*randn(0,1))*inputhit.cov[ip];
        	}
        }
      }
    }
  }
  return result;
}

#define N bsize
template< typename TAcc>
inline void ALPAKA_FN_ACC MultHelixPropEndcap(const MP6x6F* A, const MP6x6SF* B, MP6x6F* C, TAcc const & acc) {
  const float* a = A->data; //ASSUME_ALIGNED(a, 64);
  const float* b = B->data; //ASSUME_ALIGNED(b, 64);
  float* c = C->data;       //ASSUME_ALIGNED(c, 64);
  
  using Dim = alpaka::Dim<TAcc>;
  using Idx = alpaka::Idx<TAcc>;
  using Vec = alpaka::Vec<Dim, Idx>;

  Vec const threadIdx    = alpaka::getIdx<alpaka::Block, alpaka::Threads>(acc);
  Vec const threadExtent = alpaka::getWorkDiv<alpaka::Block, alpaka::Threads>(acc);
  
  for (int n = threadIdx[0]; n < N; n+=threadExtent[0])
  {
    c[ 0*N+n] = b[ 0*N+n] + a[ 2*N+n]*b[ 3*N+n] + a[ 3*N+n]*b[ 6*N+n] + a[ 4*N+n]*b[10*N+n] + a[ 5*N+n]*b[15*N+n];
    c[ 1*N+n] = b[ 1*N+n] + a[ 2*N+n]*b[ 4*N+n] + a[ 3*N+n]*b[ 7*N+n] + a[ 4*N+n]*b[11*N+n] + a[ 5*N+n]*b[16*N+n];
    c[ 2*N+n] = b[ 3*N+n] + a[ 2*N+n]*b[ 5*N+n] + a[ 3*N+n]*b[ 8*N+n] + a[ 4*N+n]*b[12*N+n] + a[ 5*N+n]*b[17*N+n];
    c[ 3*N+n] = b[ 6*N+n] + a[ 2*N+n]*b[ 8*N+n] + a[ 3*N+n]*b[ 9*N+n] + a[ 4*N+n]*b[13*N+n] + a[ 5*N+n]*b[18*N+n];
    c[ 4*N+n] = b[10*N+n] + a[ 2*N+n]*b[12*N+n] + a[ 3*N+n]*b[13*N+n] + a[ 4*N+n]*b[14*N+n] + a[ 5*N+n]*b[19*N+n];
    c[ 5*N+n] = b[15*N+n] + a[ 2*N+n]*b[17*N+n] + a[ 3*N+n]*b[18*N+n] + a[ 4*N+n]*b[19*N+n] + a[ 5*N+n]*b[20*N+n];
    c[ 6*N+n] = b[ 1*N+n] + a[ 8*N+n]*b[ 3*N+n] + a[ 9*N+n]*b[ 6*N+n] + a[10*N+n]*b[10*N+n] + a[11*N+n]*b[15*N+n];
    c[ 7*N+n] = b[ 2*N+n] + a[ 8*N+n]*b[ 4*N+n] + a[ 9*N+n]*b[ 7*N+n] + a[10*N+n]*b[11*N+n] + a[11*N+n]*b[16*N+n];
    c[ 8*N+n] = b[ 4*N+n] + a[ 8*N+n]*b[ 5*N+n] + a[ 9*N+n]*b[ 8*N+n] + a[10*N+n]*b[12*N+n] + a[11*N+n]*b[17*N+n];
    c[ 9*N+n] = b[ 7*N+n] + a[ 8*N+n]*b[ 8*N+n] + a[ 9*N+n]*b[ 9*N+n] + a[10*N+n]*b[13*N+n] + a[11*N+n]*b[18*N+n];
    c[10*N+n] = b[11*N+n] + a[ 8*N+n]*b[12*N+n] + a[ 9*N+n]*b[13*N+n] + a[10*N+n]*b[14*N+n] + a[11*N+n]*b[19*N+n];
    c[11*N+n] = b[16*N+n] + a[ 8*N+n]*b[17*N+n] + a[ 9*N+n]*b[18*N+n] + a[10*N+n]*b[19*N+n] + a[11*N+n]*b[20*N+n];
    c[12*N+n] = 0;
    c[13*N+n] = 0;
    c[14*N+n] = 0;
    c[15*N+n] = 0;
    c[16*N+n] = 0;
    c[17*N+n] = 0;
    c[18*N+n] = b[ 6*N+n];
    c[19*N+n] = b[ 7*N+n];
    c[20*N+n] = b[ 8*N+n];
    c[21*N+n] = b[ 9*N+n];
    c[22*N+n] = b[13*N+n];
    c[23*N+n] = b[18*N+n];
    c[24*N+n] = a[26*N+n]*b[ 3*N+n] + a[27*N+n]*b[ 6*N+n] + b[10*N+n] + a[29*N+n]*b[15*N+n];
    c[25*N+n] = a[26*N+n]*b[ 4*N+n] + a[27*N+n]*b[ 7*N+n] + b[11*N+n] + a[29*N+n]*b[16*N+n];
    c[26*N+n] = a[26*N+n]*b[ 5*N+n] + a[27*N+n]*b[ 8*N+n] + b[12*N+n] + a[29*N+n]*b[17*N+n];
    c[27*N+n] = a[26*N+n]*b[ 8*N+n] + a[27*N+n]*b[ 9*N+n] + b[13*N+n] + a[29*N+n]*b[18*N+n];
    c[28*N+n] = a[26*N+n]*b[12*N+n] + a[27*N+n]*b[13*N+n] + b[14*N+n] + a[29*N+n]*b[19*N+n];
    c[29*N+n] = a[26*N+n]*b[17*N+n] + a[27*N+n]*b[18*N+n] + b[19*N+n] + a[29*N+n]*b[20*N+n];
    c[30*N+n] = b[15*N+n];
    c[31*N+n] = b[16*N+n];
    c[32*N+n] = b[17*N+n];
    c[33*N+n] = b[18*N+n];
    c[34*N+n] = b[19*N+n];
    c[35*N+n] = b[20*N+n];
  }
}

template< typename TAcc>
inline void ALPAKA_FN_ACC MultHelixPropTranspEndcap(const MP6x6F* A, const MP6x6F* B, MP6x6SF* C, TAcc const & acc) {
  const float* a = A->data; //ASSUME_ALIGNED(a, 64);
  const float* b = B->data; //ASSUME_ALIGNED(b, 64);
  float* c = C->data;       //ASSUME_ALIGNED(c, 64);
  
  using Dim = alpaka::Dim<TAcc>;
  using Idx = alpaka::Idx<TAcc>;
  using Vec = alpaka::Vec<Dim, Idx>;

  Vec const threadIdx    = alpaka::getIdx<alpaka::Block, alpaka::Threads>(acc);
  Vec const threadExtent = alpaka::getWorkDiv<alpaka::Block, alpaka::Threads>(acc);
  
  for (int n = threadIdx[0]; n < N; n+=threadExtent[0])
  {
    c[ 0*N+n] = b[ 0*N+n] + b[ 2*N+n]*a[ 2*N+n] + b[ 3*N+n]*a[ 3*N+n] + b[ 4*N+n]*a[ 4*N+n] + b[ 5*N+n]*a[ 5*N+n];
    c[ 1*N+n] = b[ 6*N+n] + b[ 8*N+n]*a[ 2*N+n] + b[ 9*N+n]*a[ 3*N+n] + b[10*N+n]*a[ 4*N+n] + b[11*N+n]*a[ 5*N+n];
    c[ 2*N+n] = b[ 7*N+n] + b[ 8*N+n]*a[ 8*N+n] + b[ 9*N+n]*a[ 9*N+n] + b[10*N+n]*a[10*N+n] + b[11*N+n]*a[11*N+n];
    c[ 3*N+n] = b[12*N+n] + b[14*N+n]*a[ 2*N+n] + b[15*N+n]*a[ 3*N+n] + b[16*N+n]*a[ 4*N+n] + b[17*N+n]*a[ 5*N+n];
    c[ 4*N+n] = b[13*N+n] + b[14*N+n]*a[ 8*N+n] + b[15*N+n]*a[ 9*N+n] + b[16*N+n]*a[10*N+n] + b[17*N+n]*a[11*N+n];
    c[ 5*N+n] = 0;
    c[ 6*N+n] = b[18*N+n] + b[20*N+n]*a[ 2*N+n] + b[21*N+n]*a[ 3*N+n] + b[22*N+n]*a[ 4*N+n] + b[23*N+n]*a[ 5*N+n];
    c[ 7*N+n] = b[19*N+n] + b[20*N+n]*a[ 8*N+n] + b[21*N+n]*a[ 9*N+n] + b[22*N+n]*a[10*N+n] + b[23*N+n]*a[11*N+n];
    c[ 8*N+n] = 0;
    c[ 9*N+n] = b[21*N+n];
    c[10*N+n] = b[24*N+n] + b[26*N+n]*a[ 2*N+n] + b[27*N+n]*a[ 3*N+n] + b[28*N+n]*a[ 4*N+n] + b[29*N+n]*a[ 5*N+n];
    c[11*N+n] = b[25*N+n] + b[26*N+n]*a[ 8*N+n] + b[27*N+n]*a[ 9*N+n] + b[28*N+n]*a[10*N+n] + b[29*N+n]*a[11*N+n];
    c[12*N+n] = 0;
    c[13*N+n] = b[27*N+n];
    c[14*N+n] = b[26*N+n]*a[26*N+n] + b[27*N+n]*a[27*N+n] + b[28*N+n] + b[29*N+n]*a[29*N+n];
    c[15*N+n] = b[30*N+n] + b[32*N+n]*a[ 2*N+n] + b[33*N+n]*a[ 3*N+n] + b[34*N+n]*a[ 4*N+n] + b[35*N+n]*a[ 5*N+n];
    c[16*N+n] = b[31*N+n] + b[32*N+n]*a[ 8*N+n] + b[33*N+n]*a[ 9*N+n] + b[34*N+n]*a[10*N+n] + b[35*N+n]*a[11*N+n];
    c[17*N+n] = 0;
    c[18*N+n] = b[33*N+n];
    c[19*N+n] = b[32*N+n]*a[26*N+n] + b[33*N+n]*a[27*N+n] + b[34*N+n] + b[35*N+n]*a[29*N+n];
    c[20*N+n] = b[35*N+n];
  }
}

template< typename TAcc>
inline void ALPAKA_FN_ACC KalmanGainInv(const MP6x6SF* A, const MP3x3SF* B, MP3x3* C, TAcc const & acc) {
  const float* a = (*A).data; //ASSUME_ALIGNED(a, 64);
  const float* b = (*B).data; //ASSUME_ALIGNED(b, 64);
  float* c = (*C).data;       //ASSUME_ALIGNED(c, 64);
  
  using Dim = alpaka::Dim<TAcc>;
  using Idx = alpaka::Idx<TAcc>;
  using Vec = alpaka::Vec<Dim, Idx>;

  Vec const threadIdx    = alpaka::getIdx<alpaka::Block, alpaka::Threads>(acc);
  Vec const threadExtent = alpaka::getWorkDiv<alpaka::Block, alpaka::Threads>(acc);
  
  for (int n = threadIdx[0]; n < N; n+=threadExtent[0])
  {
    double det =
      ((a[0*N+n]+b[0*N+n])*(((a[ 6*N+n]+b[ 3*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[7*N+n]+b[4*N+n]) *(a[7*N+n]+b[4*N+n])))) -
      ((a[1*N+n]+b[1*N+n])*(((a[ 1*N+n]+b[ 1*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[7*N+n]+b[4*N+n]) *(a[2*N+n]+b[2*N+n])))) +
      ((a[2*N+n]+b[2*N+n])*(((a[ 1*N+n]+b[ 1*N+n]) *(a[7*N+n]+b[4*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[6*N+n]+b[3*N+n]))));
    double invdet = 1.0/det;

    c[ 0*N+n] =  invdet*(((a[ 6*N+n]+b[ 3*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[7*N+n]+b[4*N+n]) *(a[7*N+n]+b[4*N+n])));
    c[ 1*N+n] =  -1*invdet*(((a[ 1*N+n]+b[ 1*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[7*N+n]+b[4*N+n])));
    c[ 2*N+n] =  invdet*(((a[ 1*N+n]+b[ 1*N+n]) *(a[7*N+n]+b[4*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[7*N+n]+b[4*N+n])));
    c[ 3*N+n] =  -1*invdet*(((a[ 1*N+n]+b[ 1*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[7*N+n]+b[4*N+n]) *(a[2*N+n]+b[2*N+n])));
    c[ 4*N+n] =  invdet*(((a[ 0*N+n]+b[ 0*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[2*N+n]+b[2*N+n])));
    c[ 5*N+n] =  -1*invdet*(((a[ 0*N+n]+b[ 0*N+n]) *(a[7*N+n]+b[4*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[1*N+n]+b[1*N+n])));
    c[ 6*N+n] =  invdet*(((a[ 1*N+n]+b[ 1*N+n]) *(a[7*N+n]+b[4*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[6*N+n]+b[3*N+n])));
    c[ 7*N+n] =  -1*invdet*(((a[ 0*N+n]+b[ 0*N+n]) *(a[7*N+n]+b[4*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[1*N+n]+b[1*N+n])));
    c[ 8*N+n] =  invdet*(((a[ 0*N+n]+b[ 0*N+n]) *(a[6*N+n]+b[3*N+n])) - ((a[1*N+n]+b[1*N+n]) *(a[1*N+n]+b[1*N+n])));
  }
}

template< typename TAcc>
inline void ALPAKA_FN_ACC KalmanGain(const MP6x6SF* A, const MP3x3* B, MP3x6* C, TAcc const & acc) {
  const float* a = (*A).data; //ASSUME_ALIGNED(a, 64);
  const float* b = (*B).data; //ASSUME_ALIGNED(b, 64);
  float* c = (*C).data;       //ASSUME_ALIGNED(c, 64);
  
  using Dim = alpaka::Dim<TAcc>;
  using Idx = alpaka::Idx<TAcc>;
  using Vec = alpaka::Vec<Dim, Idx>;

  Vec const threadIdx    = alpaka::getIdx<alpaka::Block, alpaka::Threads>(acc);
  Vec const threadExtent = alpaka::getWorkDiv<alpaka::Block, alpaka::Threads>(acc);
  
  for (int n = threadIdx[0]; n < N; n+=threadExtent[0])
  {
    c[ 0*N+n] = a[0*N+n]*b[0*N+n] + a[1*N+n]*b[3*N+n] + a[2*N+n]*b[6*N+n];
    c[ 1*N+n] = a[0*N+n]*b[1*N+n] + a[1*N+n]*b[4*N+n] + a[2*N+n]*b[7*N+n];
    c[ 2*N+n] = a[0*N+n]*b[2*N+n] + a[1*N+n]*b[5*N+n] + a[2*N+n]*b[8*N+n];
    c[ 3*N+n] = a[1*N+n]*b[0*N+n] + a[6*N+n]*b[3*N+n] + a[7*N+n]*b[6*N+n];
    c[ 4*N+n] = a[1*N+n]*b[1*N+n] + a[6*N+n]*b[4*N+n] + a[7*N+n]*b[7*N+n];
    c[ 5*N+n] = a[1*N+n]*b[2*N+n] + a[6*N+n]*b[5*N+n] + a[7*N+n]*b[8*N+n];
    c[ 6*N+n] = a[2*N+n]*b[0*N+n] + a[7*N+n]*b[3*N+n] + a[11*N+n]*b[6*N+n];
    c[ 7*N+n] = a[2*N+n]*b[1*N+n] + a[7*N+n]*b[4*N+n] + a[11*N+n]*b[7*N+n];
    c[ 8*N+n] = a[2*N+n]*b[2*N+n] + a[7*N+n]*b[5*N+n] + a[11*N+n]*b[8*N+n];
    c[ 9*N+n] = a[3*N+n]*b[0*N+n] + a[8*N+n]*b[3*N+n] + a[12*N+n]*b[6*N+n];
    c[ 10*N+n] = a[3*N+n]*b[1*N+n] + a[8*N+n]*b[4*N+n] + a[12*N+n]*b[7*N+n];
    c[ 11*N+n] = a[3*N+n]*b[2*N+n] + a[8*N+n]*b[5*N+n] + a[12*N+n]*b[8*N+n];
    c[ 12*N+n] = a[4*N+n]*b[0*N+n] + a[9*N+n]*b[3*N+n] + a[13*N+n]*b[6*N+n];
    c[ 13*N+n] = a[4*N+n]*b[1*N+n] + a[9*N+n]*b[4*N+n] + a[13*N+n]*b[7*N+n];
    c[ 14*N+n] = a[4*N+n]*b[2*N+n] + a[9*N+n]*b[5*N+n] + a[13*N+n]*b[8*N+n];
    c[ 15*N+n] = a[5*N+n]*b[0*N+n] + a[10*N+n]*b[3*N+n] + a[14*N+n]*b[6*N+n];
    c[ 16*N+n] = a[5*N+n]*b[1*N+n] + a[10*N+n]*b[4*N+n] + a[14*N+n]*b[7*N+n];
    c[ 17*N+n] = a[5*N+n]*b[2*N+n] + a[10*N+n]*b[5*N+n] + a[14*N+n]*b[8*N+n];
  }
}

template< typename TAcc>
inline void ALPAKA_FN_ACC KalmanUpdate(MP6x6SF* trkErr, MP6F* inPar, const MP3x3SF* hitErr, const MP3F* msP, MP3x3 *inverse_temp, MP3x6 *kGain, MP6x6SF *newErr, TAcc const & acc){

  KalmanGainInv(trkErr,hitErr,inverse_temp,acc);
  KalmanGain(trkErr,inverse_temp,kGain,acc);

  using Dim = alpaka::Dim<TAcc>;
  using Idx = alpaka::Idx<TAcc>;
  using Vec = alpaka::Vec<Dim, Idx>;

  Vec const threadIdx    = alpaka::getIdx<alpaka::Block, alpaka::Threads>(acc);
  Vec const threadExtent = alpaka::getWorkDiv<alpaka::Block, alpaka::Threads>(acc);
  
  for (size_t it=threadIdx[0];it<bsize;it+=threadExtent[0]) {
    const float xin = x(inPar,it);
    const float yin = y(inPar,it);
    const float zin = z(inPar,it);
    const float ptin = 1./ipt(inPar,it);
    const float phiin = phi(inPar,it);
    const float thetain = theta(inPar,it);
    const float xout = x(msP,it);
    const float yout = y(msP,it);
    const float zout = z(msP,it);

    float xnew = xin + (kGain->data[0*bsize+it]*(xout-xin)) +(kGain->data[1*bsize+it]*(yout-yin));
    float ynew = yin + (kGain->data[3*bsize+it]*(xout-xin)) +(kGain->data[4*bsize+it]*(yout-yin));
    float znew = zin + (kGain->data[6*bsize+it]*(xout-xin)) +(kGain->data[7*bsize+it]*(yout-yin));
    float ptnew = ptin + (kGain->data[9*bsize+it]*(xout-xin)) +(kGain->data[10*bsize+it]*(yout-yin));
    float phinew = phiin + (kGain->data[12*bsize+it]*(xout-xin)) +(kGain->data[13*bsize+it]*(yout-yin));
    float thetanew = thetain + (kGain->data[15*bsize+it]*(xout-xin)) +(kGain->data[16*bsize+it]*(yout-yin));

    newErr->data[0*bsize+it] = trkErr->data[0*bsize+it] - (kGain->data[0*bsize+it]*trkErr->data[0*bsize+it]+kGain->data[1*bsize+it]*trkErr->data[1*bsize+it]+kGain->data[2*bsize+it]*trkErr->data[2*bsize+it]);
    newErr->data[1*bsize+it] = trkErr->data[1*bsize+it] - (kGain->data[0*bsize+it]*trkErr->data[1*bsize+it]+kGain->data[1*bsize+it]*trkErr->data[6*bsize+it]+kGain->data[2*bsize+it]*trkErr->data[7*bsize+it]);
    newErr->data[2*bsize+it] = trkErr->data[2*bsize+it] - (kGain->data[0*bsize+it]*trkErr->data[2*bsize+it]+kGain->data[1*bsize+it]*trkErr->data[7*bsize+it]+kGain->data[2*bsize+it]*trkErr->data[11*bsize+it]);
    newErr->data[3*bsize+it] = trkErr->data[3*bsize+it] - (kGain->data[0*bsize+it]*trkErr->data[3*bsize+it]+kGain->data[1*bsize+it]*trkErr->data[8*bsize+it]+kGain->data[2*bsize+it]*trkErr->data[12*bsize+it]);
    newErr->data[4*bsize+it] = trkErr->data[4*bsize+it] - (kGain->data[0*bsize+it]*trkErr->data[4*bsize+it]+kGain->data[1*bsize+it]*trkErr->data[9*bsize+it]+kGain->data[2*bsize+it]*trkErr->data[13*bsize+it]);
    newErr->data[5*bsize+it] = trkErr->data[5*bsize+it] - (kGain->data[0*bsize+it]*trkErr->data[5*bsize+it]+kGain->data[1*bsize+it]*trkErr->data[10*bsize+it]+kGain->data[2*bsize+it]*trkErr->data[14*bsize+it]);

    newErr->data[6*bsize+it] = trkErr->data[6*bsize+it] - (kGain->data[3*bsize+it]*trkErr->data[1*bsize+it]+kGain->data[4*bsize+it]*trkErr->data[6*bsize+it]+kGain->data[5*bsize+it]*trkErr->data[7*bsize+it]);
    newErr->data[7*bsize+it] = trkErr->data[7*bsize+it] - (kGain->data[3*bsize+it]*trkErr->data[2*bsize+it]+kGain->data[4*bsize+it]*trkErr->data[7*bsize+it]+kGain->data[5*bsize+it]*trkErr->data[11*bsize+it]);
    newErr->data[8*bsize+it] = trkErr->data[8*bsize+it] - (kGain->data[3*bsize+it]*trkErr->data[3*bsize+it]+kGain->data[4*bsize+it]*trkErr->data[8*bsize+it]+kGain->data[5*bsize+it]*trkErr->data[12*bsize+it]);
    newErr->data[9*bsize+it] = trkErr->data[9*bsize+it] - (kGain->data[3*bsize+it]*trkErr->data[4*bsize+it]+kGain->data[4*bsize+it]*trkErr->data[9*bsize+it]+kGain->data[5*bsize+it]*trkErr->data[13*bsize+it]);
    newErr->data[10*bsize+it] = trkErr->data[10*bsize+it] - (kGain->data[3*bsize+it]*trkErr->data[5*bsize+it]+kGain->data[4*bsize+it]*trkErr->data[10*bsize+it]+kGain->data[5*bsize+it]*trkErr->data[14*bsize+it]);

    newErr->data[11*bsize+it] = trkErr->data[11*bsize+it] - (kGain->data[6*bsize+it]*trkErr->data[2*bsize+it]+kGain->data[7*bsize+it]*trkErr->data[7*bsize+it]+kGain->data[8*bsize+it]*trkErr->data[11*bsize+it]);
    newErr->data[12*bsize+it] = trkErr->data[12*bsize+it] - (kGain->data[6*bsize+it]*trkErr->data[3*bsize+it]+kGain->data[7*bsize+it]*trkErr->data[8*bsize+it]+kGain->data[8*bsize+it]*trkErr->data[12*bsize+it]);
    newErr->data[13*bsize+it] = trkErr->data[13*bsize+it] - (kGain->data[6*bsize+it]*trkErr->data[4*bsize+it]+kGain->data[7*bsize+it]*trkErr->data[9*bsize+it]+kGain->data[8*bsize+it]*trkErr->data[13*bsize+it]);
    newErr->data[14*bsize+it] = trkErr->data[14*bsize+it] - (kGain->data[6*bsize+it]*trkErr->data[5*bsize+it]+kGain->data[7*bsize+it]*trkErr->data[10*bsize+it]+kGain->data[8*bsize+it]*trkErr->data[14*bsize+it]);

    newErr->data[15*bsize+it] = trkErr->data[15*bsize+it] - (kGain->data[9*bsize+it]*trkErr->data[3*bsize+it]+kGain->data[10*bsize+it]*trkErr->data[8*bsize+it]+kGain->data[11*bsize+it]*trkErr->data[12*bsize+it]);
    newErr->data[16*bsize+it] = trkErr->data[16*bsize+it] - (kGain->data[9*bsize+it]*trkErr->data[4*bsize+it]+kGain->data[10*bsize+it]*trkErr->data[9*bsize+it]+kGain->data[11*bsize+it]*trkErr->data[13*bsize+it]);
    newErr->data[17*bsize+it] = trkErr->data[17*bsize+it] - (kGain->data[9*bsize+it]*trkErr->data[5*bsize+it]+kGain->data[10*bsize+it]*trkErr->data[10*bsize+it]+kGain->data[11*bsize+it]*trkErr->data[14*bsize+it]);

    newErr->data[18*bsize+it] = trkErr->data[18*bsize+it] - (kGain->data[12*bsize+it]*trkErr->data[4*bsize+it]+kGain->data[13*bsize+it]*trkErr->data[9*bsize+it]+kGain->data[14*bsize+it]*trkErr->data[13*bsize+it]);
    newErr->data[19*bsize+it] = trkErr->data[19*bsize+it] - (kGain->data[12*bsize+it]*trkErr->data[5*bsize+it]+kGain->data[13*bsize+it]*trkErr->data[10*bsize+it]+kGain->data[14*bsize+it]*trkErr->data[14*bsize+it]);

    newErr->data[20*bsize+it] = trkErr->data[20*bsize+it] - (kGain->data[15*bsize+it]*trkErr->data[5*bsize+it]+kGain->data[16*bsize+it]*trkErr->data[10*bsize+it]+kGain->data[17*bsize+it]*trkErr->data[14*bsize+it]);

    setx(inPar,it,xnew );
    sety(inPar,it,ynew );
    setz(inPar,it,znew);
    setipt(inPar,it, ptnew);
    setphi(inPar,it, phinew);
    settheta(inPar,it, thetanew);
  }
  (*trkErr) = *newErr;
 }

const float kfact= 100/3.8;
template< typename TAcc>
inline void ALPAKA_FN_ACC propagateToZ(const MP6x6SF* inErr, const MP6F* inPar, const MP1I* inChg, const MP3F* msP,
	                MP6x6SF* outErr, MP6F* outPar, MP6x6F* errorProp, MP6x6F* temp, TAcc const & acc) {
  using Dim = alpaka::Dim<TAcc>;
  using Idx = alpaka::Idx<TAcc>;
  using Vec = alpaka::Vec<Dim, Idx>;

  Vec const threadIdx    = alpaka::getIdx<alpaka::Block, alpaka::Threads>(acc);
  Vec const threadExtent = alpaka::getWorkDiv<alpaka::Block, alpaka::Threads>(acc);
    
  for (size_t it=threadIdx[0];it<bsize;it+=threadExtent[0]) {	
    const float zout = z(msP,it);
    const float k = q(inChg,it)*kfact;//100/3.8;
    const float deltaZ = zout - z(inPar,it);
    const float pt = 1./ipt(inPar,it);
    const float cosP = cosf(phi(inPar,it));
    const float sinP = sinf(phi(inPar,it));
    const float cosT = cosf(theta(inPar,it));
    const float sinT = sinf(theta(inPar,it));
    const float pxin = cosP*pt;
    const float pyin = sinP*pt;
    const float icosT = 1.0/cosT;
    const float icosTk = icosT/k;
    const float alpha = deltaZ*sinT*ipt(inPar,it)*icosTk;
    //const float alpha = deltaZ*sinT*ipt(inPar,it)/(cosT*k);
    const float sina = sinf(alpha); // this can be approximated;
    const float cosa = cosf(alpha); // this can be approximated;
    setx(outPar,it, x(inPar,it) + k*(pxin*sina - pyin*(1.-cosa)) );
    sety(outPar,it, y(inPar,it) + k*(pyin*sina + pxin*(1.-cosa)) );
    setz(outPar,it,zout);
    setipt(outPar,it, ipt(inPar,it));
    setphi(outPar,it, phi(inPar,it)+alpha );
    settheta(outPar,it, theta(inPar,it) );
    
    const float sCosPsina = sinf(cosP*sina);
    const float cCosPsina = cosf(cosP*sina);
    
    for (size_t i=0;i<6;++i) errorProp->data[bsize*PosInMtrx(i,i,6) + it] = 1.;
    errorProp->data[bsize*PosInMtrx(0,2,6) + it] = cosP*sinT*(sinP*cosa*sCosPsina-cosa)*icosT;
    errorProp->data[bsize*PosInMtrx(0,3,6) + it] = cosP*sinT*deltaZ*cosa*(1.-sinP*sCosPsina)*(icosT*pt)-k*(cosP*sina-sinP*(1.-cCosPsina))*(pt*pt);
    errorProp->data[bsize*PosInMtrx(0,4,6) + it] = (k*pt)*(-sinP*sina+sinP*sinP*sina*sCosPsina-cosP*(1.-cCosPsina));
    errorProp->data[bsize*PosInMtrx(0,5,6) + it] = cosP*deltaZ*cosa*(1.-sinP*sCosPsina)*(icosT*icosT);
    errorProp->data[bsize*PosInMtrx(1,2,6) + it] = cosa*sinT*(cosP*cosP*sCosPsina-sinP)*icosT;
    errorProp->data[bsize*PosInMtrx(1,3,6) + it] = sinT*deltaZ*cosa*(cosP*cosP*sCosPsina+sinP)*(icosT*pt)-k*(sinP*sina+cosP*(1.-cCosPsina))*(pt*pt);
    errorProp->data[bsize*PosInMtrx(1,4,6) + it] = (k*pt)*(-sinP*(1.-cCosPsina)-sinP*cosP*sina*sCosPsina+cosP*sina);
    errorProp->data[bsize*PosInMtrx(1,5,6) + it] = deltaZ*cosa*(cosP*cosP*sCosPsina+sinP)*(icosT*icosT);
    errorProp->data[bsize*PosInMtrx(4,2,6) + it] = -ipt(inPar,it)*sinT*(icosTk);
    errorProp->data[bsize*PosInMtrx(4,3,6) + it] = sinT*deltaZ*(icosTk);
    errorProp->data[bsize*PosInMtrx(4,5,6) + it] = ipt(inPar,it)*deltaZ*(icosT*icosTk);
  }
  //
  MultHelixPropEndcap(errorProp, inErr, temp,acc);
  MultHelixPropTranspEndcap(errorProp, temp, outErr,acc);
}



template< typename TAcc>
void ALPAKA_FN_ACC alpaka_kernel(TAcc const & acc, MPTRK* trk, MPHIT* hit, MPTRK* outtrk){
    using Dim = alpaka::Dim<TAcc>;
    using Idx = alpaka::Idx<TAcc>;
    using Vec = alpaka::Vec<Dim, Idx>;

    Vec const threadIdx    = alpaka::getIdx<alpaka::Block, alpaka::Threads>(acc);
    Vec const threadExtent = alpaka::getWorkDiv<alpaka::Block, alpaka::Threads>(acc);
    Vec const blockIdx    = alpaka::getIdx<alpaka::Grid, alpaka::Blocks>(acc);
    Vec const blockExtent = alpaka::getWorkDiv<alpaka::Grid, alpaka::Blocks>(acc);
    
    auto & errorProp = alpaka::declareSharedVar<MP6x6F,__COUNTER__>(acc);
    auto & temp = alpaka::declareSharedVar<MP6x6F,__COUNTER__>(acc);
    auto & inverse_temp = alpaka::declareSharedVar<MP3x3,__COUNTER__>(acc);
    auto & kGain = alpaka::declareSharedVar<MP3x6,__COUNTER__>(acc);
    auto & newErr = alpaka::declareSharedVar<MP6x6SF,__COUNTER__>(acc);
    
   for (size_t ie=blockIdx[0];ie<nevts;ie+=blockExtent[0]) { // loop over events
     for (size_t ib=blockIdx[1];ib<nb;ib+=blockExtent[1]) { // loop over bunches of tracks

       const MPTRK* btracks = bTk(trk, ie, ib);
       MPTRK* obtracks = bTk(outtrk, ie, ib);
       for( size_t layer=0; layer<nlayer;++layer){
          const MPHIT* bhits = bHit(hit, ie, ib,layer);
 	        //struct MP6x6F errorProp, temp;
          propagateToZ(&(*btracks).cov, &(*btracks).par, &(*btracks).q, &(*bhits).pos, &(*obtracks).cov, &(*obtracks).par,
	        &errorProp, &temp, acc); // vectorized function
          KalmanUpdate(&(*obtracks).cov,&(*obtracks).par,&(*bhits).cov,&(*bhits).pos,&inverse_temp, &kGain, &(newErr), acc);
       }
     }
   }
}

inline void transferHit(MPHIT* hit, MPHIT* hit_dev){
    cudaMemcpyAsync(hit_dev,hit,nlayer*nevts*nb*sizeof(MPHIT), cudaMemcpyHostToDevice);
}
inline void transfer_back(MPTRK* trk, MPTRK* trk_host){
  cudaMemcpyAsync(trk_host, trk, nevts*nb*sizeof(MPTRK), cudaMemcpyDeviceToHost);
}

inline void transferTrk(MPTRK* trk, MPTRK* trk_dev){
  cudaMemcpyAsync(trk_dev, trk, nevts*nb*sizeof(MPTRK), cudaMemcpyHostToDevice);
}

int main (int argc, char* argv[]) {

  using Dim = alpaka::DimInt<2u>;
  using Idx = std::size_t;
  
  // set type of accelerator
  using Acc = alpaka::AccGpuCudaRt<Dim, Idx>;
  
  // select device
  auto const devAcc = alpaka::getDevByIdx<Acc>(0u);
  
  // cudastreamcreate
  using QueueAcc = alpaka::QueueCudaRtNonBlocking;

  using DevHost = alpaka::DevCpu;
  auto const devHost = alpaka::getDevByIdx<DevHost>(0u);
  std::cout << "Using alpaka accelerator: " << alpaka::getAccName<Acc>() << std::endl;
  
  //make queue on device
  QueueAcc accQueue(devAcc);

  using Vec = alpaka::Vec<Dim,Idx>;

  Vec const elementsPerThread(Vec::all(static_cast<Idx>(1)));
  Vec const threadsPerBlock(static_cast<Idx>(threadsperblocky),static_cast<Idx>(threadsperblockx));//,static_cast<Idx>(1));
  Vec const blocksPerGrid(static_cast<Idx>(blockspergridy),static_cast<Idx>(blockspergridx));//,static_cast<Idx>(1));

  using WorkDiv = alpaka::WorkDivMembers<Dim, Idx>;

  WorkDiv const workDiv(blocksPerGrid, threadsPerBlock,elementsPerThread);

   int itr;
   ATRK inputtrk = {
     {-12.806846618652344, -7.723824977874756, 38.13014221191406,0.23732035065189902, -2.613372802734375, 0.35594117641448975},
     {6.290299552347278e-07,4.1375109560704004e-08,7.526661534029699e-07,2.0973730840978533e-07,1.5431574240665213e-07,9.626245400795597e-08,-2.804026640189443e-06,
      6.219111130687595e-06,2.649119409845118e-07,0.00253512163402557,-2.419662877381737e-07,4.3124190760040646e-07,3.1068903991780678e-09,0.000923913115050627,
      0.00040678296006807003,-7.755406890332818e-07,1.68539375883925e-06,6.676875566525437e-08,0.0008420574605423793,7.356584799406111e-05,0.0002306247719158348},
     1
   };

   AHIT inputhit = {
     {-20.7824649810791, -12.24150276184082, 57.8067626953125},
     {2.545517190810642e-06,-2.6680759219743777e-06,2.8030024168401724e-06,0.00014160551654640585,0.00012282167153898627,11.385087966918945}
   };

   printf("track in pos: %f, %f, %f \n", inputtrk.par[0], inputtrk.par[1], inputtrk.par[2]);
   printf("track in cov: %.2e, %.2e, %.2e \n", inputtrk.cov[SymOffsets66(PosInMtrx(0,0,6))],
	                                       inputtrk.cov[SymOffsets66(PosInMtrx(1,1,6))],
	                                       inputtrk.cov[SymOffsets66(PosInMtrx(2,2,6))]);
   printf("hit in pos: %f %f %f \n", inputhit.pos[0], inputhit.pos[1], inputhit.pos[2]);
   
   printf("produce nevts=%i ntrks=%i smearing by=%f \n", nevts, ntrks, smear);
   printf("NITER=%d\n", NITER);

   long setup_start, setup_stop;
   struct timeval timecheck;

   gettimeofday(&timecheck, NULL);
   setup_start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
   
#ifdef FIXED_RSEED
   srand(1);
#endif
   
   MPTRK* trk = prepareTracks(inputtrk);
   MPHIT* hit = prepareHits(inputhit);
   MPTRK* outtrk;
   
   cudaMallocHost((void**)&outtrk,nevts*nb*sizeof(MPTRK)); 
   //cudaMallocManaged((void**)&outtrk,nevts*nb*sizeof(MPTRK));
   MPTRK* trk_dev;
   MPHIT* hit_dev;
   
   cudaMalloc((MPTRK**)&trk_dev,nevts*nb*sizeof(MPTRK));
   cudaMalloc((MPHIT**)&hit_dev,nlayer*nevts*nb*sizeof(MPHIT));
   MPTRK* outtrk_dev;
   cudaMalloc((MPTRK**)&outtrk_dev,nevts*nb*sizeof(MPTRK));

   gettimeofday(&timecheck, NULL);
   setup_stop = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;

   printf("done preparing!\n");

   printf("Size of struct MPTRK trk[] = %ld\n", nevts*nb*sizeof(struct MPTRK));
   printf("Size of struct MPTRK outtrk[] = %ld\n", nevts*nb*sizeof(struct MPTRK));
   printf("Size of struct struct MPHIT hit[] = %ld\n", nlayer*nevts*nb*sizeof(struct MPHIT));

   auto wall_start = std::chrono::high_resolution_clock::now();

   for(itr=0; itr<NITER; itr++) {
   transferTrk(trk,trk_dev);
   transferHit(hit,hit_dev);
   
   auto wall_start = std::chrono::high_resolution_clock::now();
    alpaka::exec<Acc>( accQueue,workDiv,
     [] ALPAKA_FN_ACC (Acc const & acc, MPTRK* trk_dev, MPHIT* hit_dev, MPTRK* outtrk_dev){
     alpaka_kernel(acc, trk_dev,hit_dev,outtrk_dev);
     }, trk_dev, hit_dev, outtrk_dev);
     
    alpaka::wait(accQueue);
   auto wall_stop = std::chrono::high_resolution_clock::now();
   auto wall_diff = wall_stop - wall_start;
   auto wall_time = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(wall_diff).count()) / 1e6;
   printf("without transfer=%e (s)\n", wall_time/(nevts*ntrks));
     
     transfer_back(outtrk_dev,outtrk);
     
  } //end of itr loop
  
   cudaDeviceSynchronize(); 
   auto wall_stop = std::chrono::high_resolution_clock::now();
   auto wall_diff = wall_stop - wall_start;
   auto wall_time = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(wall_diff).count()) / 1e6;

   
   printf("setup time time=%f (s)\n", (setup_stop-setup_start)*0.001);
   printf("done ntracks=%i tot time=%f (s) time/trk=%e (s)\n", nevts*ntrks*int(NITER), wall_time, wall_time/(nevts*ntrks*int(NITER)));
   printf("formatted %i %i %i %i %i %f 0 %f %i\n",int(NITER),nevts, ntrks, bsize, nb, wall_time, (setup_stop-setup_start)*0.001, 3);


   float avgx = 0, avgy = 0, avgz = 0;
   float avgpt = 0, avgphi = 0, avgtheta = 0;
   float avgdx = 0, avgdy = 0, avgdz = 0;
   for (size_t ie=0;ie<nevts;++ie) {
     for (size_t it=0;it<ntrks;++it) {
       float x_ = x(outtrk,ie,it);
       float y_ = y(outtrk,ie,it);
       float z_ = z(outtrk,ie,it);
       float pt_ = 1./ipt(outtrk,ie,it);
       float phi_ = phi(outtrk,ie,it);
       float theta_ = theta(outtrk,ie,it);
       avgpt += pt_;
       avgphi += phi_;
       avgtheta += theta_;
       avgx += x_;
       avgy += y_;
       avgz += z_;
       float hx_ = x(hit,ie,it);
       float hy_ = y(hit,ie,it);
       float hz_ = z(hit,ie,it);
       avgdx += (x_-hx_)/x_;
       avgdy += (y_-hy_)/y_;
       avgdz += (z_-hz_)/z_;
     }
   }
   avgpt = avgpt/float(nevts*ntrks);
   avgphi = avgphi/float(nevts*ntrks);
   avgtheta = avgtheta/float(nevts*ntrks);
   avgx = avgx/float(nevts*ntrks);
   avgy = avgy/float(nevts*ntrks);
   avgz = avgz/float(nevts*ntrks);
   avgdx = avgdx/float(nevts*ntrks);
   avgdy = avgdy/float(nevts*ntrks);
   avgdz = avgdz/float(nevts*ntrks);

   float stdx = 0, stdy = 0, stdz = 0;
   float stddx = 0, stddy = 0, stddz = 0;
   for (size_t ie=0;ie<nevts;++ie) {
     for (size_t it=0;it<ntrks;++it) {
       float x_ = x(outtrk,ie,it);
       float y_ = y(outtrk,ie,it);
       float z_ = z(outtrk,ie,it);
       stdx += (x_-avgx)*(x_-avgx);
       stdy += (y_-avgy)*(y_-avgy);
       stdz += (z_-avgz)*(z_-avgz);
       float hx_ = x(hit,ie,it);
       float hy_ = y(hit,ie,it);
       float hz_ = z(hit,ie,it);
       stddx += ((x_-hx_)/x_-avgdx)*((x_-hx_)/x_-avgdx);
       stddy += ((y_-hy_)/y_-avgdy)*((y_-hy_)/y_-avgdy);
       stddz += ((z_-hz_)/z_-avgdz)*((z_-hz_)/z_-avgdz);
     }
   }

   stdx = sqrtf(stdx/float(nevts*ntrks));
   stdy = sqrtf(stdy/float(nevts*ntrks));
   stdz = sqrtf(stdz/float(nevts*ntrks));
   stddx = sqrtf(stddx/float(nevts*ntrks));
   stddy = sqrtf(stddy/float(nevts*ntrks));
   stddz = sqrtf(stddz/float(nevts*ntrks));

   printf("track x avg=%f std/avg=%f\n", avgx, fabs(stdx/avgx));
   printf("track y avg=%f std/avg=%f\n", avgy, fabs(stdy/avgy));
   printf("track z avg=%f std/avg=%f\n", avgz, fabs(stdz/avgz));
   printf("track dx/x avg=%f std=%f\n", avgdx, stddx);
   printf("track dy/y avg=%f std=%f\n", avgdy, stddy);
   printf("track dz/z avg=%f std=%f\n", avgdz, stddz);
   printf("track pt avg=%f\n", avgpt);
   printf("track phi avg=%f\n", avgphi);
   printf("track theta avg=%f\n", avgtheta);

//   free(trk);
//   free(hit);
//   free(outtrk);

   return 0;
}
