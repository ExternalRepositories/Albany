//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef PHAL_NODES_TO_CELL_INTERPOLATION_HPP
#define PHAL_NODES_TO_CELL_INTERPOLATION_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Albany_Layouts.hpp"

namespace PHAL
{
/** \brief Average from Qp to Cell

    This evaluator averages the quadrature points values to
    obtain a single value for the whole cell

*/

template<typename EvalT, typename Traits>
class NodesToCellInterpolation : public PHX::EvaluatorWithBaseImpl<Traits>,
                                      public PHX::EvaluatorDerived<EvalT, Traits>
{
public:

  NodesToCellInterpolation (const Teuchos::ParameterList& p,
                                 const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup (typename Traits::SetupData d,
                              PHX::FieldManager<Traits>& fm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::MeshScalarT MeshScalarT;
  typedef typename EvalT::ScalarT     ScalarT;

  int numNodes;
  int numQPs;
  int vecDim;

  bool isVectorField;

  // Input:
  PHX::MDField<ScalarT>                       field_node;
  PHX::MDField<RealType,Cell,Node,QuadPoint>  BF;
  PHX::MDField<MeshScalarT,Cell,Node>         w_measure;

  // Output:
  PHX::MDField<ScalarT>     field_cell;
};

} // Namespace PHAL

#endif // PHAL_NODES_TO_CELL_INTERPOLATION_HPP
