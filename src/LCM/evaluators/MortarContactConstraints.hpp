//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef MORTAR_CONTACT_CONSTRAINTS_HPP
#define MORTAR_CONTACT_CONSTRAINTS_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Albany_Layouts.hpp"

#include "Teuchos_ParameterList.hpp"

namespace LCM {
/** \brief This class implements the Mortar contact algorithm. Here is the overall sketch of how things work:

   General context: A workset of elements are processed to assemble local finite element residual contributions that
   the opposite contacting surface will impose on the current workset of elements.

   1. Do a global search to find all the slave segments that can potentially intersect the master segments that this
      processor owns. This is done in preEvaluate, as we don't want to loop over worksets and we want to do the global
      search once per processor.

   2. For the elements in the workset, find the element surfaces that are master surface segments. In the beginning of
      evaluate, do a local search to find the slave segments that potentially intersect each master segment. Note that this
      can change each evaluate call (Newton iteration).

   3. In evaluate, form the mortar integration space and assemble all the slave constraint contributions into the master side
      locations residual vector - ultimately the elements of the current workset.

*/
// **************************************************************
// Base Class for code that is independent of evaluation type
// **************************************************************

template<typename EvalT, typename Traits>
class MortarContact
  : public PHX::EvaluatorWithBaseImpl<Traits>,
    public PHX::EvaluatorDerived<EvalT, Traits>  {

public:

  MortarContact(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

// These functions are defined in the specializations
  void preEvaluate(typename Traits::PreEvalData d);
  void evaluateFields(typename Traits::EvalData d);

protected:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  PHX::MDField<ScalarT,Cell,QuadPoint> M_operator; // This evaluator creates M and D, not sure what they look like yet
                                                   // so put in a placeholder

  const Teuchos::Array<std::string> masterSideNames;
  const Teuchos::Array<std::string> slaveSideNames;
  const Teuchos::Array<std::string> sideSetIDs;
  const Albany::MeshSpecsStruct* meshSpecs;

//! Coordinate vector at vertices
  PHX::MDField<MeshScalarT,Cell,Vertex,Dim> coordVec;


};
}

#endif
