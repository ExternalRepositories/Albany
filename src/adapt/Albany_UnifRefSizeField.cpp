//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Albany_UnifRefSizeField.hpp"
#include "Albany_FMDBMeshStruct.hpp"
#include "Epetra_Import.h"
#include "PWLinearSField.h"



const double dist(double *p1, double *p2){

  return std::sqrt(p1[0]*p2[0] + p1[1]*p2[1] + p1[2]*p2[2]);

}

Albany::UnifRefSizeField::UnifRefSizeField(Albany::FMDBDiscretization *disc_) :
        disc(disc_)
{
}

Albany::UnifRefSizeField::
~UnifRefSizeField()
{
}

void
Albany::UnifRefSizeField::setParams(const Epetra_Vector *sol, const Epetra_Vector *ovlp_sol, double element_size){

  solution = sol;
  ovlp_solution = ovlp_sol;
  elem_size = element_size;

}

//int Albany::UnifRefSizeField::computeSizeField(pPart part, pSField field, void *vp){
int Albany::UnifRefSizeField::computeSizeField(pPart part, pSField field){

   pVertex vt;
   double h[3], dirs[3][3];

   static int numCalls = 0;
   static double initAvgEdgeLen = 0;
   static double currGlobMin = 0, currGlobMax = 0, currGlobAvg = 0;
   if (0 == numCalls) {
      getCurrentSize(part, currGlobMin, currGlobMax, initAvgEdgeLen);
   } else {
      getCurrentSize(part, currGlobMin, currGlobMax, currGlobAvg);
   }

  double minSize = std::numeric_limits<double>::max();
  double maxSize = std::numeric_limits<double>::min();

   VIter vit = M_vertexIter(part);
   while (vt = VIter_next(vit)) {
      const double sz = 0.5 * initAvgEdgeLen;
      if ( sz < minSize ) minSize = sz;
      if ( sz > maxSize ) maxSize = sz;
      for (int i = 0; i < 3; i++) {
         h[i] = sz;
      }

      dirs[0][0] = 1.0;
      dirs[0][1] = 0.;
      dirs[0][2] = 0.;
      dirs[1][0] = 0.;
      dirs[1][1] = 1.0;
      dirs[1][2] = 0.;
      dirs[2][0] = 0.;
      dirs[2][1] = 0.;
      dirs[2][2] = 1.0;

      ((PWLsfield *) field)->setSize((pEntity) vt, dirs, h);
   }

   VIter_delete(vit);
//   double beta[] = {1.5, 1.5, 1.5};
//   ((PWLsfield *) field)->anisoSmooth(beta);


   double globMin = 0;
   double globMax = 0;
   MPI_Reduce(&minSize, &globMin, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
   MPI_Reduce(&maxSize, &globMax, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

   if (0 == SCUTIL_CommRank()) {
      if ( 0 == numCalls ) {
         printf("%s initial edgeLength avg %f min %f max %f\n", __FUNCTION__, initAvgEdgeLen, currGlobMin, currGlobMax);
         printf("%s target edgeLength min %f max %f\n", __FUNCTION__, globMin, globMax);
      } else {
         printf("%s current edgeLength avg %f min %f max %f\n", __FUNCTION__, currGlobAvg, currGlobMin, currGlobMax);
      }
      fflush(stdout);
   }

   numCalls++;

   return 1;
}

int
Albany::UnifRefSizeField::getCurrentSize(pPart part, double& globMinSize, double& globMaxSize, double& globAvgSize) {

   EIter eit = M_edgeIter(part);
   pEdge edge;
   int numEdges = 0;
   double avgSize = 0.;
   double minSize = std::numeric_limits<double>::max();
   double maxSize = std::numeric_limits<double>::min();

   while (edge = EIter_next(eit)) {
      numEdges++;
      double len = sqrt(E_lengthSq(edge));
      avgSize += len;
      if ( len < minSize ) minSize = len;
      if ( len > maxSize ) maxSize = len;
   }
   EIter_delete(eit);
   avgSize /= numEdges;

   MPI_Allreduce(&avgSize, &globAvgSize, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
   MPI_Allreduce(&maxSize, &globMaxSize, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
   MPI_Allreduce(&minSize, &globMinSize, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
   globAvgSize /= SCUTIL_CommSize();

}

