//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Piro_StratimikosUtils.hpp"
#include "Teuchos_VerboseObject.hpp"

#include "Albany_NullSpaceUtils.hpp"
#include "Albany_ThyraUtils.hpp"
#include "Albany_CommUtils.hpp"
#include "Albany_TpetraThyraUtils.hpp"

namespace Albany {

namespace {

// Copied from Trilinos/packages/ml/src/Utils/ml_rbm.c.
template <class Traits>
void Coord2RBM(
  const Teuchos::RCP<Thyra_MultiVector> &coordMV,
  const int Ndof, const int NscalarDof, const int NSdim,
  typename Traits::array_type &rbm)
{
  int ii, jj;
  RigidBodyModes::LO_type dof;

  const int numSpaceDim = coordMV->domain()->dim(); // Number of multivectors are the dimension of the problem

  auto data = getLocalData(coordMV.getConst());
  const RigidBodyModes::LO_type numNodes = data[0].size(); // length of each vector in the multivector

  const RigidBodyModes::LO_type vec_leng = numNodes*Ndof;
  Traits traits_class(Ndof, NscalarDof, NSdim, vec_leng, rbm);

  Teuchos::ArrayRCP<const ST> x = data[0];
  Teuchos::ArrayRCP<const ST> y, z;
  if(numSpaceDim > 1) {
      y = data[1];
  }
  if(numSpaceDim > 2) {
      z = data[2];
  }

  traits_class.zero();

  for (RigidBodyModes::LO_type node = 0 ; node < numNodes; node++) {
    dof = node*Ndof;
    switch( Ndof - NscalarDof ) {
    case 6:
      for (ii=3;ii<6+NscalarDof;ii++) { /* lower half = [ 0 I ] */
        for (jj=0;jj<6+NscalarDof;jj++) {
          traits_class.ArrObj(dof, ii, jj) = (ii==jj) ? 1.0 : 0.0;
        }
      }
      /* There is no break here and that is on purpose */
    case 3:
      for (ii=0;ii<3+NscalarDof;ii++) { /* upper left = [ I ] */
        for (jj=0;jj<3+NscalarDof;jj++) {
          traits_class.ArrObj(dof, ii, jj) = (ii==jj) ? 1.0 : 0.0;
        }
      }
      for (ii=0;ii<3;ii++) { /* upper right = [ Q ] */
        for (jj=3+NscalarDof;jj<6+NscalarDof;jj++) {
          // std::cout <<"jj " << jj << " " << ii + jj << std::endl;
          if(ii == jj-3-NscalarDof) traits_class.ArrObj(dof, ii, jj) = 0.0;
          else {
            if (ii+jj == 4+NscalarDof) traits_class.ArrObj(dof, ii, jj) = z[node];
            else if ( ii+jj == 5+NscalarDof ) traits_class.ArrObj(dof, ii, jj) = y[node];
            else if ( ii+jj == 6+NscalarDof ) traits_class.ArrObj(dof, ii, jj) = x[node];
            else traits_class.ArrObj(dof, ii, jj) = 0.0;
          }
        }
      }
      ii = 0; jj = 5+NscalarDof; traits_class.ArrObj(dof, ii, jj) *= -1.0;
      ii = 1; jj = 3+NscalarDof; traits_class.ArrObj(dof, ii, jj) *= -1.0;
      ii = 2; jj = 4+NscalarDof; traits_class.ArrObj(dof, ii, jj) *= -1.0;
      break;

    case 2:
      for (ii=0;ii<2+NscalarDof;ii++) { /* upper left = [ I ] */
        for (jj=0;jj<2+NscalarDof;jj++) {
          traits_class.ArrObj(dof, ii, jj) = (ii==jj) ? 1.0 : 0.0;
        }
      }
      for (ii=0;ii<2+NscalarDof;ii++) { /* upper right = [ Q ] */
        for (jj=2+NscalarDof;jj<3+NscalarDof;jj++) {
          if (ii == 0) traits_class.ArrObj(dof, ii, jj) = -y[node];
          else {
            if (ii == 1) { traits_class.ArrObj(dof, ii, jj) =  x[node];}
            else traits_class.ArrObj(dof, ii, jj) = 0.0;
          }
        }
      }
      break;

    case 1:
      for (ii = 0; ii<1+NscalarDof; ii++) {
        for (jj=0; jj<1+NscalarDof; jj++) {
          traits_class.ArrObj(dof, ii, jj) = (ii == jj) ? 1.0 : 0.0;
        }
      }
      break;

    default:
      TEUCHOS_TEST_FOR_EXCEPTION(
        true,
        std::logic_error,
        "Coord2RBM: Ndof = " << Ndof << " not implemented\n");
    } /*switch*/

  } /*for (node = 0 ; node < numNodes; node++)*/

  return;
} /*Coord2RBM*/

//IKT, 6/28/15: the following set RBMs for non-elasticity problems.
template <class Traits>
void Coord2RBM_nonElasticity(
  const Teuchos::RCP<Thyra_MultiVector> &coordMV,
  const int Ndof, const int NscalarDof, const int NSdim,
  typename Traits::array_type &rbm)
{
  //std::cout << "setting RBMs in Coord2RBM_nonElasticity!" << std::endl;
  int ii, jj;
  RigidBodyModes::LO_type dof;

  int numSpaceDim = coordMV->domain()->dim(); // Number of multivectors are the dimension of the problem
  auto data = getLocalData(coordMV.getConst());

  // At least component x should be there
  const RigidBodyModes::LO_type numNodes = data[0].size(); // length of each vector in the multivector

  const RigidBodyModes::LO_type vec_leng = numNodes*Ndof;
  Traits traits_class(Ndof, NscalarDof, NSdim, vec_leng, rbm);

  Teuchos::ArrayRCP<const ST> x = data[0];
  Teuchos::ArrayRCP<const ST> y, z;
  if(numSpaceDim > 1) {
      y = data[1];
  }
  if(numSpaceDim > 2) {
      z = data[2];
  }

  traits_class.zero();

  //std::cout << "...Ndof: " << Ndof << std::endl;
  //std::cout << "...case: " << NSdim - NscalarDof << std::endl;
  for (RigidBodyModes::LO_type node = 0 ; node < numNodes; node++) {

    dof = node*Ndof;

    switch( NSdim - NscalarDof ) {
    case 3:
      for (ii=0;ii<2;ii++) { /* upper right = [ Q ] -- xy rotation only */
        jj = 2+NscalarDof;
        // std::cout <<"jj " << jj << " " << ii + jj << std::endl;
        if (ii == 0)
          traits_class.ArrObj(dof, ii, jj) = y[node];
        else if (ii == 1)
          traits_class.ArrObj(dof, ii, jj) = x[node];
      }
      ii = 0; jj = 2+NscalarDof;
      traits_class.ArrObj(dof, ii, jj) *= -1.0;
      /* There is no break here and that is on purpose */
    case 2:
      for (ii=0;ii<2+NscalarDof;ii++) { /* upper left = [ I ] */
        for (jj=0;jj<2+NscalarDof;jj++) {
          traits_class.ArrObj(dof, ii, jj) = (ii==jj) ? 1.0 : 0.0;
        }
      }
      break;

    default:
      TEUCHOS_TEST_FOR_EXCEPTION(
        true,
        std::logic_error,
        "Coord2RBM_nonElasticity: Ndof = " << Ndof << " not implemented\n");
    } /*switch*/

  } /*for (node = 0 ; node < numNodes; node++)*/

  return;
} /*Coord2RBM_nonElasticity*/

void subtractCentroid(const Teuchos::RCP<Thyra_MultiVector> &coordMV)
{
  auto spmd_vs = getSpmdVectorSpace(coordMV->range());
  const RigidBodyModes::LO_type nnodes = spmd_vs->localSubDim(); // local length of each vector
  const LO ndim = coordMV->domain()->dim(); // Number of multivectors are the dimension of the problem

  auto data = getNonconstLocalData(coordMV);
  ST centroid[3]; // enough for up to 3d
  {
    ST sum[3];
    for (int i = 0; i < ndim; ++i) {
      Teuchos::ArrayRCP<const ST> x = data[i];
      sum[i] = 0;
      for (RigidBodyModes::LO_type j = 0; j < nnodes; ++j) sum[i] += x[j];
    }
    Teuchos::reduceAll(*createTeuchosCommFromTeuchosComm(spmd_vs->getComm()), Teuchos::REDUCE_SUM, ndim,
                                sum, centroid);
    const RigidBodyModes::GO_type numNodes = spmd_vs->localSubDim(); // length of each vector in the multivector
    for (int i = 0; i < ndim; ++i) centroid[i] /= numNodes;
  }

  for (int i = 0; i < ndim; ++i) {
    Teuchos::ArrayRCP<ST> x = data[i];
    for (Teuchos::ArrayRCP<ST>::size_type j = 0; j < nnodes; ++j)
      x[j] -= centroid[i];
  }
}
} // namespace

RigidBodyModes::RigidBodyModes(int numPDEs_)
  : numPDEs(numPDEs_), numElasticityDim(0), numScalar(0), nullSpaceDim(0),
    mlUsed(false), mueLuUsed(false), setNonElastRBM(false)
{}

void RigidBodyModes::
setPiroPL(const Teuchos::RCP<Teuchos::ParameterList>& piroParams)
{
  const Teuchos::RCP<Teuchos::ParameterList>
    stratList = Piro::extractStratimikosParams(piroParams);

  mlUsed = mueLuUsed = false;
  if (Teuchos::nonnull(stratList) &&
      stratList->isParameter("Preconditioner Type")) {
    const std::string&
      ptype = stratList->get<std::string>("Preconditioner Type");
    if (ptype == "ML") {
      plist = sublist(sublist(sublist(stratList, "Preconditioner Types"),
                              ptype), "ML Settings");
      mlUsed = true;
    }
    else if (ptype == "MueLu") {
      plist = sublist(sublist(stratList, "Preconditioner Types"), ptype);
      mueLuUsed = true;
    }
  }
}

void RigidBodyModes::
updatePL(const Teuchos::RCP<Teuchos::ParameterList>& mlParams)
{
  plist = mlParams;
}

void RigidBodyModes::setParameters(
  const int numPDEs_, const int numElasticityDim_, const int numScalar_,
  const int nullSpaceDim_, const bool setNonElastRBM_)
{
  numPDEs = numPDEs_;
  numElasticityDim = numElasticityDim_;
  numScalar = numScalar_;
  nullSpaceDim = nullSpaceDim_;
  setNonElastRBM = setNonElastRBM_;
}

void RigidBodyModes::
setCoordinates(const Teuchos::RCP<Thyra_MultiVector>& coordMV_)
{
  coordMV = coordMV_;

  TEUCHOS_TEST_FOR_EXCEPTION(
    !isMLUsed() && !isMueLuUsed(),
    std::logic_error,
    "setCoordinates was called without setting an ML or MueLu parameter list.");

  int numSpaceDim = coordMV->domain()->dim(); // Number of multivectors are the dimension of the problem

  if (isMLUsed()) { // ML here

    //MP: Even when a processor has no nodes, ML requires a nonnull pointer of coordinates.
    double emptyCoords[3] ={0.0, 0.0, 0.0};

    double* x = getNonconstLocalData(coordMV->col(0)).getRawPtr();

    if (x==nullptr) {
      x = &emptyCoords[0];
    }
    plist->set<double*>("x-coordinates", x);
    if(numSpaceDim > 1){
      double* y = getNonconstLocalData(coordMV->col(1)).getRawPtr();
      if (y==nullptr) {
        y = &emptyCoords[1];
      }
      plist->set<double*>("y-coordinates", y);
    } else {
      plist->set<double*>("y-coordinates", nullptr);
    }
    if(numSpaceDim > 2){
      double* z = getNonconstLocalData(coordMV->col(2)).getRawPtr();
      if (z==nullptr) {
        z = &emptyCoords[2];
      }
      plist->set<double*>("z-coordinates", z);
    } else {
      plist->set<double*>("z-coordinates", nullptr);
    }

    plist->set("PDE equations", numPDEs);

  } else {  // MueLu here
    if (plist->isSublist("Factories") == true) {
      // use verbose input deck
      Teuchos::ParameterList& matrixList = plist->sublist("Matrix");
      matrixList.set("PDE equations", numPDEs);
      plist->set("Coordinates", coordMV);
    } else {
      // use simplified input deck
      plist->set("Coordinates", coordMV);
      plist->set("number of equations", numPDEs);
    }
  }
}

void RigidBodyModes::
setCoordinatesAndNullspace(const Teuchos::RCP<Thyra_MultiVector>& coordMV_in,
                           const Teuchos::RCP<const Thyra_VectorSpace>& soln_vs)
{
  // numPDEs = # PDEs
  // numElasticityDim = # elasticity dofs
  // nullSpaceDim = dimension of elasticity nullspace
  // numScalar = # scalar dofs coupled to elasticity

  int numSpaceDim = coordMV->domain()->dim(); // Number of multivectors are the dimension of the problem
  const RigidBodyModes::LO_type numNodes = getSpmdVectorSpace(coordMV->range())->localSubDim();

  setCoordinates(coordMV_in);

  if (numElasticityDim > 0 || setNonElastRBM == true ) {

    if (isMLUsed()) {

     if(nullSpaceDim > 0) {
       if (setNonElastRBM == true) {
         err.resize((nullSpaceDim + numScalar) * numSpaceDim * numNodes);
       }
       else {
         err.resize((nullSpaceDim + numScalar) * numPDEs * numNodes);
       }
      }

      subtractCentroid(coordMV);

      if (setNonElastRBM == true)
        Coord2RBM_nonElasticity<Epetra_NullSpace_Traits>(coordMV, numPDEs, numScalar, nullSpaceDim, err);
      else
        Coord2RBM<Epetra_NullSpace_Traits>(coordMV, numPDEs, numScalar, nullSpaceDim, err);

      plist->set("null space: type", "pre-computed");
      plist->set("null space: dimension", nullSpaceDim + numScalar);
      plist->set("null space: vectors", &err[0]);
      plist->set("null space: add default vectors", false);

    } else {

      trr = Teuchos::rcp(new Tpetra_NullSpace_Traits::base_array_type(getTpetraMap(soln_vs),
                               nullSpaceDim + numScalar, false));

      subtractCentroid(coordMV);

      if (setNonElastRBM == true)
        Coord2RBM_nonElasticity<Tpetra_NullSpace_Traits>(coordMV, numPDEs, numScalar, nullSpaceDim, trr);
      else
        Coord2RBM<Tpetra_NullSpace_Traits>(coordMV, numPDEs, numScalar, nullSpaceDim, trr);

      TEUCHOS_TEST_FOR_EXCEPTION(
        soln_vs.is_null(), std::logic_error,
        "numElasticityDim > 0 and isMueLuUsed(): soln_map must be provided.");
      plist->set("Nullspace", trr);
    }
  }
}

} // namespace Albany
