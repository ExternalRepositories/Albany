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


#include "Albany_AggregateScalarResponseFunction.hpp"
#include "Albany_Application.hpp"
#include "Epetra_LocalMap.h"

using Teuchos::RCP;
using Teuchos::rcp;

Albany::AggregateScalarResponseFunction::
AggregateScalarResponseFunction(
  const Teuchos::RCP<const Epetra_Comm>& comm,
  const Teuchos::Array< Teuchos::RCP<ScalarResponseFunction> >& responses_) :
  SamplingBasedScalarResponseFunction(comm),
  responses(responses_)
{
}

Albany::AggregateScalarResponseFunction::
~AggregateScalarResponseFunction()
{
}

unsigned int
Albany::AggregateScalarResponseFunction::
numResponses() const 
{
  unsigned int n = 0;
  for (int i=0; i<responses.size(); i++)
    n += responses[i]->numResponses();
  return n;
}

void
Albany::AggregateScalarResponseFunction::
evaluateResponse(const double current_time,
		 const Epetra_Vector* xdot,
		 const Epetra_Vector& x,
		 const Teuchos::Array<ParamVec>& p,
		 Epetra_Vector& g)
{
  unsigned int offset = 0;
  for (unsigned int i=0; i<responses.size(); i++) {

    // Create Epetra_Map for response function
    unsigned int num_responses = responses[i]->numResponses();
    Epetra_LocalMap local_response_map(num_responses, 0, 
				       *(responses[i]->getComm()));

    // Create Epetra_Vector for response function
    Epetra_Vector local_g(local_response_map);

    // Evaluate response function
    responses[i]->evaluateResponse(current_time, xdot, x, p, local_g);
    
    // Copy result into combined result
    for (unsigned int j=0; j<num_responses; j++)
      g[offset+j] = local_g[j];

    // Increment offset in combined result
    offset += num_responses;
  }
}

void
Albany::AggregateScalarResponseFunction::
evaluateResponseT(const double current_time,
		 const Tpetra_Vector* xdotT,
		 const Tpetra_Vector& xT,
		 const Teuchos::Array<ParamVec>& p,
		 Tpetra_Vector& gT)
{
  unsigned int offset = 0;
  for (unsigned int i=0; i<responses.size(); i++) {

    // Create Tpetra_Map for response function
    unsigned int num_responses = responses[i]->numResponses();
    const Epetra_Comm& comm = *(responses[i]->getComm());
    Teuchos::RCP<const Teuchos::Comm<int> > commT = Albany::createTeuchosCommFromMpiComm(Albany::getMpiCommFromEpetraComm(comm));
    Tpetra::LocalGlobal lg = Tpetra::LocallyReplicated;
    Teuchos::RCP<Tpetra_Map> local_response_map = Teuchos::rcp(new Tpetra_Map(num_responses, 0, commT, lg));
    
    // Create Tpetra_Vector for response function
    Teuchos::RCP<Tpetra_Vector> local_gT = Teuchos::rcp(new Tpetra_Vector(local_response_map));
  
    // Evaluate response function
    responses[i]->evaluateResponseT(current_time, xdotT, xT, p, *local_gT);
    
    //get views of g and local_g for element access
    Teuchos::ArrayRCP<const ST> local_gT_constView = local_gT->get1dView();
    Teuchos::ArrayRCP<ST> gT_nonconstView = gT.get1dViewNonConst();

    // Copy result into combined result
    for (unsigned int j=0; j<num_responses; j++)
      gT_nonconstView[offset+j] = local_gT_constView[j];

    // Increment offset in combined result
    offset += num_responses;
  }
  
}

void
Albany::AggregateScalarResponseFunction::
evaluateTangent(const double alpha, 
		const double beta,
		const double current_time,
		bool sum_derivs,
		const Epetra_Vector* xdot,
		const Epetra_Vector& x,
		const Teuchos::Array<ParamVec>& p,
		ParamVec* deriv_p,
		const Epetra_MultiVector* Vxdot,
		const Epetra_MultiVector* Vx,
		const Epetra_MultiVector* Vp,
		Epetra_Vector* g,
		Epetra_MultiVector* gx,
		Epetra_MultiVector* gp)
{
  unsigned int offset = 0;
  for (unsigned int i=0; i<responses.size(); i++) {

    // Create Epetra_Map for response function
    unsigned int num_responses = responses[i]->numResponses();
    Epetra_LocalMap local_response_map(num_responses, 0, 
      *(responses[i]->getComm()));

    // Create Epetra_Vectors for response function
    RCP<Epetra_Vector> local_g;
    RCP<Epetra_MultiVector> local_gx, local_gp;
    if (g != NULL)
      local_g = rcp(new Epetra_Vector(local_response_map));
    if (gx != NULL)
      local_gx = rcp(new Epetra_MultiVector(local_response_map, 
					    gx->NumVectors()));
    if (gp != NULL)
      local_gp = rcp(new Epetra_MultiVector(local_response_map, 
					    gp->NumVectors()));

    // Evaluate response function
    responses[i]->evaluateTangent(alpha, beta, current_time, sum_derivs,
				  xdot, x, p, deriv_p, Vxdot, Vx, Vp, 
				  local_g.get(), local_gx.get(), 
				  local_gp.get());

    // Copy results into combined result
    for (unsigned int j=0; j<num_responses; j++) {
      if (g != NULL)
        (*g)[offset+j] = (*local_g)[j];
      if (gx != NULL)
	for (int k=0; k<gx->NumVectors(); k++)
	  (*gx)[k][offset+j] = (*local_gx)[k][j];
      if (gp != NULL)
	for (int k=0; k<gp->NumVectors(); k++)
	  (*gp)[k][offset+j] = (*local_gp)[k][j];
    }

    // Increment offset in combined result
    offset += num_responses;
  }
}

void
Albany::AggregateScalarResponseFunction::
evaluateTangentT(const double alpha, 
		const double beta,
		const double current_time,
		bool sum_derivs,
		const Tpetra_Vector* xdotT,
		const Tpetra_Vector& xT,
		const Teuchos::Array<ParamVec>& p,
		ParamVec* deriv_p,
		const Tpetra_MultiVector* VxdotT,
		const Tpetra_MultiVector* VxT,
		const Tpetra_MultiVector* VpT,
		Tpetra_Vector* gT,
		Tpetra_MultiVector* gxT,
		Tpetra_MultiVector* gpT)
{
  unsigned int offset = 0;
  for (unsigned int i=0; i<responses.size(); i++) {

    // Create Tpetra_Map for response function
    unsigned int num_responses = responses[i]->numResponses();
    const Epetra_Comm& comm = *(responses[i]->getComm());
    Teuchos::RCP<const Teuchos::Comm<int> > commT = Albany::createTeuchosCommFromMpiComm(Albany::getMpiCommFromEpetraComm(comm));
    Tpetra::LocalGlobal lg = Tpetra::LocallyReplicated;
    Teuchos::RCP<Tpetra_Map> local_response_map = Teuchos::rcp(new Tpetra_Map(num_responses, 0, commT, lg));

    // Create Tpetra_Vectors for response function
    RCP<Tpetra_Vector> local_gT;
    RCP<Tpetra_MultiVector> local_gxT, local_gpT;
    if (gT != NULL)
      local_gT = rcp(new Tpetra_Vector(local_response_map));
    if (gxT != NULL)
      local_gxT = rcp(new Tpetra_MultiVector(local_response_map, 
					    gxT->getNumVectors()));
    if (gpT != NULL)
      local_gpT = rcp(new Tpetra_MultiVector(local_response_map, 
					    gpT->getNumVectors()));

    // Evaluate response function
    responses[i]->evaluateTangentT(alpha, beta, current_time, sum_derivs,
				  xdotT, xT, p, deriv_p, VxdotT, VxT, VpT, 
				  local_gT.get(), local_gxT.get(), 
				  local_gpT.get());

    Teuchos::ArrayRCP<const ST> local_gT_constView;
    Teuchos::ArrayRCP<ST> gT_nonconstView;
    if (gT != NULL) { 
      local_gT_constView = local_gT->get1dView();
      gT_nonconstView = gT->get1dViewNonConst();
    }

    // Copy results into combined result
    for (unsigned int j=0; j<num_responses; j++) {
      if (gT != NULL)
        gT_nonconstView[offset+j] = local_gT_constView[j];
      if (gxT != NULL) {
        Teuchos::ArrayRCP<ST> gxT_nonconstView;
        Teuchos::ArrayRCP<const ST> local_gxT_constView;
	for (int k=0; k<gxT->getNumVectors(); k++) {
          gxT_nonconstView = gxT->getDataNonConst(k); 
          local_gxT_constView = local_gxT->getData(k); 
	  gxT_nonconstView[offset+j] = local_gxT_constView[j];
         }
      }
      if (gpT != NULL) {
        Teuchos::ArrayRCP<ST> gpT_nonconstView;
        Teuchos::ArrayRCP<const ST> local_gpT_constView;
	for (int k=0; k<gpT->getNumVectors(); k++) {
          gpT_nonconstView = gpT->getDataNonConst(k); 
          local_gpT_constView = local_gpT->getData(k); 
	  gpT_nonconstView[offset+j] = local_gpT_constView[j];
        }
      }
    }

    // Increment offset in combined result
    offset += num_responses; 
  }
}


void
Albany::AggregateScalarResponseFunction::
evaluateGradient(const double current_time,
		 const Epetra_Vector* xdot,
		 const Epetra_Vector& x,
		 const Teuchos::Array<ParamVec>& p,
		 ParamVec* deriv_p,
		 Epetra_Vector* g,
		 Epetra_MultiVector* dg_dx,
		 Epetra_MultiVector* dg_dxdot,
		 Epetra_MultiVector* dg_dp)
{
  unsigned int offset = 0;
  for (unsigned int i=0; i<responses.size(); i++) {

    // Create Epetra_Map for response function
    unsigned int num_responses = responses[i]->numResponses();
    Epetra_LocalMap local_response_map(num_responses, 0, 
				       *(responses[i]->getComm()));

    // Create Epetra_Vectors for response function
    RCP<Epetra_Vector> local_g;
    if (g != NULL)
      local_g = rcp(new Epetra_Vector(local_response_map));
    RCP<Epetra_MultiVector> local_dgdx;
    if (dg_dx != NULL)
      local_dgdx = rcp(new Epetra_MultiVector(dg_dx->Map(), num_responses));
    RCP<Epetra_MultiVector> local_dgdxdot;
    if (dg_dxdot != NULL)
      local_dgdxdot = rcp(new Epetra_MultiVector(dg_dxdot->Map(), 
						 num_responses));
    RCP<Epetra_MultiVector> local_dgdp;
    if (dg_dp != NULL)
      local_dgdp = rcp(new Epetra_MultiVector(local_response_map, 
					      dg_dp->NumVectors()));

    // Evaluate response function
    responses[i]->evaluateGradient(current_time, xdot, x, p, deriv_p, 
				   local_g.get(), local_dgdx.get(), 
				   local_dgdxdot.get(), local_dgdp.get());

    // Copy results into combined result
    for (unsigned int j=0; j<num_responses; j++) {
      if (g != NULL)
        (*g)[offset+j] = (*local_g)[j];
      if (dg_dx != NULL)
        (*dg_dx)(offset+j)->Update(1.0, *((*local_dgdx)(j)), 0.0);
      if (dg_dxdot != NULL)
        (*dg_dxdot)(offset+j)->Update(1.0, *((*local_dgdxdot)(j)), 0.0);
      if (dg_dp != NULL)
	for (int k=0; k<dg_dp->NumVectors(); k++)
	  (*dg_dp)[k][offset+j] = (*local_dgdp)[k][j];
    }

    // Increment offset in combined result
    offset += num_responses;
  }
}

void
Albany::AggregateScalarResponseFunction::
evaluateGradientT(const double current_time,
		 const Tpetra_Vector* xdotT,
		 const Tpetra_Vector& xT,
		 const Teuchos::Array<ParamVec>& p,
		 ParamVec* deriv_p,
		 Tpetra_Vector* gT,
		 Tpetra_MultiVector* dg_dxT,
		 Tpetra_MultiVector* dg_dxdotT,
		 Tpetra_MultiVector* dg_dpT)
{
  unsigned int offset = 0;
  for (unsigned int i=0; i<responses.size(); i++) {

    // Create Tpetra_Map for response function
    unsigned int num_responses = responses[i]->numResponses();
    const Epetra_Comm& comm = *(responses[i]->getComm());
    Teuchos::RCP<const Teuchos::Comm<int> > commT = Albany::createTeuchosCommFromMpiComm(Albany::getMpiCommFromEpetraComm(comm));
    Tpetra::LocalGlobal lg = Tpetra::LocallyReplicated;
    Teuchos::RCP<Tpetra_Map> local_response_map = Teuchos::rcp(new Tpetra_Map(num_responses, 0, commT, lg));

    // Create Epetra_Vectors for response function
    RCP<Tpetra_Vector> local_gT;
    if (gT != NULL)
      local_gT = rcp(new Tpetra_Vector(local_response_map));
    RCP<Tpetra_MultiVector> local_dgdxT;
    if (dg_dxT != NULL)
      local_dgdxT = rcp(new Tpetra_MultiVector(dg_dxT->getMap(), num_responses));
    RCP<Tpetra_MultiVector> local_dgdxdotT;
    if (dg_dxdotT != NULL)
      local_dgdxdotT = rcp(new Tpetra_MultiVector(dg_dxdotT->getMap(), 
						 num_responses));
    RCP<Tpetra_MultiVector> local_dgdpT;
    if (dg_dpT != NULL)
      local_dgdpT = rcp(new Tpetra_MultiVector(local_response_map, 
					      dg_dpT->getNumVectors()));

    // Evaluate response function
    responses[i]->evaluateGradientT(current_time, xdotT, xT, p, deriv_p, 
				   local_gT.get(), local_dgdxT.get(), 
				   local_dgdxdotT.get(), local_dgdpT.get());

    //get views of gT and local_gT
    Teuchos::ArrayRCP<const ST> local_gT_constView = local_gT->get1dView();
    Teuchos::ArrayRCP<ST> gT_nonconstView = gT->get1dViewNonConst();


    // Copy results into combined result
    for (unsigned int j=0; j<num_responses; j++) {
      if (gT != NULL)
        gT_nonconstView[offset+j] = local_gT_constView[j];
      if (dg_dxT != NULL) {
        Teuchos::RCP<Tpetra_Vector> dg_dxT_vec = dg_dxT->getVectorNonConst(offset+j); 
        Teuchos::RCP<const Tpetra_Vector> local_dgdxT_vec = local_dgdxT->getVector(j);
        dg_dxT_vec->update(1.0, *local_dgdxT_vec, 0.0);  
      }
      if (dg_dxdotT != NULL) {
        Teuchos::RCP<Tpetra_Vector> dg_dxdotT_vec = dg_dxdotT->getVectorNonConst(offset+j); 
        Teuchos::RCP<const Tpetra_Vector> local_dgdxdotT_vec = local_dgdxdotT->getVector(j);
        dg_dxdotT_vec->update(1.0, *local_dgdxdotT_vec, 0.0);  
        }
      if (dg_dpT != NULL) {
        Teuchos::ArrayRCP<ST> dg_dpT_nonconstView;
        Teuchos::ArrayRCP<const ST> local_dgdpT_constView;
	for (int k=0; k<dg_dpT->getNumVectors(); k++) {
          local_dgdpT_constView = local_dgdpT->getData(k); 
          dg_dpT_nonconstView = dg_dpT->getDataNonConst(k); 
	  dg_dpT_nonconstView[offset+j] = local_dgdpT_constView[j];
        }
      }
    }

    // Increment offset in combined result
    offset += num_responses;
  }
}
