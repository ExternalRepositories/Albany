//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef FELIX_RESPONSE_SURFACE_VELOCITY_MISMATCH_HPP
#define FELIX_RESPONSE_SURFACE_VELOCITY_MISMATCH_HPP

//#include "FELIX_MeshRegion.hpp"
#include "PHAL_SeparableScatterScalarResponse.hpp"

namespace FELIX {
/**
 * \brief Response Description
 */
  template<typename EvalT, typename Traits>
  class ResponseSurfaceVelocityMismatch :
    public PHAL::SeparableScatterScalarResponse<EvalT,Traits>
  {
  public:
    typedef typename EvalT::ScalarT ScalarT;
    typedef typename EvalT::MeshScalarT MeshScalarT;

    ResponseSurfaceVelocityMismatch(Teuchos::ParameterList& p,
       const Teuchos::RCP<Albany::Layouts>& dl);

    void postRegistrationSetup(typename Traits::SetupData d,
             PHX::FieldManager<Traits>& vm);

    void preEvaluate(typename Traits::PreEvalData d);

    void evaluateFields(typename Traits::EvalData d);

    void postEvaluate(typename Traits::PostEvalData d);

  private:
    Teuchos::RCP<const Teuchos::ParameterList> getValidResponseParameters() const;

    std::string surfaceSideName;
    std::string basalSideName;

    int numSideNodes;
    int numSideQPs;
    int numSideDims;

    PHX::MDField<ScalarT,Cell,Side,QuadPoint,VecDim> velocity;
    PHX::MDField<ScalarT,Cell,Side,QuadPoint,VecDim> observedVelocity;
    PHX::MDField<ScalarT,Cell,Side,QuadPoint,VecDim> observedVelocityRMS;
    PHX::MDField<ScalarT,Cell,Side,QuadPoint,Dim> grad_beta;
    PHX::MDField<RealType,Cell,Side,Node,QuadPoint> BF_basal;
    PHX::MDField<RealType,Cell,Side,QuadPoint> w_measure_basal;
    PHX::MDField<RealType,Cell,Side,QuadPoint> w_measure_surface;
    PHX::MDField<RealType,Cell,Side,QuadPoint,Dim,Dim> inv_metric_surface;

    ScalarT p_resp, p_reg, resp, reg;
    double scaling, alpha, asinh_scaling;
  };

}

#endif
