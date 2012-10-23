//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef PHAL_ODERESID_HPP
#define PHAL_ODERESID_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "PHAL_AlbanyTraits.hpp"

#include "PHAL_Dimension.hpp"

#include "Teuchos_ParameterList.hpp"

namespace PHAL {

template<typename EvalT, typename Traits>
class ODEResid : 
  public PHX::EvaluatorWithBaseImpl<Traits>,
  public PHX::EvaluatorDerived<EvalT, Traits> {
  
public:
  
  ODEResid(Teuchos::ParameterList& p);
  
  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);
  
  void evaluateFields(typename Traits::EvalData ud);
  
private:
  
  typedef typename EvalT::ScalarT ScalarT;

  PHX::MDField<ScalarT,Cell,Node> X;
  PHX::MDField<ScalarT,Cell,Node> X_dot;
  PHX::MDField<ScalarT,Cell,Node> Y;
  PHX::MDField<ScalarT,Cell,Node> Y_dot;
  PHX::MDField<ScalarT,Cell,Node> Xoderesid;
  PHX::MDField<ScalarT,Cell,Node> Yoderesid;

}; 
}

#endif
