//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef QCAD_RESPONSECENTEROFMASS_HPP
#define QCAD_RESPONSECENTEROFMASS_HPP

#include "QCAD_MeshRegion.hpp"
#include "PHAL_SeparableScatterScalarResponse.hpp"

namespace QCAD {
/** 
 * \brief Response Description
 */
  template<typename EvalT, typename Traits>
  class ResponseCenterOfMass : 
    public PHAL::SeparableScatterScalarResponse<EvalT,Traits>
  {
  public:
    typedef typename EvalT::ScalarT ScalarT;
    typedef typename EvalT::MeshScalarT MeshScalarT;
    
    ResponseCenterOfMass(Teuchos::ParameterList& p,
			 const Teuchos::RCP<Albany::Layouts>& dl);
  
    void postRegistrationSetup(typename Traits::SetupData d,
				     PHX::FieldManager<Traits>& vm);

    void preEvaluate(typename Traits::PreEvalData d);
  
    void evaluateFields(typename Traits::EvalData d);

    void postEvaluate(typename Traits::PostEvalData d);
	  
  private:
    Teuchos::RCP<const Teuchos::ParameterList> getValidResponseParameters() const;

    std::size_t numQPs;
    std::size_t numDims;
    
    PHX::MDField<ScalarT> field;
    PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim> coordVec;
    PHX::MDField<MeshScalarT,Cell,QuadPoint> weights;
    
    std::string fieldName;
    Teuchos::RCP< MeshRegion<EvalT, Traits> > opRegion;
  };
	
}

#endif
