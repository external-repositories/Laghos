// Copyright (c) 2017, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-734707. All Rights
// reserved. See files LICENSE and NOTICE for details.
//
// This file is part of CEED, a collection of benchmarks, miniapps, software
// libraries and APIs for efficient high-order finite element and spectral
// element discretizations for exascale applications. For more information and
// source code availability see http://github.com/ceed.
//
// The CEED research is supported by the Exascale Computing Project 17-SC-20-SC,
// a collaborative effort of two U.S. Department of Energy organizations (Office
// of Science and the National Nuclear Security Administration) responsible for
// the planning and preparation of a capable exascale ecosystem, including
// software, applications, hardware, advanced system engineering and early
// testbed platforms, in support of the nation's exascale computing imperative.

#include "qupdate.hpp"

using namespace std;

namespace mfem {

namespace hydrodynamics {
   
// **************************************************************************
template <const int NUM_DOFS_1D,
          const int NUM_QUAD_1D> static
void qGradVector2D(const int numElements,
                   const double* __restrict dofToQuad,
                   const double* __restrict dofToQuadD,
                   const double* __restrict in,
                   double* __restrict out){
   const int NUM_QUAD = NUM_QUAD_1D*NUM_QUAD_1D;
   GET_CONST_ADRS(dofToQuad);
   GET_CONST_ADRS(dofToQuadD);
   GET_CONST_ADRS(in);
   GET_ADRS(out);
   MFEM_FORALL(e, numElements,
   {
      double s_gradv[4*NUM_QUAD];
      for (int i = 0; i < (4*NUM_QUAD); ++i) {
         s_gradv[i] = 0.0;
      }
         
      for (int dy = 0; dy < NUM_DOFS_1D; ++dy) {
         double vDx[2*NUM_QUAD_1D];
         double vx[2*NUM_QUAD_1D];
         for (int qx = 0; qx < NUM_QUAD_1D; ++qx) {
            for (int vi = 0; vi < 2; ++vi) {
               vDx[ijN(vi,qx,2)] = 0.0;
               vx[ijN(vi,qx,2)] = 0.0;
            }
         }
         for (int dx = 0; dx < NUM_DOFS_1D; ++dx) {
            for (int qx = 0; qx < NUM_QUAD_1D; ++qx) {
               const double wDx = d_dofToQuadD[ijN(qx,dx,NUM_QUAD_1D)];
               const double wx  = d_dofToQuad[ijN(qx,dx,NUM_QUAD_1D)];
               for (int c = 0; c < 2; ++c) {
                  const double input = d_in[_ijklNM(c,dx,dy,e,NUM_DOFS_1D,numElements)];
                  vDx[ijN(c,qx,2)] += input * wDx;
                  vx[ijN(c,qx,2)] += input * wx;
               }
            }
         }
         for (int qy = 0; qy < NUM_QUAD_1D; ++qy) {
            const double vy  = d_dofToQuad[ijN(qy,dy,NUM_QUAD_1D)];
            const double vDy = d_dofToQuadD[ijN(qy,dy,NUM_QUAD_1D)];
            for (int qx = 0; qx < NUM_QUAD_1D; ++qx) {
               const int q = qx+NUM_QUAD_1D*qy;
               for (int c = 0; c < 2; ++c) {
                  s_gradv[ijkN(c,0,q,2)] += vy*vDx[ijN(c,qx,2)];
                  s_gradv[ijkN(c,1,q,2)] += vDy*vx[ijN(c,qx,2)];
               }
            }
         }
      }         
      for (int q = 0; q < NUM_QUAD; ++q) {
         d_out[ijklNM(0,0,q,e,2,NUM_QUAD)] = s_gradv[ijkN(0,0,q,2)];
         d_out[ijklNM(1,0,q,e,2,NUM_QUAD)] = s_gradv[ijkN(1,0,q,2)];
         d_out[ijklNM(0,1,q,e,2,NUM_QUAD)] = s_gradv[ijkN(0,1,q,2)];
         d_out[ijklNM(1,1,q,e,2,NUM_QUAD)] = s_gradv[ijkN(1,1,q,2)];
      }
   });
}
   
// **************************************************************************
// * Dof2DQuad
// **************************************************************************
void Dof2QuadGrad(ParFiniteElementSpace &pfes,
                  const IntegrationRule& ir,
                  const double *d_in,
                  double **d_out){
   push();
   const mfem::kFiniteElementSpace &kfes =
      *(new kFiniteElementSpace(static_cast<FiniteElementSpace*>(&pfes)));
   const FiniteElementSpace &fes = pfes;
   const mfem::kDofQuadMaps* maps = kDofQuadMaps::Get(fes,ir);
      
   const int dim = fes.GetMesh()->Dimension();
   const int vdim = fes.GetVDim();
   const int vsize = fes.GetVSize();
   assert(dim==2);
   assert(vdim==2);
   const mfem::FiniteElement& fe = *fes.GetFE(0);
   const size_t numDofs  = fe.GetDof();
   const size_t nzones = fes.GetNE();
   const size_t nqp = ir.GetNPoints();

   const size_t local_size = vdim * numDofs * nzones;
   static double *d_local_in = NULL;
   if (!d_local_in){
      d_local_in = (double*) mm::malloc<double>(local_size);
   }
      
   Vector v_in = Vector((double*)d_in, vsize);
   Vector v_local_in = Vector(d_local_in, local_size);
   kfes.GlobalToLocal(v_in, v_local_in);
            
   const size_t out_size = vdim * vdim * nqp * nzones;
   if (!(*d_out)){
      *d_out = (double*) mm::malloc<double>(out_size);
   }
    
   const int dofs1D = fes.GetFE(0)->GetOrder() + 1;
   const int quad1D = IntRules.Get(Geometry::SEGMENT,ir.GetOrder()).GetNPoints();

   assert(dofs1D==3);
   assert(quad1D==4);      
   qGradVector2D<3,4>(nzones,
                      maps->dofToQuad,
                      maps->dofToQuadD,
                      d_local_in,
                      *d_out);
   pop();
}
   
} // namespace hydrodynamics
   
} // namespace mfem
