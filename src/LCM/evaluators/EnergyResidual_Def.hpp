//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include "Intrepid_FunctionSpaceTools.hpp"
#include "Intrepid_RealSpaceTools.hpp"

#include <typeinfo>

namespace LCM {

//**********************************************************************
template<typename EvalT, typename Traits>
EnergyResidual<EvalT, Traits>::
EnergyResidual(const Teuchos::ParameterList& p,
               const Teuchos::RCP<Albany::Layouts>& dl) :
  wBF         (p.get<std::string>("Weighted BF Name"),dl->node_qp_scalar),
  Temperature (p.get<std::string>("Variable Name"),dl->qp_scalar),
  ThermalCond (p.get<std::string>("Thermal Conductivity Name"),dl->qp_scalar),
  wGradBF     (p.get<std::string>("Weighted Gradient BF Name"),dl->node_qp_vector),
  TGrad       (p.get<std::string>("Gradient QP Variable Name"),dl->qp_vector),
  Source      (p.get<std::string>("Source Name"),dl->qp_scalar),
  F           (p.get<std::string>("Deformation Gradient Name"),dl->qp_tensor),
  mechSource  (p.get<std::string>("Mechanical Source Name"),dl->qp_scalar),
  deltaTime   (p.get<std::string>("Delta Time Name"),dl->workset_scalar),
  density     (p.get<RealType>("Density") ),
  Cv          (p.get<RealType>("Heat Capacity") ),
  TResidual   (p.get<std::string>("Residual Name"),dl->node_scalar),
  haveSource  (p.get<bool>("Have Source") )
{
  this->addDependentField(wBF);
  this->addDependentField(Temperature);
  this->addDependentField(ThermalCond);
  this->addDependentField(TGrad);
  this->addDependentField(wGradBF);
  this->addDependentField(F);
  this->addDependentField(mechSource);
  this->addDependentField(deltaTime);
  if (haveSource) this->addDependentField(Source);
  this->addEvaluatedField(TResidual);

  tempName = p.get<std::string>("QP Variable Name")+"_old";

  Teuchos::RCP<PHX::DataLayout> vector_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  worksetSize = dims[0];
  numQPs  = dims[1];
  numDims = dims[2];

  // Allocate workspace
  flux.resize(dims[0], numQPs, numDims);
  C.resize(worksetSize, numQPs, numDims, numDims);
  Cinv.resize(worksetSize, numQPs, numDims, numDims);
  CinvTgrad.resize(worksetSize, numQPs, numDims);
  Tdot.resize(worksetSize, numQPs);

  this->setName("EnergyResidual"+PHX::TypeString<EvalT>::value);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void EnergyResidual<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(wBF,fm);
  this->utils.setFieldData(Temperature,fm);
  this->utils.setFieldData(ThermalCond,fm);
  this->utils.setFieldData(TGrad,fm);
  this->utils.setFieldData(wGradBF,fm);
  this->utils.setFieldData(F,fm);
  this->utils.setFieldData(mechSource,fm);
  this->utils.setFieldData(deltaTime,fm);
  if (haveSource)  this->utils.setFieldData(Source,fm);

  this->utils.setFieldData(TResidual,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void EnergyResidual<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  bool print = false;
  //if (typeid(ScalarT) == typeid(RealType)) print = true;

  // alias the function space tools
  typedef Intrepid::FunctionSpaceTools FST;

  // get old temperature
  Albany::MDArray Temperature_old = (*workset.stateArrayPtr)[tempName];

  // time step
  ScalarT dt = deltaTime(0);

  // compute the 'material' flux
  FST::tensorMultiplyDataData<ScalarT> (C, F, F, 'T');
  Intrepid::RealSpaceTools<ScalarT>::inverse(Cinv, C);
  FST::tensorMultiplyDataData<ScalarT> (CinvTgrad, Cinv, TGrad);
  FST::scalarMultiplyDataData<ScalarT> (flux, ThermalCond, CinvTgrad);

  FST::integrate<ScalarT>(TResidual, flux, wGradBF, Intrepid::COMP_CPP, false); // "false" overwrites

  if (haveSource) {
    for (int i=0; i<Source.size(); i++) Source[i] *= -1.0;
    FST::integrate<ScalarT>(TResidual, Source, wBF, Intrepid::COMP_CPP, true); // "true" sums into
  }

  for (int i=0; i<mechSource.size(); i++) mechSource[i] *= -1.0;
  FST::integrate<ScalarT>(TResidual, mechSource, wBF, Intrepid::COMP_CPP, true); // "true" sums into

  //if (workset.transientTerms && enableTransient)
  //  FST::integrate<ScalarT>(TResidual, Tdot, wBF, Intrepid::COMP_CPP, true); // "true" sums into
  //
  // compute factor
  ScalarT fac(0.0);
  if (dt > 0.0)
    fac = ( density * Cv ) / dt;

  for (std::size_t cell=0; cell < workset.numCells; ++cell)
    for (std::size_t qp=0; qp < numQPs; ++qp)
      Tdot(cell,qp) = fac * ( Temperature(cell,qp) - Temperature_old(cell,qp) );

  FST::integrate<ScalarT>(TResidual, Tdot, wBF, Intrepid::COMP_CPP, true); // "true" sums into

  if (print)
  {
    cout << " *** EnergyResidual *** " << endl;
    cout << "  **   dt: " << dt << endl;
    cout << "  **  rho: " << density << endl;
    cout << "  **   Cv: " << Cv << endl;
    for (unsigned int cell(0); cell < workset.numCells; ++cell)
    {
      cout << "  ** Cell: " << cell << endl;
      for (unsigned int qp(0); qp < numQPs; ++qp)
      {
        cout << "   * QP: " << endl;
       cout << "    F   : ";
       for (unsigned int i(0); i < numDims; ++i)
         for (unsigned int j(0); j < numDims; ++j)
           cout << F(cell,qp,i,j) << " ";
       cout << endl;

        cout << "    C   : ";
        for (unsigned int i(0); i < numDims; ++i)
          for (unsigned int j(0); j < numDims; ++j)
            cout << C(cell,qp,i,j) << " ";
        cout << endl;

        cout << "    T   : " << Temperature(cell,qp) << endl;
        cout << "    Told: " << Temperature(cell,qp) << endl;
        cout << "    k   : " << ThermalCond(cell,qp) << endl;
      }
    }
  }
}

//**********************************************************************
}
