//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "AAdapt_AdaptiveSolutionManagerT.hpp"
#if defined(HAVE_STK)
#include "AAdapt_CopyRemeshT.hpp"
#if defined(ALBANY_LCM) && defined(ALBANY_BGL)
#include "AAdapt_TopologyModificationT.hpp"
#endif
#if defined(ALBANY_LCM) && defined(LCM_SPECULATIVE)
//#include "AAdapt_RandomFracture.hpp"
#endif
#if defined(ALBANY_LCM) && defined(ALBANY_STK_PERCEPT)
#include "AAdapt_STKAdaptT.hpp"
#endif
#endif
#ifdef ALBANY_SCOREC
#include "AAdapt_MeshAdapt.hpp"
#endif
#ifdef ALBANY_AMP
#include "AAdapt_SimAdapt.hpp"
#endif
#if (defined(ALBANY_SCOREC) || defined(ALBANY_AMP))
#include "Albany_APFDiscretization.hpp"
#endif
#include "AAdapt_RC_Manager.hpp"

#include "Thyra_ModelEvaluatorDelegatorBase.hpp"

#include "Albany_ModelEvaluatorT.hpp"

AAdapt::AdaptiveSolutionManagerT::AdaptiveSolutionManagerT(
    const Teuchos::RCP<Teuchos::ParameterList>& appParams,
    const Teuchos::RCP<const Tpetra_Vector>& initial_guessT,
    const Teuchos::RCP<ParamLib>& param_lib,
    const Albany::StateManager& stateMgr,
    const Teuchos::RCP<rc::Manager>& rc_mgr,
    const Teuchos::RCP<const Teuchos_Comm>& commT) :

    out(Teuchos::VerboseObjectBase::getDefaultOStream()),
    appParams_(appParams),
    disc_(stateMgr.getDiscretization()),
    paramLib_(param_lib),
    stateMgr_(stateMgr),
    num_sol_vec(1),
    commT_(commT)
{

  // Create problem PL
  Teuchos::RCP<Teuchos::ParameterList> problemParams =
      Teuchos::sublist(appParams_, "Problem", true);

  Teuchos::ParameterList& discParams = appParams_->sublist("Discretization");

  num_sol_vec = discParams.get<int>("Number Of Solution Vectors");

  // Note that piroParams_ is a member of Thyra_AdaptiveSolutionManager
  piroParams_ = Teuchos::sublist(appParams_, "Piro", true);

  if (problemParams->isSublist("Adaptation")) { // If the user has specified adaptation on input, grab the sublist
    // Note that piroParams_ and adaptiveMesh_ are members of Thyra_AdaptiveSolutionManager
    adaptParams_ = Teuchos::sublist(problemParams, "Adaptation", true);
    adaptiveMesh_ = true;
    buildAdapter(rc_mgr);
  }

  // Want the initial time in the parameter library to be correct
  // if this is a restart solution
  if (disc_->hasRestartSolution()) {
    if (paramLib_->isParameter("Time")) {
      double initialValue = 0.0;
        if(appParams->get<std::string>("Solution Method", "Steady") == "Continuation")
          initialValue =
            appParams->sublist("Piro").sublist("LOCA").sublist("Stepper").
            get<double>("Initial Value", 0.0);
        else if(appParams->get<std::string>("Solution Method", "Steady") == "Transient")
          initialValue =
            appParams->sublist("Piro").sublist("Trapezoid Rule").get<double>("Initial Time", 0.0);
      paramLib_->setRealValue<PHAL::AlbanyTraits::Residual>("Time", initialValue);
    }
  }

  const Teuchos::RCP<const Tpetra_Map> mapT = disc_->getMapT();
  const Teuchos::RCP<const Tpetra_Map> overlapMapT = disc_->getOverlapMapT();
#ifdef ALBANY_AERAS
  //IKT, 1/20/15: the following is needed to ensure Laplace matrix is non-diagonal 
  //for Aeras problems that have hyperviscosity and are integrated using an explicit time 
  //integration scheme. 
  const Teuchos::RCP<const Tpetra_CrsGraph> overlapJacGraphT = disc_
      ->getImplicitOverlapJacobianGraphT();
#else
  const Teuchos::RCP<const Tpetra_CrsGraph> overlapJacGraphT = disc_
      ->getOverlapJacobianGraphT();
#endif

  resizeMeshDataArrays(mapT, overlapMapT, overlapJacGraphT);

  {
    Teuchos::ArrayRCP<
        Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > > > wsElNodeEqID =
        disc_->getWsElNodeEqID();
    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > > coords =
        disc_->getCoords();
    Teuchos::ArrayRCP<std::string> wsEBNames = disc_->getWsEBNames();
    const int numDim = disc_->getNumDim();
    const int neq = disc_->getNumEq();

    Teuchos::RCP<Teuchos::ParameterList> problemParams = Teuchos::sublist(
        appParams_,
        "Problem",
        true);
    if (Teuchos::nonnull(initial_guessT)) {
      initial_xT = Teuchos::rcp(new Tpetra_Vector(*initial_guessT));
// GAH need to do more here.
    } else {
      overlapped_xT->doImport(*initial_xT, *importerT, Tpetra::INSERT);

      AAdapt::InitialConditionsT(
          overlapped_xT, wsElNodeEqID, wsEBNames, coords, neq, numDim,
          problemParams->sublist("Initial Condition"),
          disc_->hasRestartSolution());

      initial_xT->doExport(*overlapped_xT, *exporterT, Tpetra::INSERT);

      if(num_sol_vec > 1){
        overlapped_xdotT->doImport(*initial_xdotT, *importerT, Tpetra::INSERT);
        AAdapt::InitialConditionsT(
          overlapped_xdotT, wsElNodeEqID, wsEBNames, coords, neq, numDim,
          problemParams->sublist("Initial Condition Dot"));
        initial_xdotT->doExport(*overlapped_xdotT, *exporterT, Tpetra::INSERT);
      }
      if(num_sol_vec > 2){
        overlapped_xdotdotT->doImport(*initial_xdotdotT, *importerT, Tpetra::INSERT);
        AAdapt::InitialConditionsT(
          overlapped_xdotdotT, wsElNodeEqID, wsEBNames, coords, neq, numDim,
          problemParams->sublist("Initial Condition DotDot"));
        initial_xdotdotT->doExport(*overlapped_xdotdotT, *exporterT, Tpetra::INSERT);
      }
    }
  }
#if (defined(ALBANY_SCOREC) || defined(ALBANY_AMP))
  {
    const Teuchos::RCP< Albany::APFDiscretization > apf_disc =
      Teuchos::rcp_dynamic_cast< Albany::APFDiscretization >(disc_);
    if ( ! apf_disc.is_null()) {
      apf_disc->writeSolutionToMeshDatabaseT(*overlapped_xT, 0, true);
      apf_disc->initTemperatureHack();
    }
  }
#endif
}

void AAdapt::AdaptiveSolutionManagerT::
buildAdapter(const Teuchos::RCP<rc::Manager>& rc_mgr)
{

  std::string& method = adaptParams_->get("Method", "");
  std::string first_three_chars = method.substr(0, 3);

#if defined(HAVE_STK)
  if (method == "Copy Remesh") {
    adapter_ = Teuchos::rcp(new AAdapt::CopyRemeshT(adaptParams_,
        paramLib_,
        stateMgr_,
        commT_));
  } else

# if defined(ALBANY_LCM) && defined(ALBANY_BGL)
  if (method == "Topmod") {
    adapter_ = Teuchos::rcp(new AAdapt::TopologyModT(adaptParams_,
        paramLib_,
        stateMgr_,
        commT_));
  } else
# endif
#endif

#if 0
# if defined(ALBANY_LCM) && defined(LCM_SPECULATIVE)
  if (method == "Random") {
    strategy = rcp(new AAdapt::RandomFracture(adaptParams_,
            param_lib_,
            state_mgr_,
            epetra_comm_));
  } else
# endif
#endif
#ifdef ALBANY_SCOREC
  // RCP needs to be non-owned because otherwise there is an RCP circle.
  if (first_three_chars == "RPI") {
    adapter_ = Teuchos::rcp(
      new AAdapt::MeshAdapt(adaptParams_, paramLib_, stateMgr_, rc_mgr,
                             commT_));
  } else
#endif
#ifdef ALBANY_AMP
  if (method == "Sim") {
    adapter_ = Teuchos::rcp(
      new AAdapt::SimAdapt(adaptParams_, paramLib_, stateMgr_, commT_));
  } else
#endif
#if defined(ALBANY_LCM) && defined(ALBANY_STK_PERCEPT)
  if (method == "Unif Size") {
    adapter_ = Teuchos::rcp(new AAdapt::STKAdaptT<AAdapt::STKUnifRefineField>(adaptParams_,
            paramLib_,
            stateMgr_,
            commT_));
  } else
#endif

  {
    TEUCHOS_TEST_FOR_EXCEPTION(true,
        Teuchos::Exceptions::InvalidParameter,
        std::endl <<
        "Error! Unknown adaptivity method requested:"
        << method <<
        " !" << std::endl
        << "Supplied parameter list is " <<
        std::endl << *adaptParams_);
  }

  *out << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
      << " Mesh adapter has been initialized:\n"
      << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
      << std::endl;

}

bool
AAdapt::AdaptiveSolutionManagerT::
adaptProblem()
{

  Teuchos::RCP<Thyra::ModelEvaluator<double> > model = this->getState()->getModel();

  // resize problem if the mesh adapts
  if (adapter_->adaptMesh()) {

    resizeMeshDataArrays(disc_->getMapT(),
        disc_->getOverlapMapT(), disc_->getOverlapJacobianGraphT());

    Teuchos::RCP<Thyra::ModelEvaluatorDelegatorBase<ST> > base =
        Teuchos::rcp_dynamic_cast<Thyra::ModelEvaluatorDelegatorBase<ST> >(
            model);

    // If dynamic cast fails
    TEUCHOS_TEST_FOR_EXCEPTION(
        base == Teuchos::null,
        std::logic_error,
        std::endl <<
        "Error! : Cast to Thyra::ModelEvaluatorDelegatorBase failed!" << std::endl);

    Teuchos::RCP<Albany::ModelEvaluatorT> me =
        Teuchos::rcp_dynamic_cast<Albany::ModelEvaluatorT>(
            base->getNonconstUnderlyingModel());

    // If dynamic cast fails
    TEUCHOS_TEST_FOR_EXCEPTION(me == Teuchos::null,
        std::logic_error,
        std::endl <<
        "Error! : Cast to Albany::ModelEvaluatorT failed!" << std::endl);

    // Allocate storage in the model evaluator
    me->allocateVectors();

    // Build the solution group down in Thyra_AdaptiveSolutionManager.cpp
    this->getState()->buildSolutionGroup();

    // getSolutionField() below returns the new solution vector with the fields transferred to it
    current_soln = disc_->getSolutionMV();
// need to only access the ones we have here
    initial_xT = current_soln->getVectorNonConst(0);
    initial_xdotT = current_soln->getVectorNonConst(1);
    initial_xdotdotT = current_soln->getVectorNonConst(2);

    *out << "Mesh adaptation was successfully performed!" << std::endl;

    return true;

  }

  *out << "Mesh adaptation was NOT successfully performed!" << std::endl;

  *out
      << "Mesh adaptation machinery has returned a FAILURE error code, exiting Albany!"
      << std::endl;

  TEUCHOS_TEST_FOR_EXCEPTION(
      true,
      std::logic_error,
      "Mesh adaptation failed!\n");

  return false;

}

void AAdapt::AdaptiveSolutionManagerT::resizeMeshDataArrays(
    const Teuchos::RCP<const Tpetra_Map> &mapT,
    const Teuchos::RCP<const Tpetra_Map> &overlapMapT,
    const Teuchos::RCP<const Tpetra_CrsGraph> &overlapJacGraphT)
{

  importerT = Teuchos::rcp(new Tpetra_Import(mapT, overlapMapT));
  exporterT = Teuchos::rcp(new Tpetra_Export(overlapMapT, mapT));

  overlapped_xT = Teuchos::rcp(new Tpetra_Vector(overlapMapT));
  overlapped_xdotT = Teuchos::rcp(new Tpetra_Vector(overlapMapT));
  overlapped_xdotdotT = Teuchos::rcp(new Tpetra_Vector(overlapMapT));
  overlapped_fT = Teuchos::rcp(new Tpetra_Vector(overlapMapT));
  overlapped_jacT = Teuchos::rcp(new Tpetra_CrsMatrix(overlapJacGraphT));

  tmp_ovlp_solMV = Teuchos::rcp(new Tpetra_MultiVector(overlapMapT, num_sol_vec, false));
  tmp_ovlp_solT = tmp_ovlp_solMV->getVectorNonConst(0);

  current_soln = disc_->getSolutionMV();
// need to only access the ones we have here
  initial_xT = current_soln->getVectorNonConst(0);
  if(current_soln->getNumVectors() > 1)
    initial_xdotT = current_soln->getVectorNonConst(1);
  else
    initial_xdotT = Teuchos::null;
  if(current_soln->getNumVectors() > 2)
    initial_xdotdotT = current_soln->getVectorNonConst(2);
  else
    initial_xdotdotT = Teuchos::null;

}

Teuchos::RCP<Tpetra_Vector>
AAdapt::AdaptiveSolutionManagerT::getOverlapSolutionT(
    const Tpetra_Vector& solutionT)
{
  tmp_ovlp_solT->doImport(solutionT, *importerT, Tpetra::INSERT);
  return tmp_ovlp_solT;
}

Teuchos::RCP<Tpetra_MultiVector>
AAdapt::AdaptiveSolutionManagerT::getOverlapSolutionMV(
    const Tpetra_MultiVector& solutionT)
{
  tmp_ovlp_solMV->doImport(solutionT, *importerT, Tpetra::INSERT);
  return tmp_ovlp_solMV;
}

void
AAdapt::AdaptiveSolutionManagerT::scatterXT(
    const Tpetra_Vector& xT,
    const Tpetra_Vector* x_dotT,
    const Tpetra_Vector* x_dotdotT)
{

  overlapped_xT->doImport(xT, *importerT, Tpetra::INSERT);

  if (x_dotT) overlapped_xdotT->doImport(*x_dotT, *importerT, Tpetra::INSERT);
  if (x_dotdotT)
    overlapped_xdotdotT->doImport(*x_dotdotT, *importerT, Tpetra::INSERT);

}

Teuchos::RCP<Thyra::MultiVectorBase<double> >
AAdapt::AdaptiveSolutionManagerT::
getCurrentSolution()
{
   return Thyra::createMultiVector<ST, LO, GO, KokkosNode>(current_soln);
}

void
AAdapt::AdaptiveSolutionManagerT::
projectCurrentSolution()
{

  // grp->getNOXThyraVecRCPX() is the current solution on the old mesh

  // TO provide an example, assume that the meshes are identical and we can just copy the data between them (a Copy Remesh)

  /*
   const Teuchos::RCP<const Tpetra_Vector> testSolution =
   ConverterT::getConstTpetraVector(grp_->getNOXThyraVecRCPX()->getThyraRCPVector());
   */

//    *initial_xT = *testSolution;
}
