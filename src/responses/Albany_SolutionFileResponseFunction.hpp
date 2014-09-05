//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef ALBANY_SOLUTIONFILERESPONSEFUNCTION_HPP
#define ALBANY_SOLUTIONFILERESPONSEFUNCTION_HPP

#include "Albany_SamplingBasedScalarResponseFunction.hpp"

namespace Albany {

  /*!
   * \brief Response function representing the difference from a stored vector on disk
   */
  template<class VectorNorm>
  class SolutionFileResponseFunction : 
    public SamplingBasedScalarResponseFunction {
  public:
  
    //! Default constructor
    SolutionFileResponseFunction(const Teuchos::RCP<const Epetra_Comm>& comm);

    //! Destructor
    virtual ~SolutionFileResponseFunction();

    //! Get the number of responses
    virtual unsigned int numResponses() const;

    //! Evaluate responses
    virtual void 
    evaluateResponseT(const double current_time,
		     const Tpetra_Vector* xdotT,
		     const Tpetra_Vector* xdotdotT,
		     const Tpetra_Vector& xT,
		     const Teuchos::Array<ParamVec>& p,
		     Tpetra_Vector& gT);

    //! Evaluate tangent = dg/dx*dx/dp + dg/dxdot*dxdot/dp + dg/dp
    virtual void 
    evaluateTangentT(const double alpha, 
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

#ifdef ALBANY_EPETRA
    //! Evaluate gradient = dg/dx, dg/dxdot, dg/dp
    virtual void 
    evaluateGradient(const double current_time,
		     const Epetra_Vector* xdot,
		     const Epetra_Vector* xdotdot,
		     const Epetra_Vector& x,
		     const Teuchos::Array<ParamVec>& p,
		     ParamVec* deriv_p,
		     Epetra_Vector* g,
		     Epetra_MultiVector* dg_dx,
		     Epetra_MultiVector* dg_dxdot,
		     Epetra_MultiVector* dg_dxdotdot,
		     Epetra_MultiVector* dg_dp);
#endif
  
    virtual void 
    evaluateGradientT(const double current_time,
		     const Tpetra_Vector* xdotT,
		     const Tpetra_Vector* xdotdotT,
		     const Tpetra_Vector& xT,
		     const Teuchos::Array<ParamVec>& p,
		     ParamVec* deriv_p,
		     Tpetra_Vector* gT,
		     Tpetra_MultiVector* dg_dxT,
		     Tpetra_MultiVector* dg_dxdotT,
		     Tpetra_MultiVector* dg_dxdotdotT,
		     Tpetra_MultiVector* dg_dpT);

  private:

    //! Private to prohibit copying
    SolutionFileResponseFunction(const SolutionFileResponseFunction&);
    
    //! Private to prohibit copying
    SolutionFileResponseFunction& operator=(const SolutionFileResponseFunction&);

    //! Reference Vector
    Epetra_Vector* RefSoln;
    //! Reference Vector - Tpetra
    Tpetra_Vector* RefSolnT;

    bool solutionLoaded;

    //! Basic idea borrowed from EpetraExt - TO DO: put it back there?
    int MatrixMarketFileToVector( const char *filename, const Epetra_BlockMap & map, Epetra_Vector * & A);
    int MatrixMarketFileToMultiVector( const char *filename, const Epetra_BlockMap & map, Epetra_MultiVector * & A);
    
    int MatrixMarketFileToTpetraVector( const char *filename, const Tpetra_Map & map, Tpetra_Vector * & A);
    int MatrixMarketFileToTpetraMultiVector( const char *filename, const Tpetra_Map & map, Tpetra_MultiVector * & A);

  };

//	namespace SolutionFileResponseFunction {
	
	  struct NormTwo {
	
	    double Norm(const Epetra_Vector& vec){ double norm; vec.Norm2(&norm); return norm * norm;}
	    double NormT(const Tpetra_Vector& vecT){ Teuchos::ScalarTraits<ST>::magnitudeType normT = vecT.norm2(); return normT * normT;}
	
	  };
	
	  struct NormInf {
	
	    double Norm(const Epetra_Vector& vec){ double norm; vec.NormInf(&norm); return norm;}
	    double NormT(const Tpetra_Vector& vecT){ Teuchos::ScalarTraits<ST>::magnitudeType normT = vecT.normInf(); return normT;}
	
	  };
//	}

}

// Define macro for explicit template instantiation
#define SOLFILERESP_INSTANTIATE_TEMPLATE_CLASS_NORMTWO(name) \
  template class name<Albany::NormTwo>;
#define SOLFILERESP_INSTANTIATE_TEMPLATE_CLASS_NORMINF(name) \
  template class name<Albany::NormInf>;

#define SOLFILERESP_INSTANTIATE_TEMPLATE_CLASS(name) \
  SOLFILERESP_INSTANTIATE_TEMPLATE_CLASS_NORMTWO(name) \
  SOLFILERESP_INSTANTIATE_TEMPLATE_CLASS_NORMINF(name)

#endif // ALBANY_SOLUTIONFILERESPONSEFUNCTION_HPP
