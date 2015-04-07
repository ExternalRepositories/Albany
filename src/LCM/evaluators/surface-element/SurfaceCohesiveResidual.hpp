//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef SURFACE_COHESIVE_RESIDUAL_HPP
#define SURFACE_COHESIVE_RESIDUAL_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Intrepid_CellTools.hpp"
#include "Intrepid_Cubature.hpp"

#include "Albany_Layouts.hpp"

namespace LCM {
///
///    Compute the residual forces on a surface based on cohesive traction
///

template<typename EvalT, typename Traits>
class SurfaceCohesiveResidual:
    public PHX::EvaluatorWithBaseImpl<Traits>,
    public PHX::EvaluatorDerived<EvalT, Traits> {

public:

  SurfaceCohesiveResidual(Teuchos::ParameterList const & p,
      Teuchos::RCP<Albany::Layouts> const & dl);

  void postRegistrationSetup(typename Traits::SetupData d,
      PHX::FieldManager<Traits> & vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  // Input:
  // Numerical integration rule
  Teuchos::RCP<Intrepid::Cubature<RealType> >
  cubature_;

  // Finite element basis for the midplane
  Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
  intrepid_basis_;

  // Reference area
  PHX::MDField<ScalarT, Cell, QuadPoint>
  ref_area_;

  // Traction vector based on cohesive-separation law
  PHX::MDField<ScalarT, Cell, QuadPoint, Dim>
  cohesiveTraction;

  // Reference Cell FieldContainers
  Intrepid::FieldContainer<RealType>
  refValues;

  Intrepid::FieldContainer<RealType>
  refGrads;

  Intrepid::FieldContainer<RealType>
  refPoints;

  Intrepid::FieldContainer<RealType>
  refWeights;

  // Output:
  PHX::MDField<ScalarT, Cell, Node, Dim> force;

  unsigned int worksetSize;
  unsigned int numNodes;
  unsigned int numQPs;
  unsigned int numDims;
  unsigned int numPlaneNodes;
  unsigned int numPlaneDims;
};
}

#endif
