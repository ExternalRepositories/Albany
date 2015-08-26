//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef SURFACESCALARJUMP_HPP
#define SURFACESCALARJUMP_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Intrepid_CellTools.hpp"
#include "Intrepid_Cubature.hpp"

#include "Albany_Layouts.hpp"

namespace LCM {
/** \brief

    Compute the scalar jump between
    two planes of the localization element

**/

template<typename EvalT, typename Traits>
class SurfaceScalarJump : public PHX::EvaluatorWithBaseImpl<Traits>,
                          public PHX::EvaluatorDerived<EvalT, Traits>  {

public:

  SurfaceScalarJump(const Teuchos::ParameterList& p,
		            const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  // Input:
  //! Numerical integration rule
  Teuchos::RCP<Intrepid::Cubature<RealType>> cubature;
  //! Finite element basis for the midplane
  Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType>>> intrepidBasis;


  //! Nodal value of scalar
  PHX::MDField<ScalarT,Cell,Vertex> scalar;

  PHX::MDField<ScalarT,Cell,Vertex> nodalTemperature;
  PHX::MDField<ScalarT,Cell,Vertex> nodalTransport;
  PHX::MDField<ScalarT,Cell,Vertex> nodalHydroStress;
  PHX::MDField<ScalarT,Cell,Vertex> nodalPorePressure;

  // Reference Cell FieldContainers
  Intrepid::FieldContainer<RealType> refValues;
  Intrepid::FieldContainer<RealType> refGrads;
  Intrepid::FieldContainer<RealType> refPoints;
  Intrepid::FieldContainer<RealType> refWeights;

  // Output:
  PHX::MDField<ScalarT,Cell,QuadPoint> scalarJump;
  PHX::MDField<ScalarT,Cell,QuadPoint> scalarAverage;

  PHX::MDField<ScalarT,Cell,QuadPoint> midPlaneTemperature;
  PHX::MDField<ScalarT,Cell,QuadPoint> midPlaneTransport;
  PHX::MDField<ScalarT,Cell,QuadPoint> midPlaneHydroStress;
  PHX::MDField<ScalarT,Cell,QuadPoint> midPlanePorePressure;

  PHX::MDField<ScalarT,Cell,QuadPoint> jumpTemperature;
  PHX::MDField<ScalarT,Cell,QuadPoint> jumpTransport;
  PHX::MDField<ScalarT,Cell,QuadPoint> jumpHydroStress;
  PHX::MDField<ScalarT,Cell,QuadPoint> jumpPorePressure;

  bool haveTemperature, haveTransport, haveHydroStress, havePorePressure;

  unsigned int worksetSize;
  unsigned int numNodes;
  unsigned int numQPs;
  unsigned int numDims;
  unsigned int numPlaneNodes;
  unsigned int numPlaneDims;
};
}

#endif
