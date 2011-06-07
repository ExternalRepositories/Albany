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


#ifndef DISLOCATIONDENSITY_HPP
#define DISLOCATIONDENSITY_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Intrepid_CellTools.hpp"
#include "Intrepid_Cubature.hpp"

/** \brief Dislocation Density Tensor

    This evaluator calculates the dislcation density tensor

*/
namespace LCM {

template<typename EvalT, typename Traits>
class DislocationDensity : public PHX::EvaluatorWithBaseImpl<Traits>,
			   public PHX::EvaluatorDerived<EvalT, Traits>  {

public:

  DislocationDensity(const Teuchos::ParameterList& p);

  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;
  int  numVertices, numDims, numNodes, numQPs;
  bool square;

  // Input:
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim,Dim> Fp;
  PHX::MDField<ScalarT,Cell,Node,QuadPoint> BF;
  PHX::MDField<ScalarT,Cell,Node,QuadPoint,Dim> GradBF;

  // Temporary FieldContainers
  Intrepid::FieldContainer<ScalarT> BF_operator;
  Intrepid::FieldContainer<ScalarT> BF_inverse;
  Intrepid::FieldContainer<ScalarT> nodalFp;
  Intrepid::FieldContainer<ScalarT> curlFp;
  Intrepid::FieldContainer<ScalarT> A;
  Intrepid::FieldContainer<ScalarT> Ainv;
  
  // Output:
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim,Dim> G;
};
}

#endif
