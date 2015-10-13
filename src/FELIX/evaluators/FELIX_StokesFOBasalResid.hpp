//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef FELIX_STOKES_FO_BASAL_RESID_HPP
#define FELIX_STOKES_FO_BASAL_RESID_HPP 1

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
class StokesFOBasalResid : public PHX::EvaluatorWithBaseImpl<Traits>,
                          public PHX::EvaluatorDerived<EvalT, Traits>
{
public:

  typedef typename EvalT::ScalarT ScalarT;

  StokesFOBasalResid (const Teuchos::ParameterList& p,
                      const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup (typename Traits::SetupData d,
                              PHX::FieldManager<Traits>& fm);

  void evaluateFields(typename Traits::EvalData d);

private:

  ScalarT printedFF;

  // Input:
  PHX::MDField<ScalarT,Cell,Side,QuadPoint>         beta;
  PHX::MDField<ScalarT,Cell,Side,QuadPoint,VecDim>  u;
  PHX::MDField<RealType,Cell,Side,Node,QuadPoint>   BF;
  PHX::MDField<RealType,Cell,Side,QuadPoint>        w_measure;

  // Output:
  PHX::MDField<ScalarT,Cell,Node,VecDim>            basalResid;

  std::string                     basalSideName;
  std::vector<std::vector<int> >  sideNodes;

  int numCellNodes;
  int numSideNodes;
  int numSideQPs;
  int sideDim;
  int vecDim;
};

} // Namespace FELIX

#endif // FELIX_STOKES_FO_BASAL_RESID_HPP
