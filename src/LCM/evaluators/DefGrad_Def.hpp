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
DefGrad<EvalT, Traits>::
DefGrad(const Teuchos::ParameterList& p) :
  GradU         (p.get<std::string>                   ("Gradient QP Variable Name"),
	         p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") ),
  weights       (p.get<std::string>                   ("Weights Name"),
	         p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  defgrad       (p.get<std::string>                  ("DefGrad Name"),
	         p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") ),
  J             (p.get<std::string>                   ("DetDefGrad Name"),
	         p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  weightedAverage(false),
  alpha(0.05)
{
  if ( p.isType<string>("Weighted Volume Average J Name") )
    weightedAverage = p.get<bool>("Weighted Volume Average J");
  if ( p.isType<double>("Average J Stabilization Parameter Name") )
    alpha = p.get<double>("Average J Stabilization Parameter");

  Teuchos::RCP<PHX::DataLayout> tensor_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout");

  std::vector<PHX::DataLayout::size_type> dims;
  tensor_dl->dimensions(dims);
  worksetSize  = dims[0];
  numQPs  = dims[1];
  numDims = dims[2];

  this->addDependentField(GradU);
  this->addDependentField(weights);

  this->addEvaluatedField(defgrad);
  this->addEvaluatedField(J);

  this->setName("DefGrad"+PHX::TypeString<EvalT>::value);

}

//**********************************************************************
template<typename EvalT, typename Traits>
void DefGrad<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(weights,fm);
  this->utils.setFieldData(defgrad,fm);
  this->utils.setFieldData(J,fm);
  this->utils.setFieldData(GradU,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void DefGrad<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  //bool print = false;
  //if (typeid(ScalarT) == typeid(RealType)) print = true;

  // Compute DefGrad tensor from displacement gradient
  for (std::size_t cell=0; cell < workset.numCells; ++cell)
  {
    for (std::size_t qp=0; qp < numQPs; ++qp)
    {
      for (std::size_t i=0; i < numDims; ++i)
      {
        for (std::size_t j=0; j < numDims; ++j)
	{
          defgrad(cell,qp,i,j) = GradU(cell,qp,i,j);
        }
	defgrad(cell,qp,i,i) += 1.0;
      }
    }
  }
  // Since Intrepid will later perform calculations on the entire workset size
  // and not just the used portion, we must fill the excess with reasonable 
  // values. Leaving this out leads to inversion of 0 tensors.
  for (std::size_t cell=workset.numCells; cell < worksetSize; ++cell) 
    for (std::size_t qp=0; qp < numQPs; ++qp) 
      for (std::size_t i=0; i < numDims; ++i)
	defgrad(cell,qp,i,i) = 1.0;

  Intrepid::RealSpaceTools<ScalarT>::det(J, defgrad);

  if (weightedAverage)
  {
    ScalarT Jbar, wJbar, vol;
    for (std::size_t cell=0; cell < workset.numCells; ++cell)
    {
      Jbar = 0.0;
      vol = 0.0;
      for (std::size_t qp=0; qp < numQPs; ++qp)
      {
  	Jbar += weights(cell,qp) * std::log( J(cell,qp) );
  	vol  += weights(cell,qp);
      }
      Jbar /= vol;

      // Jbar = std::exp(Jbar);
      for (std::size_t qp=0; qp < numQPs; ++qp)
      {
  	for (std::size_t i=0; i < numDims; ++i)
  	{
  	  for (std::size_t j=0; j < numDims; ++j)
  	  {
            wJbar = std::exp( (1-alpha) * Jbar + alpha * std::log( J(cell,qp) ) );
  	    defgrad(cell,qp,i,j) *= std::pow( wJbar / J(cell,qp) ,1./3. );
  	  }
  	}
  	J(cell,qp) = wJbar;
      }
    }
  }
}

//**********************************************************************
}
