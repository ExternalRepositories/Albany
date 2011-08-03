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


#include <fstream>
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Sacado_ParameterRegistration.hpp"
#include "Intrepid_FunctionSpaceTools.hpp"

namespace PHAL {

template<typename EvalT, typename Traits>
JouleHeating<EvalT, Traits>::
JouleHeating(Teuchos::ParameterList& p) :
  potentialGrad(p.get<std::string>("Gradient Variable Name"),
      p.get<Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout")),
  potentialFlux(p.get<std::string>("Flux Variable Name"),
		p.get<Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout")),
  jouleHeating(p.get<std::string>("Source Name"),
      p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout"))
{
  Teuchos::RCP<PHX::DataLayout> vector_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  this->addEvaluatedField(jouleHeating);
  this->addDependentField(potentialGrad);
  this->addDependentField(potentialFlux);
  this->setName("Joule Heating"+PHX::TypeString<EvalT>::value);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void JouleHeating<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(jouleHeating,fm);
  this->utils.setFieldData(potentialGrad,fm);
  this->utils.setFieldData(potentialFlux,fm);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void JouleHeating<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Intrepid::FunctionSpaceTools::dotMultiplyDataData<ScalarT>
                 (jouleHeating, potentialFlux, potentialGrad);
}
// **********************************************************************
// **********************************************************************
}
