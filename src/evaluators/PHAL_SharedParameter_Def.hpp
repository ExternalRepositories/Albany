//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <vector>
#include <string>

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Sacado_ParameterRegistration.hpp"

namespace PHAL {

template<typename EvalT, typename Traits>
SharedParameter<EvalT, Traits>::
SharedParameter(const Teuchos::ParameterList& p) 
{  
  paramName =  p.get<std::string>("Parameter Name");
  paramValue =  p.get<double>("Parameter Value");

  Teuchos::RCP<PHX::DataLayout> layout =
      p.get< Teuchos::RCP<PHX::DataLayout> >("Data Layout");

  //! Initialize field with same name as parameter
  PHX::MDField<ScalarT,Dim> f(paramName, layout);
  paramAsField = f;

  // Sacado-ized parameter
  Teuchos::RCP<ParamLib> paramLib =
    p.get< Teuchos::RCP<ParamLib> >("Parameter Library"); //, Teuchos::null ANDY - why a compiler error with this?
  new Sacado::ParameterRegistration<EvalT, SPL_Traits>(
      paramName, this, paramLib);

  this->addEvaluatedField(paramAsField);
  this->setName("Shared Parameter" );
}

// **********************************************************************
template<typename EvalT, typename Traits> 
void SharedParameter<EvalT, Traits>::postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(paramAsField,fm);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void SharedParameter<EvalT, Traits>::evaluateFields(typename Traits::EvalData workset)
{
  paramAsField(0) = paramValue;
}

// **********************************************************************
template<typename EvalT,typename Traits>
typename SharedParameter<EvalT,Traits>::ScalarT& 
SharedParameter<EvalT,Traits>::getValue(const std::string &n)
{
  TEUCHOS_TEST_FOR_EXCEPT(n != paramName);
  return paramValue;
}

// **********************************************************************
}


