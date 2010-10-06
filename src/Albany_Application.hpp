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

#include "Epetra_Vector.h"
#include "Epetra_CrsMatrix.h"
#include "Epetra_Import.h"
#include "Epetra_Export.h"

#include "Albany_AbstractDiscretization.hpp"
#include "Albany_AbstractProblem.hpp"

#include "CUTR_AbstractMeshMover.hpp"

#include "Sacado_ScalarParameterLibrary.hpp"
#include "Sacado_ScalarParameterVector.hpp"
#include "Sacado_ParameterAccessor.hpp"
#include "Sacado_ParameterRegistration.hpp"

#include "PHAL_AlbanyTraits.hpp"
#include "Phalanx.hpp"

#include "Stokhos_OrthogPolyExpansion.hpp"
#include "Stokhos_VectorOrthogPoly.hpp"
#include "Stokhos_VectorOrthogPolyTraitsEpetra.hpp"

#include "Teko_InverseLibrary.hpp"

namespace Albany {

  class Application :
     public Sacado::ParameterAccessor<PHAL::AlbanyTraits::Residual, SPL_Traits> {
  public:

    //! Constructor 
    Application(const Teuchos::RCP<const Epetra_Comm>& comm,
		const Teuchos::RCP<Teuchos::ParameterList>& params,
		const Teuchos::RCP<Epetra_Vector>& initial_guess = 
		Teuchos::null);

    //! Destructor
    ~Application();

    //! Get underlying abstract discretization
    Teuchos::RCP<Albany::AbstractDiscretization> getDiscretization() const;

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

    //! Get response map
    Teuchos::RCP<const Epetra_Map> getResponseMap() const;

    //! Return whether problem is transient
    bool isTransient() const;

    //! Return whether problem wants to use its own preconditioner
    bool suppliesPreconditioner() const;

    //! Get stochastic expansion
    Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,double> >
    getStochasticExpansion();

    //! Intialize stochastic Galerkin method
    void init_sg(const Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,double> >& expansion);

    //! Compute global residual
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalResidual(
			 const double current_time,
			 const Epetra_Vector* xdot,
			 const Epetra_Vector& x,
			 const Teuchos::Array< Teuchos::RCP<ParamVec> >& p,
			 Epetra_Vector& f);

    //! Compute global Jacobian
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalJacobian(
			 const double alpha, 
			 const double beta,  
			 const double current_time,
			 const Epetra_Vector* xdot,
			 const Epetra_Vector& x,
			 const Teuchos::Array< Teuchos::RCP<ParamVec> >& p,
			 Epetra_Vector* f,
			 Epetra_CrsMatrix& jac);

    //! Compute global Preconditioner
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalPreconditioner(
                             const Teuchos::RCP<Epetra_CrsMatrix>& jac,
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
                              const Teuchos::Array< Teuchos::RCP<ParamVec> >& p,
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
    virtual void 
    evaluateResponses(const Epetra_Vector* xdot,
                      const Epetra_Vector& x,
                      const Teuchos::Array< Teuchos::RCP<ParamVec> >& p,
                      Epetra_Vector& g);

    //! Evaluate tangent = dg/dx*dx/dp + dg/dxdot*dxdot/dp + dg/dp
    /*!
     * Set xdot, dxdot_dp to NULL for steady-state problems
     */
    virtual void 
    evaluateResponseTangents(
	   const Epetra_Vector* xdot,
	   const Epetra_Vector& x,
	   const Teuchos::Array< Teuchos::RCP<ParamVec> >& p,
	   const Teuchos::Array< Teuchos::RCP<ParamVec> >& deriv_p,
	   const Teuchos::Array< Teuchos::RCP<Epetra_MultiVector> >& dxdot_dp,
	   const Teuchos::Array< Teuchos::RCP<Epetra_MultiVector> >& dx_dp,
	   Epetra_Vector* g,
	   const Teuchos::Array< Teuchos::RCP<Epetra_MultiVector> >& gt);

    //! Evaluate gradient = dg/dx, dg/dxdot, dg/dp
    /*!
     * Set xdot, dg_dxdot to NULL for steady-state problems
     */
    virtual void 
    evaluateResponseGradients(
	    const Epetra_Vector* xdot,
	    const Epetra_Vector& x,
	    const Teuchos::Array< Teuchos::RCP<ParamVec> >& p,
	    const Teuchos::Array< Teuchos::RCP<ParamVec> >& deriv_p,
	    Epetra_Vector* g,
	    Epetra_MultiVector* dg_dx,
	    Epetra_MultiVector* dg_dxdot,
	    const Teuchos::Array< Teuchos::RCP<Epetra_MultiVector> >& dg_dp);

    //! Compute global residual for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalSGResidual(
			const double current_time,
		        const Stokhos::VectorOrthogPoly<Epetra_Vector>* sg_xdot,
			const Stokhos::VectorOrthogPoly<Epetra_Vector>& sg_x,
			const ParamVec* p,
			const ParamVec* sg_p,
			const Teuchos::Array<SGType>* sg_p_vals,
			Stokhos::VectorOrthogPoly<Epetra_Vector>& sg_f);

    //! Compute global Jacobian for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalSGJacobian(
			double alpha, double beta,
			const double current_time,
			const Stokhos::VectorOrthogPoly<Epetra_Vector>* sg_xdot,
			const Stokhos::VectorOrthogPoly<Epetra_Vector>& sg_x,
			const ParamVec* p,
			const ParamVec* sg_p,
			const Teuchos::Array<SGType>* sg_p_vals,
			Stokhos::VectorOrthogPoly<Epetra_Vector>* sg_f,
			Stokhos::VectorOrthogPoly<Epetra_CrsMatrix>& sg_jac);

    //! Evaluate stochastic Galerkin response functions
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void 
    evaluateSGResponses(const Stokhos::VectorOrthogPoly<Epetra_Vector>* sg_xdot,
			const Stokhos::VectorOrthogPoly<Epetra_Vector>& sg_x,
			const ParamVec* p,
			const ParamVec* sg_p,
			const Teuchos::Array<SGType>* sg_p_vals,
			Stokhos::VectorOrthogPoly<Epetra_Vector>& sg_g);

    //! Provide access to shapeParameters -- no AD
    PHAL::AlbanyTraits::Residual::ScalarT& getValue(const std::string &n);

    //! Function to copy newState into oldState
    void updateState();

  private:

    //! Call to Teko to build strided block operator
    Teuchos::RCP<Epetra_Operator> buildWrappedOperator(
                           const Teuchos::RCP<Epetra_Operator>& Jac,
                           const Teuchos::RCP<Epetra_Operator>& wrapInput,
                           bool reorder=false) const;

    //! Utility function to set up ShapeParameters through Sacado
    void registerShapeParameters();
    
    //! Utility function write Graph vis file for first fill type called
    void writeGraphVisFile() const;
    
    //! Private to prohibit copying
    Application(const Application&);

    //! Private to prohibit copying
    Application& operator=(const Application&);

  protected:

    //! Output stream, defaults to pronting just Proc 0
    Teuchos::RCP<Teuchos::FancyOStream> out;

    //! Is problem transient
    bool transient;
    
    //! Element discretization
    Teuchos::RCP<Albany::AbstractDiscretization> disc;

    //! Initial solution vector
    Teuchos::RCP<Epetra_Vector> initial_x;

    //! Initial solution vector
    Teuchos::RCP<Epetra_Vector> initial_x_dot;

    //! Importer for overlapped data
    Teuchos::RCP<Epetra_Import> importer;

    //! Exporter for overlapped data
    Teuchos::RCP<Epetra_Export> exporter;

    //! Overlapped solution vector
    Teuchos::RCP<Epetra_Vector> overlapped_x;

    //! Overlapped time derivative vector
    Teuchos::RCP<Epetra_Vector> overlapped_xdot;

    //! Overlapped residual vector
    Teuchos::RCP<Epetra_Vector> overlapped_f;

    //! Overlapped Jacobian matrix
    Teuchos::RCP<Epetra_CrsMatrix> overlapped_jac;

    //! Parameter library
    Teuchos::RCP<ParamLib> paramLib;

    //! Response functions
    std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> > responses;

    //! Map for combined response functions
    Teuchos::RCP<Epetra_Map> response_map;

    //! Phalanx Field Manager for volumetric fills
    Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > fm;

    //! Phalanx Field Manager for Dirichlet Conditions
    Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > dfm;

    Teuchos::ArrayRCP<double> coordinates;
    Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > elNodeID;

    //! Stochastic Galerkin expansion
    Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,double> > sg_expansion;

    //! SG overlapped solution vectors
    Teuchos::RCP< Stokhos::VectorOrthogPoly<Epetra_Vector> >  sg_overlapped_x;

    //! SG overlapped time derivative vectors
    Teuchos::RCP< Stokhos::VectorOrthogPoly<Epetra_Vector> > sg_overlapped_xdot;

    //! SG overlapped residual vectors
    Teuchos::RCP< Stokhos::VectorOrthogPoly<Epetra_Vector> > sg_overlapped_f;

    //! Overlapped Jacobian matrixs
    Teuchos::RCP< Stokhos::VectorOrthogPoly<Epetra_CrsMatrix> > sg_overlapped_jac;

    //! Data for Physics-Based Preconditioners
    bool physicsBasedPreconditioner;
    Teuchos::RCP<Teuchos::ParameterList> tekoParams;

    //! Shape Optimization data
    bool shapeParamsHaveBeenReset;
    std::vector<RealType> shapeParams;
    std::vector<std::string> shapeParamNames;
    Teuchos::RCP<CUTR::AbstractMeshMover> meshMover;

    unsigned int neq;

    //! Size of a chunk of elements to be processed at once.
    int worksetSize;

    //! State data, allocated in Problem class
    Teuchos::RCP<Intrepid::FieldContainer<RealType> > oldState;
    Teuchos::RCP<Intrepid::FieldContainer<RealType> > newState;


    //! Teko stuff
    Teuchos::RCP<Teko::InverseLibrary> inverseLib;
    Teuchos::RCP<Teko::InverseFactory> inverseFac;
    Teuchos::RCP<Epetra_Operator> wrappedJac;
    std::vector<int> blockDecomp;

    std::vector<Teuchos::RCP<Teuchos::Time> > timers;

    bool setupCalledResidual;
    bool setupCalledJacobian;
    bool setupCalledTangent;
    bool setupCalledSGResidual;
    bool setupCalledSGJacobian;
    mutable int phxGraphVisDetail;
  };
}
#endif // ALBANY_APPLICATION_HPP
