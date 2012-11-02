//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef PHAL_LINCOMPRNSRESID_HPP
#define PHAL_LINCOMPRNSRESID_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

namespace PHAL {
/** \brief Finite Element Interpolation Evaluator

    This evaluator interpolates nodal DOF values to quad points.

*/

template<typename EvalT, typename Traits>
class LinComprNSResid : public PHX::EvaluatorWithBaseImpl<Traits>,
		        public PHX::EvaluatorDerived<EvalT, Traits>  {

public:

  LinComprNSResid(const Teuchos::ParameterList& p);

  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  // Input:
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint> wBF;
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> wGradBF;

  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim> C;
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim,Dim> Cgrad;
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim> CDot;
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim> force;
  
  Teuchos::Array<double> baseFlowData;  
  double gamma_gas; 
  
  // Output:
  PHX::MDField<ScalarT,Cell,Node,VecDim> Residual;


  std::size_t numNodes;
  std::size_t numQPs;
  std::size_t numDims;
  std::size_t vecDim;
  bool enableTransient;
  enum EQNTYPE {EULER, NS}; 
  EQNTYPE eqn_type;  
};
}

#endif
