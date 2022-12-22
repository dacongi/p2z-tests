

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <iostream>
#include <chrono>
#include <iomanip>


// kokkos
#include <Kokkos_Core.hpp>
// our typedefs curtisy of the examples
#include "kokkos_config.h"
#include "ptoz_data.h"

#define AVAL 5.
#define BVAL 6.
#define TOL 0.0001


void prepareTracks(ATRK inputtrk, MPTRK &intrk);
void prepareHits(AHIT inputhit, MPHIT &inhit);

void propagateToZ(const CBTRK &inTrks, const CBHIT &inHits, CBTRK &outTrks, 
                  ViewMatrixCB &errorProp, ViewMatrixCB &temp);
void update(const CBTRK &inTrks, const CBHIT &inHits);
void averageOutputs(MPTRK &outtrk, MPHIT &hit);

void invert_33(const ViewMatrixCB &mat, int batch);
KOKKOS_INLINE_FUNCTION
void gemm(const ViewMatrixCB &A, const ViewMatrixCB &B, const ViewMatrixCB &C, int batch, int rows_a, int cols_a, int cols_b);
void gemm_T(const ViewMatrixCB &A, const ViewMatrixCB &B, const ViewMatrixCB &C, int batch, int rows_a, int cols_a, int cols_b);

double get_time();

void read_input(int argc, char* argv[], 
                int &N, int &M, int &S, int &nrepeat, 
                int &batch_size, int &nbatches, bool &use_batches);
float randn(float mu, float sigma);


int main( int argc, char* argv[] )
{

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
  printf("bsize=%d\n", bsize);


  Kokkos::initialize( argc, argv );
  {

  double start_t, prep_t, p2z_t, end_t;

  start_t = get_time();

  CBTRK all_tracks;
  new(&(all_tracks.cov))  ViewMatrixCB("cov", nevts*nb, 6, 6, bsize); // 6x6 symmetric batch matrix
  new(&(all_tracks.par))  ViewVectorCB("par", nevts*nb, 6, bsize);    // batch of len 6 vectors
  new(&(all_tracks.q))    ViewIntCB("q", nevts*nb, bsize);            // bsize array of int
  CBHIT all_hits;
  new(&(all_hits.cov))  ViewMatrixCB("cov", nevts*nb, 3, 3, bsize); // 3x3 symmetric batch matrix
  new(&(all_hits.pos))  ViewVectorCB("pos", nevts*nb, 3, bsize);    // batch of len 6 vectors
  CBTRK all_out;
  new(&(all_out.cov))  ViewMatrixCB("cov", nevts*nb, 6, 6, bsize); // 6x6 symmetric batch matrix
  new(&(all_out.par))  ViewVectorCB("par", nevts*nb, 6, bsize);    // batch of len 6 vectors
  new(&(all_out.q))    ViewIntCB("q", nevts*nb, bsize);            // bsize array of int
  MPTRK trk; 
  trk.cov = Kokkos::create_mirror_view(all_tracks.cov);
  trk.par = Kokkos::create_mirror_view(all_tracks.par);
  trk.q = Kokkos::create_mirror_view(all_tracks.q);
  prepareTracks(inputtrk, trk);
  MPHIT hit;
  hit.cov = Kokkos::create_mirror_view(all_hits.cov);
  hit.pos = Kokkos::create_mirror_view(all_hits.pos);
  prepareHits(inputhit, hit);
  MPTRK outtrk;
  outtrk.cov = Kokkos::create_mirror_view(all_out.cov);
  outtrk.par = Kokkos::create_mirror_view(all_out.par);
  outtrk.q = Kokkos::create_mirror_view(all_out.q);

  ViewMatrixCB errorProp("ep", nevts*nb, 6, 6, bsize),
               temp("temp", nevts*nb, 6, 6, bsize);


  printf("done preparing!\n");

  prep_t = get_time();
  
  auto wall_start = std::chrono::high_resolution_clock::now();

  for(itr=0; itr<NITER; itr++) {
      Kokkos::deep_copy(all_tracks.cov, trk.cov);
      Kokkos::deep_copy(all_tracks.par, trk.par);
      Kokkos::deep_copy(all_tracks.q, trk.q);
      Kokkos::deep_copy(all_hits.cov, hit.cov);
      Kokkos::deep_copy(all_hits.pos, hit.pos);
  
      propagateToZ(all_tracks, all_hits, all_out, errorProp, temp);
      update(all_out, all_hits);

      Kokkos::deep_copy(outtrk.cov, all_out.cov);
      Kokkos::deep_copy(outtrk.par, all_out.par);
      Kokkos::deep_copy(outtrk.q, all_out.q);

  } // end of itr loop
  Kokkos::fence();
  auto wall_stop = std::chrono::high_resolution_clock::now();

  p2z_t = get_time();


  averageOutputs(outtrk, hit);

  end_t = get_time();

  printf("done ntracks =%i \n", nevts*ntrks);
  printf("done niter   =%i \n", NITER);
  printf("total tracks =%i \n", nevts*ntrks*NITER);
  printf("Total time   =%f \n", end_t - start_t);
  printf("Setup time =%f \n", prep_t - start_t);
  printf("p2z time     =%f \n", p2z_t - prep_t);
  printf("Time / track =%e \n", (p2z_t - prep_t) / (float)(nevts*ntrks*NITER) );

  auto wall_diff = wall_stop - wall_start;
  auto wall_time = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(wall_diff).count()) / 1e6;
  printf("setup time time=%f (s)\n", (prep_t - start_t));
  printf("done ntracks=%i tot time=%f (s) time/trk=%e (s)\n", nevts*ntrks*int(NITER), wall_time, wall_time/(nevts*ntrks*NITER));
  printf("formatted %i %i %i %i %i %f 0 %f %i\n",int(NITER),nevts, ntrks, bsize, nb, wall_time, (prep_t - start_t), -1);

  // for (size_t ie=0;ie<nevts;++ie) {
  //   for (size_t ib=0;ib<nb;++ib) {
  //     delete &(outtrk[ib + nb*ie].par);
      // delete &(outtrk[ib + nb*ie].cov);
      // delete &(outtrk[ib + nb*ie].q);
      // delete &(outtrk[ib + nb*ie].hitidx);

      // delete &(trk[ib + nb*ie].par);
      // delete &(trk[ib + nb*ie].cov);
      // delete &(trk[ib + nb*ie].q);
      // delete &(trk[ib + nb*ie].hitidx);

      // delete &(hit[ib + nb*ie].pos);
      // delete &(hit[ib + nb*ie].cov);
  //   }
  // }

  // delete(&(all_tracks.par));
  // delete(&(all_tracks.cov));
  // delete(&(all_tracks.q));

  // delete(&(all_hits.pos));
  // delete(&(all_hits.cov));

  // delete(&(all_out.par));
  // delete(&(all_out.cov));
  // delete(&(all_out.q));

  }
  Kokkos::finalize(); 

  return 0;
}

void prepareTracks(ATRK inputtrk, MPTRK &intrk) { 
  // store in element order for bunches of bsize matrices (a la matriplex)
  for (size_t ie=0;ie<nevts;++ie) {
    for (size_t ib=0;ib<nb;++ib) {
      for (size_t it=0;it<bsize;++it) {

  //par
  for (size_t ip=0;ip<6;++ip) {
    intrk.par(ib + nb*ie,ip,it) = (1+smear*randn(0,1))*inputtrk.par[ip];
  }
  //cov
  for (size_t i=0;i<6;++i)
    for (size_t j=0;j<6;++j)
      intrk.cov(ib + nb*ie,i,j,it) = (1+smear*randn(0,1))*inputtrk.cov[SymOffsets66(PosInMtrx(i,j,6))];
  //q
  intrk.q(ib + nb*ie,it) = inputtrk.q-2*ceil(-0.5 + (float)rand() / RAND_MAX);//fixme check
      
      } // block loop
    } // nb
  } // nevts
}

void prepareHits(AHIT inputhit, MPHIT &inhit) {

  // store in element order for bunches of bsize matrices (a la matriplex)
  for (size_t ie=0;ie<nevts;++ie) {
    for (size_t ib=0;ib<nb;++ib) {
      for (size_t it=0;it<bsize;++it) {
    
    //pos
    for (size_t ip=0;ip<3;++ip) {
      inhit.pos(ib + nb*ie,ip,it) = (1+smear*randn(0,1))*inputhit.pos[ip];
    }
    //cov
    for (size_t i=0;i<3;++i)
      for (size_t j=0;j<3;++j)
        inhit.cov(ib + nb*ie,i,j,it) = (1+smear*randn(0,1))*inputhit.cov[SymOffsets66(PosInMtrx(i,j,6))];
      
      } // bsize
    } // nb
  } // nevts
}

// MP version
// example:
//      propagateToZ(btracks.cov,  // MP6x6SF
//                   btracks.par,  // MP6F
//                   btracks.q,    // MP1I
//                   bhits.pos,    // MP3F
//                   obtracks.cov, // MP6x6SF
//                   obtracks.par  // MP6F
//                   );
// void propagateToZ(const ViewMatrixMP inErr,  // input covariance
//                   const ViewVectorMP inPar,  // input parameters/state
//                   const ViewVectorINT inChg, // input q from track
//                   const ViewVectorMP msP,    // input parameters from hit?
//                   ViewMatrixMP outErr,       // output covariance
//                   ViewVectorMP outPar) {     // output parameters/state
  //


void propagateToZ(const CBTRK &inTrks, const CBHIT &inHits, CBTRK &outTrks, 
                  ViewMatrixCB &errorProp, ViewMatrixCB &temp) {

  // ViewMatrixCB errorProp("ep", nevts*nb, 6, 6, bsize),
  //              temp("temp", nevts*nb, 6, 6, bsize);

  Kokkos::parallel_for( "p2z_batch_loop", range_policy(0,nb*nevts), KOKKOS_LAMBDA ( int batch ) {
  // #pragma omp parallel for
  // for (size_t ie=0;ie<nevts;++ie) { // combined these two loop over batches
  // for (size_t ib=0;ib<nb;++ib) {    // // TODO make a kookos parallel
  // size_t batch = ib + nb*ie;
  
  for (size_t layer=0;layer<nlayers;++layer) { 

  #pragma ivdep
  #pragma simd
  for (size_t it=0;it<bsize;++it) { 
   
    const float zout = inHits.pos(batch, Z_IND, it); 
    const float k = inTrks.q(batch, it)*100/3.8; 
    const float deltaZ = zout - inTrks.par(batch, Z_IND, it); 
    const float pt = 1.0f/inTrks.par(batch, IPT_IND,it); 
    const float cosP = cosf( inTrks.par(batch, PHI_IND, it) ); 
    const float sinP = sinf( inTrks.par(batch, PHI_IND, it) ); 
    const float cosT = cosf( inTrks.par(batch, THETA_IND, it) ); 
    const float sinT = sinf( inTrks.par(batch, THETA_IND, it) ); 
    const float pxin = cosP*pt;
    const float pyin = sinP*pt;
    const float alpha = deltaZ*sinT*inTrks.par(batch, IPT_IND, it)/(cosT*k); 
    const float sina = sinf(alpha); // this can be approximated;
    const float cosa = cosf(alpha); // this can be approximated;

    // array of state
    outTrks.par(batch,X_IND,it)     = inTrks.par(batch, X_IND, it) + k*(pxin*sina - pyin*(1.0f-cosa));
    outTrks.par(batch,Y_IND,it)     = inTrks.par(batch, Y_IND, it) + k*(pyin*sina + pxin*(1.0f-cosa));
    outTrks.par(batch,Z_IND,it)     = zout;
    outTrks.par(batch,IPT_IND,it)   = inTrks.par(batch, IPT_IND, it);
    outTrks.par(batch,PHI_IND,it)   = inTrks.par(batch, PHI_IND, it)+alpha;
    outTrks.par(batch,THETA_IND,it) = inTrks.par(batch, THETA_IND, it);
    
    const float sCosPsina = sinf(cosP*sina);
    const float cCosPsina = cosf(cosP*sina);
    
    for (size_t i=0;i<6;++i) errorProp(batch,i,i,it) = 1.0f;
    //there are two cause we're doing symmetry
    errorProp(batch,2,0,it) = errorProp(batch,0,2,it) = cosP*sinT*(sinP*cosa*sCosPsina-cosa)/cosT;
    errorProp(batch,3,0,it) = errorProp(batch,0,3,it) = cosP*sinT*deltaZ*cosa*(1.0f-sinP*sCosPsina)/(cosT*inTrks.par(batch,IPT_IND,it))-k*(cosP*sina-sinP*(1.0f-cCosPsina))/(inTrks.par(batch,IPT_IND,it)*inTrks.par(batch,IPT_IND,it));
    errorProp(batch,4,0,it) = errorProp(batch,0,4,it) = (k/inTrks.par(batch,IPT_IND,it))*(-sinP*sina+sinP*sinP*sina*sCosPsina-cosP*(1.0f-cCosPsina));
    errorProp(batch,5,0,it) = errorProp(batch,0,5,it) = cosP*deltaZ*cosa*(1.0f-sinP*sCosPsina)/(cosT*cosT);
    errorProp(batch,2,1,it) = errorProp(batch,1,2,it) = cosa*sinT*(cosP*cosP*sCosPsina-sinP)/cosT;
    errorProp(batch,3,1,it) = errorProp(batch,1,3,it) = sinT*deltaZ*cosa*(cosP*cosP*sCosPsina+sinP)/(cosT*inTrks.par(batch,IPT_IND,it))-k*(sinP*sina+cosP*(1.0f-cCosPsina))/(inTrks.par(batch,IPT_IND,it)*inTrks.par(batch,IPT_IND,it));
    errorProp(batch,4,1,it) = errorProp(batch,1,4,it) = (k/inTrks.par(batch,IPT_IND,it))*(-sinP*(1.0f-cCosPsina)-sinP*cosP*sina*sCosPsina+cosP*sina);
    errorProp(batch,5,1,it) = errorProp(batch,1,5,it) = deltaZ*cosa*(cosP*cosP*sCosPsina+sinP)/(cosT*cosT);
    errorProp(batch,2,4,it) = errorProp(batch,4,2,it) = -inTrks.par(batch,IPT_IND,it)*sinT/(cosT*k);
    errorProp(batch,3,4,it) = errorProp(batch,4,3,it) = sinT*deltaZ/(cosT*k);
    errorProp(batch,5,4,it) = errorProp(batch,4,5,it) = inTrks.par(batch,IPT_IND,it)*deltaZ/(cosT*cosT*k);

  } // bsize


  for ( int i = 0; i < 6; ++i ) {
    for ( int j = 0; j < 6; ++j ) {

      #pragma ivdep
      #pragma simd
      for ( int it = 0; it < bsize; ++it ) 
        temp(batch,i,j,it) = 0.0;

      for ( int k = 0; k < 6; ++k ) {
        #pragma ivdep
        #pragma simd
        for ( int it = 0; it < bsize; ++it ) {
          temp(batch,i,j,it) += errorProp(batch,i,k,it) * inTrks.cov(batch,k,j,it);
        }
      }
      
    }
  } //gemm

  //gemm with B transposed
  for ( int i = 0; i < 6; ++i ) {
    for ( int j = 0; j < 6; ++j ) {

      #pragma ivdep
      #pragma simd
      for ( int it = 0; it < bsize; ++it ) 
        outTrks.cov(batch,i,j,it) = 0.0;

      for ( int k = 0; k < 6; ++k ) {
        #pragma ivdep
        #pragma simd
        for ( int it = 0; it < bsize; ++it ) {
          outTrks.cov(batch,i,j,it) += errorProp(batch,i,k,it) * temp(batch,j,k,it);
        }
      }
      
    }
  } //gemmT


  } // nlayers
  // } // nb
  // }  // nevts
  });

} // P2Z


void invert_33(const ViewMatrixCB &mat, int batch) {

  float det[bsize];
  ViewMatrixCB temp_33("temp_33", 3, 3, bsize);

  for ( int it = 0; it < bsize; ++it ) {

    det[it] = mat(batch,0,0,it)*(mat(batch,1,1,it)*mat(batch,2,2,it)-mat(batch,1,2,it)*mat(batch,2,1,it))
            - mat(batch,0,1,it)*(mat(batch,1,0,it)*mat(batch,2,2,it)-mat(batch,1,2,it)*mat(batch,2,0,it))
            + mat(batch,0,2,it)*(mat(batch,1,0,it)*mat(batch,2,1,it)-mat(batch,1,1,it)*mat(batch,2,0,it));

    temp_33(batch,0,0,it) =      (mat(batch,1,1,it)*mat(batch,2,2,it)-mat(batch,1,2,it)*mat(batch,2,1,it));
    temp_33(batch,0,1,it) = -1 * (mat(batch,0,1,it)*mat(batch,2,2,it)-mat(batch,0,2,it)*mat(batch,2,1,it));
    temp_33(batch,0,2,it) =      (mat(batch,0,1,it)*mat(batch,1,2,it)-mat(batch,0,2,it)*mat(batch,1,1,it));

    temp_33(batch,1,0,it) = -1 * (mat(batch,1,0,it)*mat(batch,2,2,it)-mat(batch,1,2,it)*mat(batch,2,0,it));
    temp_33(batch,1,1,it) =      (mat(batch,0,0,it)*mat(batch,2,2,it)-mat(batch,0,2,it)*mat(batch,2,0,it));
    temp_33(batch,1,2,it) = -1 * (mat(batch,0,0,it)*mat(batch,1,2,it)-mat(batch,0,2,it)*mat(batch,1,0,it));
    
    temp_33(batch,2,0,it) =      (mat(batch,1,0,it)*mat(batch,2,1,it)-mat(batch,1,1,it)*mat(batch,2,0,it));
    temp_33(batch,2,1,it) = -1 * (mat(batch,0,0,it)*mat(batch,2,1,it)-mat(batch,0,1,it)*mat(batch,2,0,it));
    temp_33(batch,2,2,it) =      (mat(batch,0,0,it)*mat(batch,1,1,it)-mat(batch,0,1,it)*mat(batch,1,0,it));

    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++)
        mat(batch,i,j,it) = temp_33(batch,i,j,it);

  }

}


void update(const CBTRK &trk, const CBHIT &hit) {
// newTrk.par = trk.par+Kgain(hit.pos-H*trk.par)
// newTrk.cov = trk.cov-Kgain*H*trk.cov
// Kgain =  trk.cov*Ht(H*trk.cov*Ht+hit.cov)^-1 ---- 6x3?
// H = 1 0 0 0 0 0
//     0 1 0 0 0 0
//     0 0 1 0 0 0



  // ViewMatrixCB errorProp("ep", nevts*nb, 6, 6, bsize),
  //              temp("temp", nevts*nb, 6, 6, bsize);


    ViewMatrixCB Ht("Ht", nb*nevts, 6, 3, bsize);
    ViewMatrixCB H_trk_cov("H_trk_par", nb*nevts, 3, 6, bsize);
    ViewMatrixCB K("H", nb*nevts, 6, 3, bsize);
    ViewMatrixCB mat("mat", nb*nevts, 3, 3, bsize);
    ViewMatrixCB temp_33("temp_33", nb*nevts, 3, 3, bsize);
    ViewMatrixCB temp_63("temp_63", nb*nevts, 6, 3, bsize);
    ViewMatrixCB temp_31("temp_31", nb*nevts, 3, 1, bsize);
    ViewMatrixCB temp_61("temp_61", nb*nevts, 6, 1, bsize);
    ViewMatrixCB temp_66("temp_66", nb*nevts, 6, 6, bsize);

  Kokkos::parallel_for( "update_batch_loop", range_policy(0,nb*nevts), KOKKOS_LAMBDA ( int batch ) {

  for (size_t layer=0;layer<nlayers;++layer) { 

    // ViewMatrixOB Ht("Ht", 6, 3, bsize);
    // ViewMatrixOB H_trk_cov("H_trk_par", 3, 6, bsize);
    // ViewMatrixOB K("H", 6, 3, bsize);
    // ViewMatrixOB temp_33("temp_33", 3, 3, bsize);
    // ViewMatrixOB temp_63("temp_63", 6, 3, bsize);
    // ViewMatrixOB temp_31("temp_31", 3, 1, bsize);
    // ViewMatrixOB temp_61("temp_61", 6, 1, bsize);
    // ViewMatrixOB temp_66("temp_66", 6, 6, bsize);


    for ( int i = 0; i < 3; ++i ) {
      for ( int j = 0; j < 6; ++j ) {
        for ( int it = 0; it < bsize; ++it ) {
          H_trk_cov(batch,i,j,it) = trk.cov(batch,i,j,it);
          if(i == j)
            Ht(batch,j,i,it) = 1.0f;
          else
            Ht(batch,j,i,it) = 0.0f;
        }
      }
    }

    // Kgain =  trk.cov*Ht(H*trk.cov*Ht+hit.cov)^-1 ---- 6x3?
    gemm(H_trk_cov, Ht, temp_33, batch, 3, 6, 3);
    // for ( int i = 0; i < 3; ++i ) {
    //   for ( int j = 0; j < 3; ++j ) {
    //     for ( int b = 0; b < bsize; ++b )  temp_33(batch,i,j,b) = 0.0;
    //     for ( int k = 0; k < 6; ++k ) {
    //       for ( int b = 0; b < bsize; ++b ) temp_33(batch,i,j,b) += H_trk_cov(batch,i,k,b) * Ht(batch,k,j,b);
    //     }
    //   }
    // }
    for ( int i = 0; i < 3; ++i ) {
      for ( int j = 0; j < 3; ++j ) {
        for ( int it = 0; it < bsize; ++it )
          mat(batch,i,j,it) = hit.cov(batch,i,j,it) + temp_33(batch,i,j,it);
      }
    }
    
    float det[bsize];

    for ( int it = 0; it < bsize; ++it ) {

      det[it] = mat(batch,0,0,it)*(mat(batch,1,1,it)*mat(batch,2,2,it)-mat(batch,1,2,it)*mat(batch,2,1,it))
              - mat(batch,0,1,it)*(mat(batch,1,0,it)*mat(batch,2,2,it)-mat(batch,1,2,it)*mat(batch,2,0,it))
              + mat(batch,0,2,it)*(mat(batch,1,0,it)*mat(batch,2,1,it)-mat(batch,1,1,it)*mat(batch,2,0,it));
      det[it] = 1.0f/det[it];

      temp_33(batch,0,0,it) =      det[it] * (mat(batch,1,1,it)*mat(batch,2,2,it)-mat(batch,1,2,it)*mat(batch,2,1,it));
      temp_33(batch,0,1,it) = -1 * det[it] * (mat(batch,0,1,it)*mat(batch,2,2,it)-mat(batch,0,2,it)*mat(batch,2,1,it));
      temp_33(batch,0,2,it) =      det[it] * (mat(batch,0,1,it)*mat(batch,1,2,it)-mat(batch,0,2,it)*mat(batch,1,1,it));

      temp_33(batch,1,0,it) = -1 * det[it] * (mat(batch,1,0,it)*mat(batch,2,2,it)-mat(batch,1,2,it)*mat(batch,2,0,it));
      temp_33(batch,1,1,it) =      det[it] * (mat(batch,0,0,it)*mat(batch,2,2,it)-mat(batch,0,2,it)*mat(batch,2,0,it));
      temp_33(batch,1,2,it) = -1 * det[it] * (mat(batch,0,0,it)*mat(batch,1,2,it)-mat(batch,0,2,it)*mat(batch,1,0,it));
      
      temp_33(batch,2,0,it) =      det[it] * (mat(batch,1,0,it)*mat(batch,2,1,it)-mat(batch,1,1,it)*mat(batch,2,0,it));
      temp_33(batch,2,1,it) = -1 * det[it] * (mat(batch,0,0,it)*mat(batch,2,1,it)-mat(batch,0,1,it)*mat(batch,2,0,it));
      temp_33(batch,2,2,it) =      det[it] * (mat(batch,0,0,it)*mat(batch,1,1,it)-mat(batch,0,1,it)*mat(batch,1,0,it));

      // for (int i = 0; i < 3; i++)
      //   for (int j = 0; j < 3; j++)
      //     mat(batch,i,j,it) = temp_33(batch,i,j,it);

    }

    // Kgain =  trk.cov*Ht*temp_33
    gemm(Ht, temp_33, temp_63, batch, 6, 3, 3);
    for ( int i = 0; i < 6; ++i ) {
      for ( int j = 0; j < 3; ++j ) {
        for ( int it = 0; it < bsize; ++it ) K(batch,i,j,it) = 0.0;
        for ( int k = 0; k < 6; ++k ) {
          for ( int it = 0; it < bsize; ++it ) {
            K(batch,i,j,it) += trk.cov(batch,i,k,it) * temp_63(batch,k,j,it);
          }
        }
      }
    }


// newTrk.par = trk.par+Kgain(hit.pos-H*trk.par)
    for ( int i = 0; i < 3; ++i ) {
      for ( int it = 0; it < bsize; ++it )
        temp_31(batch,i,0,it) = hit.pos(batch,i,it) - trk.par(batch,i,it);
    }
    gemm(K, temp_31, temp_61, batch, 6, 3, 1);
    for ( int i = 0; i < 6; ++i ) {
      for ( int it = 0; it < bsize; ++it )
        trk.par(batch,i,it) += temp_61(batch,i,0,it);
    }


// newTrk.cov = trk.cov-Kgain*H*trk.cov
    gemm(K, H_trk_cov, temp_66, batch, 6, 3, 6);
    for ( int i = 0; i < 6; ++i ) {
      for ( int j = 0; j < 6; ++j ) {
        for ( int it = 0; it < bsize; ++it )
          trk.cov(batch,i,j,it) -= temp_66(batch,i,j,it);
      }
    }

  } // nlayers

  });

}

void averageOutputs(MPTRK &outtrk, MPHIT &hit) {

  double avgx = 0, avgy = 0, avgz = 0;
  double avgdx = 0, avgdy = 0, avgdz = 0;
  for (size_t ie=0;ie<nevts;++ie) { // loop over events
    for (size_t ib=0;ib<nb;++ib) { // loop over bunches of tracks
      for (size_t it=0;it<bsize;++it) {
        float x_ = outtrk.par(ib + nb*ie,X_IND,it);
        float y_ = outtrk.par(ib + nb*ie,Y_IND,it);
        float z_ = outtrk.par(ib + nb*ie,Z_IND,it);
        avgx += x_;
        avgy += y_;
        avgz += z_;
        float hx_ = hit.pos(ib + nb*ie,X_IND,it);
        float hy_ = hit.pos(ib + nb*ie,Y_IND,it);
        float hz_ = hit.pos(ib + nb*ie,Z_IND,it);
        avgdx += (x_-hx_)/x_;
        avgdy += (y_-hy_)/y_;
        avgdz += (z_-hz_)/z_;
      }
    }
  }
  avgx = avgx/double(nevts*ntrks);
  avgy = avgy/double(nevts*ntrks);
  avgz = avgz/double(nevts*ntrks);
  avgdx = avgdx/double(nevts*ntrks);
  avgdy = avgdy/double(nevts*ntrks);
  avgdz = avgdz/double(nevts*ntrks);

  double stdx = 0, stdy = 0, stdz = 0;
  double stddx = 0, stddy = 0, stddz = 0;
  for (size_t ie=0;ie<nevts;++ie) { // loop over events
    for (size_t ib=0;ib<nb;++ib) { // loop over bunches of tracks
      for (size_t it=0;it<bsize;++it) {
        float x_ = outtrk.par(ib + nb*ie,X_IND,it);
        float y_ = outtrk.par(ib + nb*ie,Y_IND,it);
        float z_ = outtrk.par(ib + nb*ie,Z_IND,it);
        stdx += (x_-avgx)*(x_-avgx);
        stdy += (y_-avgy)*(y_-avgy);
        stdz += (z_-avgz)*(z_-avgz);
        float hx_ = hit.pos(ib + nb*ie,X_IND,it);
        float hy_ = hit.pos(ib + nb*ie,Y_IND,it);
        float hz_ = hit.pos(ib + nb*ie,Z_IND,it);
        stddx += ((x_-hx_)/x_-avgdx)*((x_-hx_)/x_-avgdx);
        stddy += ((y_-hy_)/y_-avgdy)*((y_-hy_)/y_-avgdy);
        stddz += ((z_-hz_)/z_-avgdz)*((z_-hz_)/z_-avgdz);
      }
    }
  }

  stdx = sqrtf(stdx/double(nevts*ntrks));
  stdy = sqrtf(stdy/double(nevts*ntrks));
  stdz = sqrtf(stdz/double(nevts*ntrks));
  stddx = sqrtf(stddx/double(nevts*ntrks));
  stddy = sqrtf(stddy/double(nevts*ntrks));
  stddz = sqrtf(stddz/double(nevts*ntrks));

  printf("track x avg=%f std/avg=%f\n", avgx, fabs(stdx/avgx));
  printf("track y avg=%f std/avg=%f\n", avgy, fabs(stdy/avgy));
  printf("track z avg=%f std/avg=%f\n", avgz, fabs(stdz/avgz));
  printf("track dx/x avg=%f std=%f\n", avgdx, stddx);
  printf("track dy/y avg=%f std=%f\n", avgdy, stddy);
  printf("track dz/z avg=%f std=%f\n", avgdz, stddz);
}

// TODO make these use kokkos?
// TODO make them actually fit the gemm standard?
KOKKOS_INLINE_FUNCTION void gemm(const ViewMatrixCB &A, const ViewMatrixCB &B, const ViewMatrixCB &C, int batch, int rows_a, int cols_a, int cols_b) {

  for ( int i = 0; i < rows_a; ++i ) {
    for ( int j = 0; j < cols_b; ++j ) {

      for ( int b = 0; b < bsize; ++b ) 
        C(batch,i,j,b) = 0.0;

      for ( int k = 0; k < cols_a; ++k ) {
        for ( int b = 0; b < bsize; ++b ) {
          C(batch,i,j,b) += A(batch,i,k,b) * B(batch,k,j,b);
        }
      }
      
    }
  }

}


void gemm_T(const ViewMatrixCB &A, const ViewMatrixCB &B, const ViewMatrixCB &C, int batch, int rows_a, int cols_a, int cols_b) {

  for ( int i = 0; i < rows_a; ++i ) {
    for ( int j = 0; j < cols_b; ++j ) {

      for ( int b = 0; b < bsize; ++b ) 
        C(batch,i,j,b) = 0.0;

      for ( int k = 0; k < cols_a; ++k ) {
        for ( int b = 0; b < bsize; ++b ) {
          C(batch,i,j,b) += A(batch,i,k,b) * B(batch,j,k,b);
        }
      }
      
    }
  }

}



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

double get_time() {
  struct timeval timecheck;
  gettimeofday( &timecheck, NULL );
  double time = ( 1.0 * timecheck.tv_sec ) + ( 1.0e-6 * timecheck.tv_usec );
  return time;
}

/* Copyright for the lovely input reader below
// It comes from the kokkos examples
// but we have lightly modified it to work with nice small matrices
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 2.0
//              Copyright (2014) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions Contact  H. Carter Edwards (hcedwar@sandia.gov)
//
// ************************************************************************
//@HEADER
*/
void read_input(int argc, char* argv[], int &N, int &M, int &S, int &nrepeat, int &batch_size, int &nbatches, bool &use_batches) {

  // Read command line arguments.
  for ( int i = 0; i < argc; i++ ) {
    if ( ( strcmp( argv[ i ], "-N" ) == 0 ) || ( strcmp( argv[ i ], "-Rows" ) == 0 ) ) {
      N = atoi( argv[ ++i ] );
      printf( "  User N is %d\n", N );
    }
    else if ( ( strcmp( argv[ i ], "-M" ) == 0 ) || ( strcmp( argv[ i ], "-Columns" ) == 0 ) ) {
      M = atoi( argv[ ++i ] );;
      printf( "  User M is %d\n", M );
    }
    else if ( strcmp( argv[ i ], "-nrepeat" ) == 0 ) {
      nrepeat = atoi( argv[ ++i ] );
    }
    else if ( strcmp( argv[ i ], "-batch_size" ) == 0 ) {
      batch_size = atoi( argv[ ++i ] );
    }
    else if ( strcmp( argv[ i ], "-nbatches" ) == 0 ) {
      nbatches = atoi( argv[ ++i ] );
    }
    else if ( strcmp( argv[ i ], "-use_batches" ) == 0 ) {
      use_batches = true;
    }
    else if ( ( strcmp( argv[ i ], "-h" ) == 0 ) || ( strcmp( argv[ i ], "-help" ) == 0 ) ) {
      printf( "  y^T*A*x Options:\n" );
      printf( "  -Rows (-N) <int>:      determines number of rows (default: 1024)\n" );
      printf( "  -Columns (-M) <int>:   determines number of columns (default: 1024)\n" );
      printf( "  -nrepeat <int>:        number of repetitions (default: 100)\n" );
      printf( "  -batch_size <int>:     number of matrices per batch (default: 100)\n" );
      printf( "  -nbatches <int>:       number of batches (default: 100)\n" );
      printf( "  -use_batches:          perform batch based test\n" );
      printf( "  -help (-h):            print this message\n\n" );
      exit( 1 );
    }
  }

  // If only M is undefined, set it.
  if ( M == -1 ) M = 1024;

  // If N is undefined, set it.
  if ( N == -1 ) N = 1024;

  S = M*N;

  printf( "  Total size S = %d N = %d M = %d\n", S, N, M );
  printf( "  Using %d batches of %d matrices\n", nbatches, batch_size );

  // Check sizes.
  if ( ( S < 0 ) || ( N < 0 ) || ( M < 0 ) || ( nrepeat < 0 ) ) {
    printf( "  Sizes must be greater than 0.\n" );
    exit( 1 );
  }

  if ( ( N * M ) != S ) {
    printf( "  N * M != S\n" );
    exit( 1 );
  }

}



