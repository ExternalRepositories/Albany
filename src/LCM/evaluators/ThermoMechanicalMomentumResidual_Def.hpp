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
#include "Sacado_ParameterRegistration.hpp"

namespace LCM {

//**********************************************************************
template<typename EvalT, typename Traits>
ThermoMechanicalMomentumResidual<EvalT, Traits>::
ThermoMechanicalMomentumResidual(const Teuchos::ParameterList& p) :
  stress      (p.get<std::string>                   ("Stress Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") ),
  J           (p.get<std::string>                   ("DetDefGrad Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  defgrad     (p.get<std::string>                   ("DefGrad Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") ),
  wGradBF     (p.get<std::string>                   ("Weighted Gradient BF Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("Node QP Vector Data Layout") ),
  wBF         (p.get<std::string>                   ("Weighted BF Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("Node QP Scalar Data Layout") ),
  Residual    (p.get<std::string>                   ("Residual Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("Node Vector Data Layout") )
{
  this->addDependentField(stress);
  this->addDependentField(J);
  this->addDependentField(defgrad);
  this->addDependentField(wGradBF);
  this->addDependentField(wBF);

  this->addEvaluatedField(Residual);

  this->setName("ThermoMechanicalMomentumResidual"+PHX::TypeString<EvalT>::value);

  std::vector<PHX::DataLayout::size_type> dims;
  wGradBF.fieldTag().dataLayout().dimensions(dims);
  numNodes = dims[1];
  numQPs   = dims[2];
  numDims  = dims[3];
  int worksetSize = dims[0];

  // Works space FCs
  F_inv.resize(worksetSize, numQPs, numDims, numDims);
  F_invT.resize(worksetSize, numQPs, numDims, numDims);
  JF_invT.resize(worksetSize, numQPs, numDims, numDims);
  P.resize(worksetSize, numQPs, numDims, numDims);

  Teuchos::RCP<ParamLib> paramLib = p.get< Teuchos::RCP<ParamLib> >("Parameter Library");

  matModel = p.get<std::string>("Stress Name");

  zGrav=0.0;
  new Sacado::ParameterRegistration<EvalT, SPL_Traits>("zGrav", this, paramLib);

}

//**********************************************************************
template<typename EvalT, typename Traits>
void ThermoMechanicalMomentumResidual<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(stress,fm);
  this->utils.setFieldData(J,fm);
  this->utils.setFieldData(defgrad,fm);
  this->utils.setFieldData(wGradBF,fm);
  this->utils.setFieldData(wBF,fm);

  this->utils.setFieldData(Residual,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void ThermoMechanicalMomentumResidual<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  cout.precision(15);
  typedef Intrepid::FunctionSpaceTools FST;
  typedef Intrepid::RealSpaceTools<ScalarT> RST;

  RST::inverse(F_inv, defgrad);
  RST::transpose(F_invT, F_inv);
  FST::scalarMultiplyDataData<ScalarT>(JF_invT, J, F_invT);
  FST::tensorMultiplyDataData<ScalarT>(P, stress, JF_invT);
  for (std::size_t cell=0; cell < workset.numCells; ++cell) 
  {
    for (std::size_t node=0; node < numNodes; ++node) 
    {
      for (std::size_t dim=0; dim<numDims; dim++)  Residual(cell,node,dim)=0.0;
      for (std::size_t qp=0; qp < numQPs; ++qp) 
      {
	for (std::size_t i=0; i<numDims; i++) 
	{
	  for (std::size_t j=0; j<numDims; j++) 
	  {
	    Residual(cell,node,i) += P(cell, qp, i, j) * wGradBF(cell, node, qp, j);
	  } 
	} 
      } 
    } 
  }
}
// **********************************************************************
template<typename EvalT,typename Traits>
typename ThermoMechanicalMomentumResidual<EvalT,Traits>::ScalarT&
ThermoMechanicalMomentumResidual<EvalT,Traits>::getValue(const std::string &n)
{
  return zGrav;
}


//**********************************************************************
}

