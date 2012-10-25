//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "LCM/utils/Tensor.h"

namespace LCM {

//**********************************************************************
template<typename EvalT, typename Traits>
Neohookean<EvalT, Traits>::
Neohookean(const Teuchos::ParameterList& p,
           const Teuchos::RCP<Albany::Layouts>& dl) :
  defGrad          (p.get<std::string>("DefGrad Name"), dl->qp_tensor),
  J                (p.get<std::string>("DetDefGrad Name"), dl->qp_scalar),
  elasticModulus   (p.get<std::string>("Elastic Modulus Name"), dl->qp_scalar),
  poissonsRatio    (p.get<std::string>("Poissons Ratio Name"), dl->qp_scalar),
  stress           (p.get<std::string>("Stress Name"), dl->qp_tensor)
{
  // Pull out numQPs and numDims from a Layout
  std::vector<PHX::DataLayout::size_type> dims;
  dl->qp_tensor->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];
  worksetSize = dims[0];

  this->addDependentField(defGrad);
  this->addDependentField(J);
  this->addDependentField(elasticModulus);
  this->addDependentField(poissonsRatio);

  this->addEvaluatedField(stress);

  this->setName("NeoHookean Stress"+PHX::TypeString<EvalT>::value);

}

//**********************************************************************
template<typename EvalT, typename Traits>
void Neohookean<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(stress,fm);
  this->utils.setFieldData(defGrad,fm);
  this->utils.setFieldData(J,fm);
  this->utils.setFieldData(elasticModulus,fm);
  this->utils.setFieldData(poissonsRatio,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void Neohookean<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  cout.precision(15);
  ScalarT kappa;
  ScalarT mu;
  ScalarT Jm53;
  
  for (std::size_t cell=0; cell < workset.numCells; ++cell) {
    for (std::size_t qp=0; qp < numQPs; ++qp) {
      kappa = elasticModulus(cell,qp) / ( 3. * ( 1. - 2. * poissonsRatio(cell,qp) ) );
      mu    = elasticModulus(cell,qp) / ( 2. * ( 1. + poissonsRatio(cell,qp) ) );
      Jm53  = std::pow(J(cell,qp), -5./3.);

      LCM::Tensor<ScalarT> F(numDims, &defGrad(cell,qp,0,0));
      LCM::Tensor<ScalarT> I(LCM::eye<ScalarT>(numDims));
      LCM::Tensor<ScalarT> b(F*transpose(F));
      LCM::Tensor<ScalarT> sigma = 0.5 * kappa * ( J(cell,qp) - 1. / J(cell,qp) ) * I
        + mu * Jm53 * LCM::dev(b);

      for (std::size_t i=0; i < numDims; ++i)
        for (std::size_t j=0; j < numDims; ++j)
          stress(cell,qp,i,j) = sigma(i,j);
    }
  }
}

//**********************************************************************
}

