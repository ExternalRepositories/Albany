//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef MIXTURE_THERMAL_EXPANSION_HPP
#define MIXTURE_THERMAL_EXPANSION_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

namespace LCM {
/** \brief

    This evaluator calculates thermal expansion of a bi-phase
    mixture through volume averaging


*/

template<typename EvalT, typename Traits>
class MixtureThermalExpansion : public PHX::EvaluatorWithBaseImpl<Traits>,
	       public PHX::EvaluatorDerived<EvalT, Traits>  {

public:

  MixtureThermalExpansion(const Teuchos::ParameterList& p);

  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  // Input:
  PHX::MDField<ScalarT,Cell,QuadPoint> biotCoefficient;
  PHX::MDField<ScalarT,Cell,QuadPoint> porosity;
  PHX::MDField<ScalarT,Cell,QuadPoint> alphaSkeleton;
  PHX::MDField<ScalarT,Cell,QuadPoint> alphaPoreFluid;

  // Output:
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim,Dim> mixtureThermalExpansion;

  unsigned int numQPs;
  unsigned int numDims;
};
}

#endif
