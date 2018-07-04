//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Phalanx_TypeStrings.hpp"

namespace Tsunami {

//**********************************************************************
template<typename EvalT, typename Traits>
NavierStokesMomentumResid<EvalT, Traits>::
NavierStokesMomentumResid(const Teuchos::ParameterList& p,
                    const Teuchos::RCP<Albany::Layouts>& dl) :
  wBF       (p.get<std::string> ("Weighted BF Name"), dl->node_qp_scalar),
  wGradBF   (p.get<std::string> ("Weighted Gradient BF Name"), dl->node_qp_vector),
  pGrad     (p.get<std::string> ("Pressure Gradient QP Variable Name"), dl->qp_vector),
  VGrad     (p.get<std::string> ("Velocity Gradient QP Variable Name"), dl->qp_tensor),
  P         (p.get<std::string> ("Pressure QP Variable Name"), dl->qp_scalar),
  force     (p.get<std::string> ("Body Force Name"), dl->qp_vector),
  MResidual (p.get<std::string> ("Residual Name"),dl->node_vector),
  Rm        (p.get<std::string> ("Rm Name"), dl->qp_vector),
  haveSUPG  (p.get<bool>        ("Have SUPG"))
{

  if (haveSUPG) {
    TauSUPG = decltype(TauSUPG)(
      p.get<std::string>("Tau SUPG Name"), dl->qp_scalar);
    this->addDependentField(TauSUPG);
  }

  this->addDependentField(wBF);
  this->addDependentField(VGrad);
  this->addDependentField(pGrad);
  this->addDependentField(wGradBF);
  this->addDependentField(P);
  this->addDependentField(force);
  this->addDependentField(Rm);

  this->addEvaluatedField(MResidual);

  mu = p.get<double>("Viscosity"); 
  rho = p.get<double>("Density"); 
 
  std::vector<PHX::DataLayout::size_type> dims;
  dl->node_qp_vector->dimensions(dims);
  numNodes = dims[1];
  numQPs  = dims[2];
  numDims = dims[3];

  this->setName("NavierStokesMomentumResid"+PHX::typeAsString<EvalT>());
}

//**********************************************************************
template<typename EvalT, typename Traits>
void NavierStokesMomentumResid<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(wBF,fm);
  this->utils.setFieldData(VGrad,fm);
  this->utils.setFieldData(pGrad,fm);
  this->utils.setFieldData(wGradBF,fm);
  this->utils.setFieldData(P,fm);
  this->utils.setFieldData(force,fm);
  this->utils.setFieldData(Rm,fm);
  if (haveSUPG) {
    this->utils.setFieldData(TauSUPG,fm);
  }
  this->utils.setFieldData(MResidual,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void NavierStokesMomentumResid<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  for (int cell=0; cell < workset.numCells; ++cell) {
    for (int node=0; node < numNodes; ++node) {
      for (int i=0; i<numDims; i++) {
  MResidual(cell,node,i) = 0.0;
  for (int qp=0; qp < numQPs; ++qp) {
    MResidual(cell,node,i) +=
      (Rm(cell,qp,i) - pGrad(cell,qp,i))*wBF(cell,node,qp) -
       P(cell,qp)*wGradBF(cell,node,qp,i);
    for (int j=0; j < numDims; ++j) {
      MResidual(cell,node,i) +=
        mu*(VGrad(cell,qp,i,j)+VGrad(cell,qp,j,i))*wGradBF(cell,node,qp,j);
//        mu*VGrad(cell,qp,i,j)*wGradBF(cell,node,qp,j);
    }
  }
      }
    }
  }


}

}

