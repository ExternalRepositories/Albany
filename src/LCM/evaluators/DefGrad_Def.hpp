/********************************************************************\
*            Albany, Copyright (2010) Sandia Corporation             *
*                                                                    *
* Notice: This computer software was prepared by Sandia Corporation, *
* hereinafter the Contractor, under Contract DE-AC04-94AL85000 with  *
* the Department of Energy (DOE). All rights in the computer software*
* are reserved by DOE on behalf of the United States Government and  *
* the Contractor as provided in the Contract. You are authorized to  *
* use this computer software for Governmental purposes but it is not *
* to be released or distributed to the public. NEITHER THE GOVERNMENT*
* NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR      *
* ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This notice    *
* including this sentence must appear on any copies of this software.*
*    Questions to Andy Salinger, agsalin@sandia.gov                  *
\********************************************************************/


#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include "Intrepid_FunctionSpaceTools.hpp"
#include "Intrepid_RealSpaceTools.hpp"

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
  avgJ          (p.get<bool> ("avgJ Name")),
  volavgJ       (p.get<bool> ("volavgJ Name")),
  weighted_Volume_Averaged_J      (p.get<bool> ("weighted_Volume_Averaged_J Name"))
{
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

  if (avgJ)
  {
    ScalarT Jbar;
    for (std::size_t cell=0; cell < workset.numCells; ++cell)
    {
      Jbar = 0.0;
      for (std::size_t qp=0; qp < numQPs; ++qp)
      {
        //TEUCHOS_TEST_FOR_EXCEPTION(J(cell,qp) < 0, std::runtime_error,
        //    " negative volume detected in avgJ routine");
	Jbar += std::log(J(cell,qp));
        //Jbar += J(cell,qp);
      }
      Jbar /= numQPs;
      Jbar = std::exp(Jbar);
      for (std::size_t qp=0; qp < numQPs; ++qp)
      {
	for (std::size_t i=0; i < numDims; ++i)
	{
	  for (std::size_t j=0; j < numDims; ++j)
	  {
	    defgrad(cell,qp,i,j) *= std::pow(Jbar/J(cell,qp),1./3.);
	  }
	}
	J(cell,qp) = Jbar;
      }
    }
  }
  else if (volavgJ)
  {
    ScalarT Jbar, vol;
    for (std::size_t cell=0; cell < workset.numCells; ++cell)
    {
      Jbar = 0.0;
      vol = 0.0;
      for (std::size_t qp=0; qp < numQPs; ++qp)
      {
        //TEUCHOS_TEST_FOR_EXCEPTION(J(cell,qp) < 0, std::runtime_error,
        //    " negative volume detected in volavgJ routine");
	Jbar += weights(cell,qp) * std::log( J(cell,qp) );
	vol  += weights(cell,qp);
      }
      Jbar /= vol;
      Jbar = std::exp(Jbar);
      for (std::size_t qp=0; qp < numQPs; ++qp)
      {
	for (std::size_t i=0; i < numDims; ++i)
	{
	  for (std::size_t j=0; j < numDims; ++j)
	  {
	    defgrad(cell,qp,i,j) *= std::pow(Jbar/J(cell,qp),1./3.);
	  }
	}
	J(cell,qp) = Jbar;
      }
    }
  }
  else if (weighted_Volume_Averaged_J)
  {
    ScalarT Jbar, wJbar, vol;
    ScalarT StabAlpha = 0.5; // This setting need to change later..
    for (std::size_t cell=0; cell < workset.numCells; ++cell)
    {
      Jbar = 0.0;
      vol = 0.0;
      for (std::size_t qp=0; qp < numQPs; ++qp)
      {
        //TEUCHOS_TEST_FOR_EXCEPTION(J(cell,qp) < 0, std::runtime_error,
        //    " negative volume detected in volavgJ routine");
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
            wJbar =   std::exp( (1-StabAlpha)*Jbar+
                                StabAlpha*std::log(J(cell,qp)));

  	    defgrad(cell,qp,i,j) *= std::pow(wJbar /J(cell,qp),1./3.);
  	  }
  	}
  	J(cell,qp) = wJbar;
      }
    }
  }
}

//**********************************************************************
}
