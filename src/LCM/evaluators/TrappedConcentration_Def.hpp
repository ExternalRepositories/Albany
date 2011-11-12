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

namespace LCM {

//**********************************************************************
template<typename EvalT, typename Traits>
TrappedConcentration<EvalT, Traits>::
TrappedConcentration(const Teuchos::ParameterList& p) :
  Vmolar       (p.get<std::string>                   ("Molar Volume Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  Clattice       (p.get<std::string>                   ("Lattice Concentration Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  Keq       (p.get<std::string>                   ("Equilibrium Constant Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  Ntrap       (p.get<std::string>                   ("Trapped Solvent Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  Ctrapped      (p.get<std::string>      ("Trapped Concentration Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") )
{
  this->addDependentField(Vmolar);
  this->addDependentField(Keq);
  this->addDependentField(Ntrap);
  this->addDependentField(Clattice);

  this->addEvaluatedField(Ctrapped);

  this->setName("Trapped Concentration"+PHX::TypeString<EvalT>::value);

  Teuchos::RCP<PHX::DataLayout> scalar_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  scalar_dl->dimensions(dims);
  numQPs  = dims[1];
}

//**********************************************************************
template<typename EvalT, typename Traits>
void TrappedConcentration<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(Ctrapped,fm);
  this->utils.setFieldData(Vmolar,fm);
  this->utils.setFieldData(Keq,fm);
  this->utils.setFieldData(Ntrap,fm);
  this->utils.setFieldData(Clattice,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void TrappedConcentration<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  // Compute Strain tensor from displacement gradient
  for (std::size_t cell=0; cell < workset.numCells; ++cell) {
    for (std::size_t qp=0; qp < numQPs; ++qp) {

    	// Nlattice = avogadroNUM(cell,qp)/Vmolar(cell,qp);
    	Nlattice = 1.0/Vmolar(cell,qp);

    	Ctrapped(cell,qp) = Ntrap(cell,qp)*Keq(cell,qp)*Clattice(cell,qp)/
    			            ( Keq(cell,qp)*Clattice(cell,qp) + Nlattice );



    }
  }

}

//**********************************************************************
}

