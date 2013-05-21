//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef PNP_CONCENTRATIONRESID_HPP
#define PNP_CONCENTRATIONRESID_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Albany_Layouts.hpp"

namespace PNP {
/** \brief Finite Element Interpolation Evaluator

    This evaluator interpolates nodal DOF values to quad points.

*/

template<typename EvalT, typename Traits>
class ConcentrationResid : public PHX::EvaluatorWithBaseImpl<Traits>,
		    public PHX::EvaluatorDerived<EvalT, Traits>  {

public:

  ConcentrationResid(const Teuchos::ParameterList& p,
                 const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  // Input:
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint> wBF;
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> wGradBF;
  //PHX::MDField<ScalarT,Cell,QuadPoint> Potential;
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim> PotentialGrad;
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim> Concentration;
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim> Concentration_dot;
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim> ConcentrationGrad;
  //PHX::MDField<ScalarT,Cell,QuadPoint> Permittivity;
  //PHX::MDField<ScalarT,Cell,QuadPoint,Dim> PhiGrad;
  //PHX::MDField<ScalarT,Cell,QuadPoint,Dim> PhiFlux;

  // Output:
  PHX::MDField<ScalarT,Cell,Node,VecDim> ConcentrationResidual;

  int numNodes, numQPs, numDims, numSpecies;
  std::vector<double> D,beta; // Placeholder for charges
};
}

#endif
