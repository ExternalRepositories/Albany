//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

//IK, 9/13/14: Epetra ifdef'ed out if ALBANY_EPETRA_EXE is off.

#ifndef ALBANY_SOLUTION_RESPONSE_FUNCTION_HPP
#define ALBANY_SOLUTION_RESPONSE_FUNCTION_HPP

#include "Albany_DistributedResponseFunction.hpp"
#include "Albany_Application.hpp"

#include "Teuchos_ParameterList.hpp"
#include "Teuchos_Array.hpp"

#if defined(ALBANY_EPETRA)
#include "Epetra_Map.h"
#include "Epetra_Import.h"
#include "Epetra_CrsGraph.h"
#endif

namespace Albany {

  /*!
   * \brief A response function given by (possibly a portion of) the solution
   */
  class SolutionResponseFunction : public DistributedResponseFunction {
  public:
  
    //! Default constructor
    SolutionResponseFunction(
      const Teuchos::RCP<Albany::Application>& application,
      Teuchos::ParameterList& responseParams);

    //! Destructor
    virtual ~SolutionResponseFunction();

#if defined(ALBANY_EPETRA)
    //! Setup response function
    virtual void setup();
#endif

    //! Setup response function
    virtual void setupT();

#if defined(ALBANY_EPETRA)
    //! Get the map associate with this response
    virtual Teuchos::RCP<const Epetra_Map> responseMap() const;
#endif
    
    //! Get the map associate with this response
    virtual Teuchos::RCP<const Tpetra_Map> responseMapT() const;

    //! Create operator for gradient
#if defined(ALBANY_EPETRA)
    virtual Teuchos::RCP<Epetra_Operator> createGradientOp() const;
#endif
    virtual Teuchos::RCP<Tpetra_Operator> createGradientOpT() const;

    //! \name Deterministic evaluation functions
    //@{

    //! Evaluate responses
    virtual void evaluateResponseT(
      const double current_time,
      const Tpetra_Vector* xdotT,
      const Tpetra_Vector* xdotdotT,
      const Tpetra_Vector& xT,
      const Teuchos::Array<ParamVec>& p,
      Tpetra_Vector& gT);
    
    //! Evaluate tangent = dg/dx*dx/dp + dg/dxdot*dxdot/dp + dg/dp
    virtual void evaluateTangentT(
      const double alpha, 
      const double beta,
      const double omega,
      const double current_time,
      bool sum_derivs,
      const Tpetra_Vector* xdot,
      const Tpetra_Vector* xdotdot,
      const Tpetra_Vector& x,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      const Tpetra_MultiVector* Vxdot,
      const Tpetra_MultiVector* Vxdotdot,
      const Tpetra_MultiVector* Vx,
      const Tpetra_MultiVector* Vp,
      Tpetra_Vector* g,
      Tpetra_MultiVector* gx,
      Tpetra_MultiVector* gp);

#if defined(ALBANY_EPETRA)
    //! Evaluate gradient = dg/dx, dg/dxdot, dg/dp
    virtual void evaluateGradient(
      const double current_time,
      const Epetra_Vector* xdot,
      const Epetra_Vector* xdotdot,
      const Epetra_Vector& x,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      Epetra_Vector* g,
      Epetra_Operator* dg_dx,
      Epetra_Operator* dg_dxdot,
      Epetra_Operator* dg_dxdotdot,
      Epetra_MultiVector* dg_dp);
#endif

    //! Evaluate gradient = dg/dx, dg/dxdot, dg/dp - Tpetra
    virtual void evaluateGradientT(
      const double current_time,
      const Tpetra_Vector* xdotT,
      const Tpetra_Vector* xdotdotT,
      const Tpetra_Vector& xT,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      Tpetra_Vector* gT,
      Tpetra_Operator* dg_dxT,
      Tpetra_Operator* dg_dxdotT,
      Tpetra_Operator* dg_dxdotdotT,
      Tpetra_MultiVector* dg_dpT);

    //! Evaluate distributed parameter derivative = dg/dp
    virtual void
    evaluateDistParamDerivT(
          const double current_time,
          const Tpetra_Vector* xdotT,
          const Tpetra_Vector* xdotdotT,
          const Tpetra_Vector& xT,
          const Teuchos::Array<ParamVec>& param_array,
          const std::string& dist_param_name,
          Tpetra_MultiVector* dg_dpT);
    //@}

  private:

    //! Private to prohibit copying
    SolutionResponseFunction(const SolutionResponseFunction&);
    
    //! Private to prohibit copying
    SolutionResponseFunction& operator=(const SolutionResponseFunction&);

  protected:

#if defined(ALBANY_EPETRA)
    Teuchos::RCP<Epetra_Map> 
    buildCulledMap(const Epetra_Map& x_map, 
		   const Teuchos::Array<int>& keepDOF) const;
#endif
    
    Teuchos::RCP<const Tpetra_Map> 
    buildCulledMapT(const Tpetra_Map& x_mapT, 
		   const Teuchos::Array<int>& keepDOF) const;

#if defined(ALBANY_EPETRA)
    void cullSolution(const Epetra_MultiVector& x, 
		      Epetra_MultiVector& x_culled) const;
#endif
    
    //Tpetra version of above function
    void cullSolutionT(const Tpetra_MultiVector& xT, 
		      Tpetra_MultiVector& x_culledT) const;

  protected:

    //! Application to get global maps
    Teuchos::RCP<Albany::Application> application;

    //! Mask for DOFs to keep
    Teuchos::Array<int> keepDOF;

#if defined(ALBANY_EPETRA)
    //! Epetra map for response
    Teuchos::RCP<const Epetra_Map> culled_map;
#endif
    
    //! Tpetra map for response
    Teuchos::RCP<const Tpetra_Map> culled_mapT;

#if defined(ALBANY_EPETRA)
    //! Importer mapping between full and culled solution
    Teuchos::RCP<Epetra_Import> importer; 
#endif

    //! Tpetra importer mapping between full and culled solution
    Teuchos::RCP<Tpetra_Import> importerT;

#if defined(ALBANY_EPETRA)
    //! Graph of gradient operator
    Teuchos::RCP<Epetra_CrsGraph> gradient_graph;
#endif    

    //! Graph of gradient operator - Tpetra version
    Teuchos::RCP<Tpetra_CrsGraph> gradient_graphT;

  };

}

#endif // ALBANY_SOLUTION_RESPONSE_FUNCTION_HPP
