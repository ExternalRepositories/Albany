//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#include "Teuchos_TestForException.hpp"
#include "Albany_AdaptationFactory.hpp"
#include "Albany_CopyRemesh.hpp"
//#ifdef ALBANY_LCM 
#if defined(ALBANY_LCM) && defined(LCM_SPECULATIVE)
#include "Albany_TopologyModification.hpp"
#include "Albany_RandomFracture.hpp"
#endif

Albany::AdaptationFactory::AdaptationFactory(
       const Teuchos::RCP<Teuchos::ParameterList>& adaptParams_,
       const Teuchos::RCP<ParamLib>& paramLib_,
       Albany::StateManager& StateMgr_,
       const Teuchos::RCP<const Epetra_Comm>& comm_) :
  adaptParams(adaptParams_),
  paramLib(paramLib_),
  StateMgr(StateMgr_),
  comm(comm_)
{
}

Teuchos::RCP<Albany::AbstractAdapter>
Albany::AdaptationFactory::create()
{
  Teuchos::RCP<Albany::AbstractAdapter> strategy;
  using Teuchos::rcp;
  std::string& method = adaptParams->get("Method", "");

  if (method == "Copy Remesh") {
    strategy = rcp(new Albany::CopyRemesh(adaptParams, paramLib, StateMgr, comm));
  }
//#ifdef ALBANY_LCM
#if defined(ALBANY_LCM) && defined(LCM_SPECULATIVE)
  else if (method == "Topmod") {
    strategy = rcp(new Albany::TopologyMod(adaptParams, paramLib, StateMgr, comm));
  }
  else if (method == "Random") {
    strategy = rcp(new Albany::RandomFracture(adaptParams, paramLib, StateMgr, comm));
  }
#endif
  else {
    TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
                       std::endl << 
                       "Error!  Unknown adaptivity method requested: " << method << 
                       "!" << std::endl << "Supplied parameter list is " << 
                       std::endl << *adaptParams);
  }

  return strategy;
}

