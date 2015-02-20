//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef ATO_INTERNALENERGYRESPONSE_HPP
#define ATO_INTERNALENERGYRESPONSE_HPP

#include "PHAL_SeparableScatterScalarResponse.hpp"
#include "ATO_TopoTools.hpp"



/** 
 * \brief Response Description
 */
namespace ATO 
{
  template<typename EvalT, typename Traits>
  class InternalEnergyResponse : 
    public PHAL::SeparableScatterScalarResponse<EvalT,Traits>
  {
  public:
    typedef typename EvalT::ScalarT ScalarT;
    typedef typename EvalT::MeshScalarT MeshScalarT;
    
    InternalEnergyResponse(Teuchos::ParameterList& p,
			const Teuchos::RCP<Albany::Layouts>& dl);
  
    void postRegistrationSetup(typename Traits::SetupData d,
				     PHX::FieldManager<Traits>& vm);
  
    void preEvaluate(typename Traits::PreEvalData d);
  
    void evaluateFields(typename Traits::EvalData d);

    void postEvaluate(typename Traits::PostEvalData d);

	  
  private:

    std::string dFdpName;
    std::string FName;
    static const std::string className;

    PHX::MDField<ScalarT> gradX;
    PHX::MDField<ScalarT> workConj;
    PHX::MDField<MeshScalarT,Cell,QuadPoint> qp_weights;
    PHX::MDField<RealType,Cell,Node,QuadPoint> BF;
    PHX::MDField<ScalarT,Cell,Node> topo;


    Teuchos::RCP< PHX::Tag<ScalarT> > stiffness_objective_tag;
    Albany::StateManager* pStateMgr;

    Teuchos::RCP<Topology> topology;

  };
	
}

#endif
