/********************************************************************\
*            Albany, Copyright (2010) Sandia Corporation             *
*                                                                    *
* Notice: This computer software was prepared by Sandia Corporation, *
* hereinafter the Contractor, under Contract DE-AC04-94AL85000 with  *
* the Department of Energy (DOE). All rights in the computer software*
* are reserved by DOE on behalf of the United States Government and  *
* the Contractor as provided in the Contract. You are authorized to  *
* use this computer software for Governmental purposes but it is not *
* to be released or distributed to the public. NEITHER THE GOVERNMENT*
* NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR      *
* ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This notice    *
* including this sentence must appear on any copies of this software.*
*    Questions to Andy Salinger, agsalin@sandia.gov                  *
\********************************************************************/


#ifndef SCALAR_L2_PROJECTION_RESIDUAL_HPP
#define SCALAR_L2_PROJECTION_RESIDUAL_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Intrepid_CellTools.hpp"
#include "Intrepid_Cubature.hpp"

namespace LCM {
/** \brief Finite Element Interpolation Evaluator

    This evaluator computes residual of a scalar projection from Gauss points to nodes.

*/

template<typename EvalT, typename Traits>
class ScalarL2ProjectionResidual : public PHX::EvaluatorWithBaseImpl<Traits>,
				public PHX::EvaluatorDerived<EvalT, Traits>  {

public:

  ScalarL2ProjectionResidual(const Teuchos::ParameterList& p);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  // Input:
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint> wBF;
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> wGradBF;
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim, Dim> DefGrad;
  PHX::MDField<ScalarT,Cell,QuadPoint> projectedStress;


  // Input for hydro-static stress effect
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim,Dim> Pstress;







  bool haveSource;
  bool haveMechSource;
  bool enableTransient;

  unsigned int numNodes;
  unsigned int numQPs;
  unsigned int numDims;
  unsigned int worksetSize;


  Intrepid::FieldContainer<ScalarT> tauStress;
  Intrepid::FieldContainer<ScalarT> tauH;

  // Output:
  PHX::MDField<ScalarT,Cell,Node> TResidual;


};
}

#endif
