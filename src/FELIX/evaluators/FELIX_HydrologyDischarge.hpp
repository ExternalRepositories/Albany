//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef FELIX_HYDROLOGY_DISCHARGE_HPP
#define FELIX_HYDROLOGY_DISCHARGE_HPP 1

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Albany_Layouts.hpp"

namespace FELIX
{

/** \brief Hydrology Residual Evaluator

    This evaluator evaluates the residual of the Hydrology model
*/

template<typename EvalT, typename Traits>
class HydrologyDischarge : public PHX::EvaluatorWithBaseImpl<Traits>,
                         public PHX::EvaluatorDerived<EvalT, Traits>
{
public:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::ParamScalarT ParamScalarT;

  HydrologyDischarge (const Teuchos::ParameterList& p,
                      const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup (typename Traits::SetupData d,
                              PHX::FieldManager<Traits>& fm);

  void evaluateFields(typename Traits::EvalData d);

private:

  // Input:
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim>  gradPhi;
  PHX::MDField<ParamScalarT,Cell,QuadPoint>  h;

  // Output:
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim>  q;

  int numQPs;
  int numDim;

  double mu_w;
  double k_0;
};

} // Namespace FELIX

#endif // FELIX_HYDROLOGY_DISCHARGE_HPP
