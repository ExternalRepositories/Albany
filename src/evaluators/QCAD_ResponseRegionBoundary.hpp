//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef QCAD_RESPONSEREGIONBOUNDARY_HPP
#define QCAD_RESPONSEREGIONBOUNDARY_HPP

#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Albany_ProblemUtils.hpp"
#include "QCAD_MaterialDatabase.hpp"
#include "QCAD_EvaluatorTools.hpp"

namespace QCAD {

/** 
 * \brief QCAD Response which computes regions within the mesh based on field values, usually to determing
 *         sub-regions of the mesh for later processing (e.g. for a quantum mechanical solution to be obtained)
 */
  template<typename EvalT, typename Traits>
  class ResponseRegionBoundary :
    public PHX::EvaluatorWithBaseImpl<Traits>,
    public PHX::EvaluatorDerived<EvalT, Traits>,
    public EvaluatorTools<EvalT, Traits>
  {
  public:
    typedef typename EvalT::ScalarT ScalarT;
    typedef typename EvalT::MeshScalarT MeshScalarT;
    
    ResponseRegionBoundary(Teuchos::ParameterList& p,
		       const Teuchos::RCP<Albany::Layouts>& dl);
  
    void postRegistrationSetup(typename Traits::SetupData d,
			       PHX::FieldManager<Traits>& vm);
  
    void evaluateFields(typename Traits::EvalData d);

    void postEvaluate(typename Traits::PostEvalData d);

    Teuchos::RCP<const PHX::FieldTag> getEvaluatedFieldTag() const {
      return response_field_tag;
    }

    Teuchos::RCP<const PHX::FieldTag> getResponseFieldTag() const {
      return response_field_tag;
    }
	  
  private:
    Teuchos::RCP<const Teuchos::ParameterList> getValidResponseParameters() const;

    std::string regionType;
    std::string outputFilename;
    double levelSetFieldMin, levelSetFieldMax;

    std::size_t numQPs;
    std::size_t numDims;

    std::string levelSetFieldname;
    PHX::MDField<ScalarT> levelSetField;    
    PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim> coordVec;
    PHX::MDField<MeshScalarT,Cell,QuadPoint> weights;
    
    std::vector<std::string> ebNames;
    bool bQuantumEBsOnly;

    //Region boundary: for now just min/max along each coordinate direction
    //  * just MeshScalarTs - no derivative information for region boundaries yet, as
    //  * it's not clear this should be done and whether it's ever desired
    std::vector<MeshScalarT> minVals, maxVals;

    //! Material database
    Teuchos::RCP<QCAD::MaterialDatabase> materialDB;

    Teuchos::RCP< PHX::Tag<ScalarT> > response_field_tag;
  };
	
}

#endif
