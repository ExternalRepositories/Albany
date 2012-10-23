//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef SURFACESCALARGRADIENT_HPP
#define SURFACESCALARGRADIENT_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Intrepid_CellTools.hpp"
#include "Intrepid_Cubature.hpp"

#include "Albany_Layouts.hpp"

namespace LCM {
/** \brief

    Construct a deformation gradient on a surface

**/

template<typename EvalT, typename Traits>
class SurfaceScalarGradient : public PHX::EvaluatorWithBaseImpl<Traits>,
                          public PHX::EvaluatorDerived<EvalT, Traits>  {

public:



  SurfaceScalarGradient(const Teuchos::ParameterList& p,
                        const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);



private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  // Input:
  //! Length scale parameter for localization zone
  ScalarT thickness;
  //! Numerical integration rule
  Teuchos::RCP<Intrepid::Cubature<RealType> > cubature;

  //! for the parallel gradient term
  Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > intrepidBasis;
  //! Cell Topology
  Teuchos::RCP<shards::CellTopology> cellType;
  // nodal value used to construct in-plan gradient
  PHX::MDField<ScalarT,Cell,Node> nodalScalar;

  //! Vector to take the jump of
  PHX::MDField<MeshScalarT,Cell,Vertex,Dim> vector;
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim> jump;

  PHX::MDField<ScalarT,Cell,QuadPoint,Dim, Dim> currentBasis;
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim, Dim> refDualBasis;
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim> refNormal;
  PHX::MDField<MeshScalarT,Cell,QuadPoint> weights;

  //! Reference Cell FieldContainers
  Intrepid::FieldContainer<RealType> refValues;
  Intrepid::FieldContainer<RealType> refGrads;
  Intrepid::FieldContainer<RealType> refPoints;
  Intrepid::FieldContainer<RealType> refWeights;

  // Surface Ref Bases FieldContainers
  Intrepid::FieldContainer<ScalarT> midplaneScalar;

  // Output:
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim> scalarGrad;
  PHX::MDField<ScalarT,Cell,QuadPoint> J;

  unsigned int worksetSize;
  unsigned int numNodes;
  unsigned int numQPs;
  unsigned int numDims;
  unsigned int numPlaneNodes;
  unsigned int numPlaneDims;

  //! flag to compute the weighted average of J
  bool weightedAverage;

  //! stabilization parameter for the weighted average
  ScalarT alpha;

};
}

#endif
