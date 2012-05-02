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


#ifndef ALBANY_APPLICATION_HPP
#define ALBANY_APPLICATION_HPP

#include <vector>

#include "Teuchos_RCP.hpp"
#include "Teuchos_ArrayRCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_SerialDenseMatrix.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_TimeMonitor.hpp"

#include "Epetra_Map.h"
#include "Epetra_Vector.h"
#include "Epetra_CrsMatrix.h"
#include "Epetra_Import.h"
#include "Epetra_Export.h"

#include "Albany_AbstractDiscretization.hpp"
#include "Albany_AbstractProblem.hpp"
#include "Albany_AbstractResponseFunction.hpp"
#include "Albany_StateManager.hpp"
#include "Albany_Adaptation.hpp"

#ifdef ALBANY_CUTR
  #include "CUTR_CubitMeshMover.hpp"
  #include "STKMeshData.hpp"
#endif

#include "Sacado_ScalarParameterLibrary.hpp"
#include "Sacado_ScalarParameterVector.hpp"
#include "Sacado_ParameterAccessor.hpp"
#include "Sacado_ParameterRegistration.hpp"

#include "PHAL_AlbanyTraits.hpp"
#include "Phalanx.hpp"

#include "Stokhos_OrthogPolyExpansion.hpp"
#include "Stokhos_Quadrature.hpp"
#include "Stokhos_EpetraVectorOrthogPoly.hpp"
#include "Stokhos_EpetraMultiVectorOrthogPoly.hpp"
#include "EpetraExt_MultiComm.h"

#include "Teko_InverseLibrary.hpp"

namespace Albany {

  class Application :
     public Sacado::ParameterAccessor<PHAL::AlbanyTraits::Residual, SPL_Traits> {
  public:

     enum SolutionMethod {Steady, Transient, Continuation, MultiProblem};

    //! Constructor 
    Application(const Teuchos::RCP<const Epetra_Comm>& comm,
		const Teuchos::RCP<Teuchos::ParameterList>& params,
		const Teuchos::RCP<const Epetra_Vector>& initial_guess = 
		Teuchos::null);

    //! Destructor
    ~Application();

    void getRBMInfo(int& numPDEs, int& numElasticityDim, int& numScalar, int& nullSpaceDim);

    //! Get underlying abstract discretization
    Teuchos::RCP<Albany::AbstractDiscretization> getDiscretization() const;

    //! Get problem object
    Teuchos::RCP<Albany::AbstractProblem> getProblem() const;

    //! Get communicator
    Teuchos::RCP<const Epetra_Comm> getComm() const;

    //! Get DOF map
    Teuchos::RCP<const Epetra_Map> getMap() const;

    //! Get Jacobian graph
    Teuchos::RCP<const Epetra_CrsGraph> getJacobianGraph() const;

    //! Get Preconditioner Operator
    Teuchos::RCP<Epetra_Operator> getPreconditioner();

    //! Get initial solution
    Teuchos::RCP<const Epetra_Vector> getInitialSolution() const;

    //! Get initial solution
    Teuchos::RCP<const Epetra_Vector> getInitialSolutionDot() const;

    //! Get parameter library
    Teuchos::RCP<ParamLib> getParamLib();

   //! Get number of responses
   SolutionMethod getSolutionMethod() const {return solMethod; }

    //! Get number of responses
    int getNumResponses() const;

    //! Get response function
    Teuchos::RCP<AbstractResponseFunction> getResponse(int i) const;

    //! Return whether problem wants to use its own preconditioner
    bool suppliesPreconditioner() const;

    //! Get stochastic expansion
    Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,double> >
    getStochasticExpansion();

    //! Intialize stochastic Galerkin method
    void init_sg(
      const Teuchos::RCP<const Stokhos::OrthogPolyBasis<int,double> >& basis,
      const Teuchos::RCP<const Stokhos::Quadrature<int,double> >& quad,
      const Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,double> >& expansion,
      const Teuchos::RCP<const EpetraExt::MultiComm>& multiComm);

    //! Get number of worksets
    int getNumWorksets() const { return numWorksets; }

    //! Compute global residual
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalResidual(const double current_time,
			       const Epetra_Vector* xdot,
			       const Epetra_Vector& x,
			       const Teuchos::Array<ParamVec>& p,
			       Epetra_Vector& f);

    //! Compute global Jacobian
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalJacobian(const double alpha, 
			       const double beta,  
			       const double current_time,
			       const Epetra_Vector* xdot,
			       const Epetra_Vector& x,
			       const Teuchos::Array<ParamVec>& p,
			       Epetra_Vector* f,
			       Epetra_CrsMatrix& jac);

    //! Compute global Preconditioner
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalPreconditioner(const Teuchos::RCP<Epetra_CrsMatrix>& jac,
				     const Teuchos::RCP<Epetra_Operator>& prec);

    //! Compute global Tangent
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalTangent(const double alpha, 
			      const double beta,
			      const double current_time,
			      bool sum_derivs,
			      const Epetra_Vector* xdot,
			      const Epetra_Vector& x,
			      const Teuchos::Array<ParamVec>& p,
			      ParamVec* deriv_p,
			      const Epetra_MultiVector* Vx,
			      const Epetra_MultiVector* Vxdot,
			      const Epetra_MultiVector* Vp,
			      Epetra_Vector* f,
			      Epetra_MultiVector* JV,
			      Epetra_MultiVector* fp);

    //! Evaluate response functions
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void evaluateResponse(
      int response_index,
      const double current_time,
      const Epetra_Vector* xdot,
      const Epetra_Vector& x,
      const Teuchos::Array<ParamVec>& p,
      Epetra_Vector& g);
    
    //! Evaluate tangent = alpha*dg/dx*Vx + beta*dg/dxdot*Vxdot + dg/dp*Vp
    /*!
     * Set xdot, dxdot_dp to NULL for steady-state problems
     */
    void evaluateResponseTangent(
      int response_index,
      const double alpha, 
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
      Epetra_MultiVector* gp);

    //! Evaluate gradient = dg/dx, dg/dxdot, dg/dp
    /*!
     * Set xdot, dg_dxdot to NULL for steady-state problems
     */
    void evaluateResponseDerivative(
      int response_index,
      const double current_time,
      const Epetra_Vector* xdot,
      const Epetra_Vector& x,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      Epetra_Vector* g,
      const EpetraExt::ModelEvaluator::Derivative& dg_dx,
      const EpetraExt::ModelEvaluator::Derivative& dg_dxdot,
      const EpetraExt::ModelEvaluator::Derivative& dg_dp);

    //! Compute global residual for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalSGResidual(
      const double current_time,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType> >& sg_p_vals,
      Stokhos::EpetraVectorOrthogPoly& sg_f);

    //! Compute global Jacobian for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalSGJacobian(
      const double alpha, 
      const double beta,
      const double current_time,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType> >& sg_p_vals,
      Stokhos::EpetraVectorOrthogPoly* sg_f,
      Stokhos::VectorOrthogPoly<Epetra_CrsMatrix>& sg_jac);

    //! Compute global Tangent for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalSGTangent(
      const double alpha, 
      const double beta, 
      const double current_time,
      bool sum_derivs,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType> >& sg_p_vals,
      ParamVec* deriv_p,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vp,
      Stokhos::EpetraVectorOrthogPoly* sg_f,
      Stokhos::EpetraMultiVectorOrthogPoly* sg_JVx,
      Stokhos::EpetraMultiVectorOrthogPoly* sg_fVp);

    //! Evaluate stochastic Galerkin response functions
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void evaluateSGResponse(
      int response_index,
      const double curr_time,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType> >& sg_p_vals,
      Stokhos::EpetraVectorOrthogPoly& sg_g);

    //! Evaluate tangent = dg/dx*dx/dp + dg/dxdot*dxdot/dp + dg/dp
    /*!
     * Set xdot, dxdot_dp to NULL for steady-state problems
     */
    void 
    evaluateSGResponseTangent(
      int response_index,
      const double alpha, 
      const double beta, 
      const double current_time,
      bool sum_derivs,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType> >& sg_p_vals,
      ParamVec* deriv_p,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vp,
      Stokhos::EpetraVectorOrthogPoly* sg_g,
      Stokhos::EpetraMultiVectorOrthogPoly* sg_JV,
      Stokhos::EpetraMultiVectorOrthogPoly* sg_gp);

    //! Evaluate gradient = dg/dx, dg/dxdot, dg/dp
    /*!
     * Set xdot, dg_dxdot to NULL for steady-state problems
     */
    void 
    evaluateSGResponseDerivative(
      int response_index,
      const double current_time,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType> >& sg_p_vals,
      ParamVec* deriv_p,
      Stokhos::EpetraVectorOrthogPoly* sg_g,
      const EpetraExt::ModelEvaluator::SGDerivative& sg_dg_dx,
      const EpetraExt::ModelEvaluator::SGDerivative& sg_dg_dxdot,
      const EpetraExt::ModelEvaluator::SGDerivative& sg_dg_dp);

    //! Compute global residual for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalMPResidual(
      const double current_time,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType> >& mp_p_vals,
      Stokhos::ProductEpetraVector& mp_f);

    //! Compute global Jacobian for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalMPJacobian(
      const double alpha, 
      const double beta,
      const double current_time,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType> >& mp_p_vals,
      Stokhos::ProductEpetraVector* mp_f,
      Stokhos::ProductContainer<Epetra_CrsMatrix>& mp_jac);

    //! Compute global Tangent for multi-point problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalMPTangent(
      const double alpha, 
      const double beta, 
      const double current_time,
      bool sum_derivs,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType> >& mp_p_vals,
      ParamVec* deriv_p,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vp,
      Stokhos::ProductEpetraVector* mp_f,
      Stokhos::ProductEpetraMultiVector* mp_JVx,
      Stokhos::ProductEpetraMultiVector* mp_fVp);

    //! Evaluate stochastic Galerkin response functions
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void evaluateMPResponse(
      int response_index,
      const double curr_time,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType> >& mp_p_vals,
      Stokhos::ProductEpetraVector& mp_g);

    //! Evaluate tangent = dg/dx*dx/dp + dg/dxdot*dxdot/dp + dg/dp
    /*!
     * Set xdot, dxdot_dp to NULL for steady-state problems
     */
    void 
    evaluateMPResponseTangent(
      int response_index,
      const double alpha, 
      const double beta, 
      const double current_time,
      bool sum_derivs,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType> >& mp_p_vals,
      ParamVec* deriv_p,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vp,
      Stokhos::ProductEpetraVector* mp_g,
      Stokhos::ProductEpetraMultiVector* mp_JV,
      Stokhos::ProductEpetraMultiVector* mp_gp);

    //! Evaluate gradient = dg/dx, dg/dxdot, dg/dp
    /*!
     * Set xdot, dg_dxdot to NULL for steady-state problems
     */
    void 
    evaluateMPResponseDerivative(
      int response_index,
      const double current_time,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType> >& mp_p_vals,
      ParamVec* deriv_p,
      Stokhos::ProductEpetraVector* mp_g,
      const EpetraExt::ModelEvaluator::MPDerivative& mp_dg_dx,
      const EpetraExt::ModelEvaluator::MPDerivative& mp_dg_dxdot,
      const EpetraExt::ModelEvaluator::MPDerivative& mp_dg_dp);

    //! Provide access to shapeParameters -- no AD
    PHAL::AlbanyTraits::Residual::ScalarT& getValue(const std::string &n);

    //! Class to manage state variables (a.k.a. history)
    StateManager& getStateMgr() {return stateMgr;};

    //! Evaluate state field manager
    void evaluateStateFieldManager(const double current_time,
				   const Epetra_Vector* xdot,
				   const Epetra_Vector& x);

    //! Access to number of worksets - needed for working with StateManager
    int getNumWorksets() { return numWorksets;};
    
    bool is_adjoint;

  private:

    //! Private to prohibit copying
    Application(const Application&);

    //! Private to prohibit copying
    Application& operator=(const Application&);

    //! Call to Teko to build strided block operator
    Teuchos::RCP<Epetra_Operator> buildWrappedOperator(
                           const Teuchos::RCP<Epetra_Operator>& Jac,
                           const Teuchos::RCP<Epetra_Operator>& wrapInput,
                           bool reorder=false) const;

    //! Utility function to set up ShapeParameters through Sacado
    void registerShapeParameters();

    void defineTimers();

  public:

    //! Routine to get workset (bucket) sized info needed by all Evaluation types
    template <typename EvalT>
    void loadWorksetBucketInfo(PHAL::Workset& workset, const int& ws);

    //! Routine to load some basic workset info needed by many Evaluation types
    void loadBasicWorksetInfo(
            PHAL::Workset& workset,
            Teuchos::RCP<Epetra_Vector> overlapped_x,
            Teuchos::RCP<Epetra_Vector> overlapped_xdot,
            double current_time);
   // Tpetra analog of above function 
   void loadBasicWorksetInfoT(
            PHAL::Workset& workset,
            Teuchos::RCP<Tpetra_Vector> overlapped_xT,
            Teuchos::RCP<Tpetra_Vector> overlapped_xdotT,
            double current_time);

    void loadWorksetJacobianInfo(PHAL::Workset& workset,
                const double& alpha, const double& beta);

    //! Routine to load common nodeset info into workset
    void loadWorksetNodesetInfo(PHAL::Workset& workset);

    //! Routine to load common sideset info into workset
    void loadWorksetSidesetInfo(PHAL::Workset& workset, const int ws);

    void setupBasicWorksetInfo(
      PHAL::Workset& workset,
      double current_time,
      const Epetra_Vector* xdot, 
      const Epetra_Vector* x,
      const Teuchos::Array<ParamVec>& p);

    void setupBasicWorksetInfoT(
      PHAL::Workset& workset,
      double current_time,
      Teuchos::RCP<const Tpetra_Vector> xdot, 
      Teuchos::RCP<const Tpetra_Vector> x,
      const Teuchos::Array<ParamVec>& p);

    void setupBasicWorksetInfo(
      PHAL::Workset& workset,
      double current_time,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot, 
      const Stokhos::EpetraVectorOrthogPoly* sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType> >& sg_p_vals);

    void setupBasicWorksetInfo(
      PHAL::Workset& workset,
      double current_time,
      const Stokhos::ProductEpetraVector* mp_xdot, 
      const Stokhos::ProductEpetraVector* mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType> >& mp_p_vals);

    void setupTangentWorksetInfo(
      PHAL::Workset& workset, 
      double current_time,
      bool sum_derivs,
      const Epetra_Vector* xdot, 
      const Epetra_Vector* x,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vp);

    void setupTangentWorksetInfoT(
      PHAL::Workset& workset, 
      double current_time,
      bool sum_derivs,
      Teuchos::RCP<const Tpetra_Vector> xdotT, 
      Teuchos::RCP<const Tpetra_Vector> xT,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      Teuchos::RCP<const Tpetra_MultiVector> VxdotT,
      Teuchos::RCP<const Tpetra_MultiVector> VxT,
      Teuchos::RCP<const Tpetra_MultiVector> VpT);
    
   void setupTangentWorksetInfo(
      PHAL::Workset& workset, 
      double current_time,
      bool sum_derivs,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot, 
      const Stokhos::EpetraVectorOrthogPoly* sg_x,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType> >& sg_p_vals,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vp);

    void setupTangentWorksetInfo(
      PHAL::Workset& workset, 
      double current_time,
      bool sum_derivs,
      const Stokhos::ProductEpetraVector* mp_xdot, 
      const Stokhos::ProductEpetraVector* mp_x,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType> >& mp_p_vals,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vp);
      
    void postRegSetup(std::string eval);

  protected:

    //! Communicator
    Teuchos::RCP<const Epetra_Comm> comm;

    //! Tpetra communicator and Kokkos node
    Teuchos::RCP<const Teuchos::Comm<int> > commT;
    Teuchos::RCP<KokkosNode> nodeT;

    //! Output stream, defaults to pronting just Proc 0
    Teuchos::RCP<Teuchos::FancyOStream> out;

    //! Element discretization
    Teuchos::RCP<Albany::AbstractDiscretization> disc;

    //! Problem class
    Teuchos::RCP<Albany::AbstractProblem> problem;

    //! Initial Epetra solution vector
    Teuchos::RCP<Epetra_Vector> initial_x;
    //! Initial Tpetra solution vector
    Teuchos::RCP<Tpetra_Vector> initial_xT;

    //! Initial Epetra solution vector
    Teuchos::RCP<Epetra_Vector> initial_x_dot;
    //! Initial Tpetra solution vector
    Teuchos::RCP<Tpetra_Vector> initial_x_dotT;

    //! Epetra Importer for overlapped data
    Teuchos::RCP<Epetra_Import> importer;
    //! Tpetra Importer for overlapped data
    Teuchos::RCP<Tpetra_Import> importerT;

    //! Epetra Exporter for overlapped data
    Teuchos::RCP<Epetra_Export> exporter;
    //! Tpetra Exporter for overlapped data
    Teuchos::RCP<Tpetra_Export> exporterT;

    //! Overlapped Epetra solution vector
    Teuchos::RCP<Epetra_Vector> overlapped_x;
    //! Overlapped Tpetra solution vector
    Teuchos::RCP<Tpetra_Vector> overlapped_xT;

    //! Overlapped Epetra time derivative vector
    Teuchos::RCP<Epetra_Vector> overlapped_xdot;
    //! Overlapped Tpetra time derivative vector
    Teuchos::RCP<Tpetra_Vector> overlapped_xdotT;

    //! Overlapped Epetra residual vector
    Teuchos::RCP<Epetra_Vector> overlapped_f;
    //! Overlapped Tpetra residual vector
    Teuchos::RCP<Tpetra_Vector> overlapped_fT;

    //! Overlapped Epetra Jacobian matrix
    Teuchos::RCP<Epetra_CrsMatrix> overlapped_jac;
    //! Overlapped Tpetra Jacobian matrix
    Teuchos::RCP<Tpetra_CrsMatrix> overlapped_jacT;

    //! Parameter library
    Teuchos::RCP<ParamLib> paramLib;

    //! Response functions
    Teuchos::Array< Teuchos::RCP<Albany::AbstractResponseFunction> > responses;

    //! Phalanx Field Manager for volumetric fills
    Teuchos::ArrayRCP<Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > > fm;

    //! Phalanx Field Manager for Dirichlet Conditions
    Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > dfm;

    //! Phalanx Field Manager for Neumann Conditions
    Teuchos::ArrayRCP<Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > > nfm;

    //! Phalanx Field Manager for states
    Teuchos::Array< Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > > sfm;

    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > > > wsElNodeEqID;
    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > > coords;
    Teuchos::ArrayRCP<std::string> wsEBNames;
    Teuchos::ArrayRCP<int> wsPhysIndex;

    //! Stochastic Galerkin basis
    Teuchos::RCP<const Stokhos::OrthogPolyBasis<int,double> > sg_basis;

    //! Stochastic Galerkin quadrature
    Teuchos::RCP<const Stokhos::Quadrature<int,double> > sg_quad;

    //! Stochastic Galerkin expansion
    Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,double> > sg_expansion;

    //! Product multi-comm
    Teuchos::RCP<const EpetraExt::MultiComm> product_comm;

    //! Overlap stochastic map
    Teuchos::RCP<const Epetra_BlockMap> sg_overlap_map;

    //! SG overlapped solution vectors
    Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly >  sg_overlapped_x;

    //! SG overlapped time derivative vectors
    Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > sg_overlapped_xdot;

    //! SG overlapped residual vectors
    Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > sg_overlapped_f;

    //! Overlapped Jacobian matrixs
    Teuchos::RCP< Stokhos::VectorOrthogPoly<Epetra_CrsMatrix> > sg_overlapped_jac;

    //! MP overlapped solution vectors
    Teuchos::RCP< Stokhos::ProductEpetraVector >  mp_overlapped_x;

    //! MP overlapped time derivative vectors
    Teuchos::RCP< Stokhos::ProductEpetraVector > mp_overlapped_xdot;

    //! MP overlapped residual vectors
    Teuchos::RCP< Stokhos::ProductEpetraVector > mp_overlapped_f;

    //! Overlapped Jacobian matrixs
    Teuchos::RCP< Stokhos::ProductContainer<Epetra_CrsMatrix> > mp_overlapped_jac;

    //! Data for Physics-Based Preconditioners
    bool physicsBasedPreconditioner;
    Teuchos::RCP<Teuchos::ParameterList> tekoParams;

    //! Type of solution method
    SolutionMethod solMethod;

    //! Shape Optimization data
    bool shapeParamsHaveBeenReset;
    std::vector<RealType> shapeParams;
    std::vector<std::string> shapeParamNames;
#ifdef ALBANY_CUTR
    Teuchos::RCP<CUTR::CubitMeshMover> meshMover;
#endif

    Teuchos::RCP<Adaptation> adapter;

    unsigned int neq;

    //! Number of worksets (buckets) to be processed 
    int numWorksets;

    //! Teko stuff
    Teuchos::RCP<Teko::InverseLibrary> inverseLib;
    Teuchos::RCP<Teko::InverseFactory> inverseFac;
    Teuchos::RCP<Epetra_Operator> wrappedJac;
    std::vector<int> blockDecomp;

    std::vector<Teuchos::RCP<Teuchos::Time> > timers;

    std::set<string> setupSet;
    mutable int phxGraphVisDetail;
    mutable int stateGraphVisDetail;

    StateManager stateMgr;

    bool morphFromInit;
    bool ignore_residual_in_jacobian;

    //! To prevent a singular mass matrix associated with Dirichlet
    //  conditions, optionally add a small perturbation to the diag
    double perturbBetaForDirichlets;
  };
}

template <typename EvalT>
void Albany::Application::loadWorksetBucketInfo(PHAL::Workset& workset, 
						const int& ws)
{
  workset.numCells = wsElNodeEqID[ws].size();
  workset.wsElNodeEqID = wsElNodeEqID[ws];
  workset.wsCoords = coords[ws];
  workset.EBName = wsEBNames[ws];

  // Sidesets are integrated within the Cells
  loadWorksetSidesetInfo(workset, ws);

  workset.stateArrayPtr = &stateMgr.getStateArray(ws);
  workset.eigenDataPtr = stateMgr.getEigenData();

  PHAL::BuildSerializer<EvalT> bs(workset);
}

#endif // ALBANY_APPLICATION_HPP
