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


//**********************************************************************
template<typename EvalT, typename Traits>
Neohookean<EvalT, Traits>::
Neohookean(const Teuchos::ParameterList& p) :
  lcg              (p.get<std::string>                   ("LCG Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") ),
  stress           (p.get<std::string>                   ("Stress Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") ),
  elasticModulus   (p.get<std::string>                   ("Elastic Modulus Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  poissonsRatio    (p.get<std::string>                   ("Poissons Ratio Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  J                (p.get<std::string>                   ("DetDefGrad Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") )
{
  // Pull out numQPs and numDims from a Layout
  Teuchos::RCP<PHX::DataLayout> tensor_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  tensor_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  this->addDependentField(lcg);
  this->addDependentField(J);
  this->addDependentField(elasticModulus);
  // PoissonRatio not used in 1D stress calc
  if (numDims>1) this->addDependentField(poissonsRatio);

  this->addEvaluatedField(stress);

  this->setName("Stress");

}

//**********************************************************************
template<typename EvalT, typename Traits>
void Neohookean<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(stress,fm);
  this->utils.setFieldData(lcg,fm);
  this->utils.setFieldData(J,fm);
  this->utils.setFieldData(elasticModulus,fm);
  if (numDims>1) this->utils.setFieldData(poissonsRatio,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void Neohookean<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  ScalarT kappa;
  ScalarT mu;
  ScalarT Jm53;
  switch (numDims) {
  case 1:
    Intrepid::FunctionSpaceTools::tensorMultiplyDataData<ScalarT>(stress, elasticModulus, lcg);
    break;
  case 2:
    // Compute Stress (with the plane strain assumption for now)
    for (std::size_t cell=0; cell < workset.numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
	kappa = elasticModulus(cell,qp) / ( 3. * ( 1. - 2. * poissonsRatio(cell,qp) ) );
	mu    = elasticModulus(cell,qp) / ( 2. * ( 1. - poissonsRatio(cell,qp) ) );
	Jm53  = std::pow(J(cell,qp), -5./3.);
	stress(cell,qp,0,0) = 0.5 * kappa * ( J(cell,qp) - 1. / J(cell,qp) ) 
	  + mu * Jm53 * ( lcg(cell,qp,0,0) - ( 1. / 3. ) * ( lcg(cell,qp,0,0) + lcg(cell,qp,1,1) ) );
	stress(cell,qp,1,1) = 0.5 * kappa * ( J(cell,qp) - 1. / J(cell,qp) ) 
	  + mu * Jm53 * ( lcg(cell,qp,1,1) - ( 1. / 3. ) * ( lcg(cell,qp,0,0) + lcg(cell,qp,1,1) ) );
	stress(cell,qp,0,1) = mu * Jm53 * ( lcg(cell,qp,0,1) );
	stress(cell,qp,1,0) = stress(cell,qp,0,1); 
      }
    }
    break;
  case 3:
    // Compute Stress
    for (std::size_t cell=0; cell < workset.numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
	kappa = elasticModulus(cell,qp) / ( 3. * ( 1. - 2. * poissonsRatio(cell,qp) ) );
	mu    = elasticModulus(cell,qp) / ( 2. * ( 1. - poissonsRatio(cell,qp) ) );
	Jm53  = std::pow(J(cell,qp), -5./3.);
	stress(cell,qp,0,0) = 0.5 * kappa * ( J(cell,qp) - 1. / J(cell,qp) ) 
	  + mu * Jm53 * ( lcg(cell,qp,0,0) - ( 1. / 3. ) * ( lcg(cell,qp,0,0) + lcg(cell,qp,1,1) + lcg(cell,qp,2,2) ) );
	stress(cell,qp,1,1) = 0.5 * kappa * ( J(cell,qp) - 1. / J(cell,qp) ) 
	  + mu * Jm53 * ( lcg(cell,qp,1,1) - ( 1. / 3. ) * ( lcg(cell,qp,0,0) + lcg(cell,qp,1,1) + lcg(cell,qp,2,2) ) );
	stress(cell,qp,2,2) = 0.5 * kappa * ( J(cell,qp) - 1. / J(cell,qp) ) 
	  + mu * Jm53 * ( lcg(cell,qp,2,2) - ( 1. / 3. ) * ( lcg(cell,qp,0,0) + lcg(cell,qp,1,1) + lcg(cell,qp,2,2) ) );
	stress(cell,qp,0,1) = mu * Jm53 * ( lcg(cell,qp,0,1) );
	stress(cell,qp,1,2) = mu * Jm53 * ( lcg(cell,qp,1,2) );
	stress(cell,qp,2,0) = mu * Jm53 * ( lcg(cell,qp,2,0) );
	stress(cell,qp,1,0) = stress(cell,qp,0,1);
	stress(cell,qp,2,1) = stress(cell,qp,1,2);
	stress(cell,qp,0,2) = stress(cell,qp,2,0);
      }
    }
    break;
  }
}

//**********************************************************************
