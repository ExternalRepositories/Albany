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


#ifndef QCAD_POISSONSOURCE_HPP
#define QCAD_POISSONSOURCE_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Teuchos_ParameterList.hpp"
#include "Epetra_Vector.h"
#include "Sacado_ParameterAccessor.hpp"
#include "Stokhos_KL_ExponentialRandomField.hpp"
#include "Teuchos_Array.hpp"

/** 
 * \brief Evaluates Poisson Source Term 
 */
namespace QCAD 
{
	template<typename EvalT, typename Traits>
	class PoissonSource : 
  public PHX::EvaluatorWithBaseImpl<Traits>,
  public PHX::EvaluatorDerived<EvalT, Traits>,
  public Sacado::ParameterAccessor<EvalT, SPL_Traits> 
  {
	public:
  	typedef typename EvalT::ScalarT ScalarT;
  	typedef typename EvalT::MeshScalarT MeshScalarT;

  	PoissonSource(Teuchos::ParameterList& p);
  
  	void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);
  
  	void evaluateFields(typename Traits::EvalData d);
  
  	//! Function to allow parameters to be exposed for embedded analysis
  	ScalarT& getValue(const std::string &n);

	private:

  	//! Reference parameter list generator to check xml input file
  	Teuchos::RCP<const Teuchos::ParameterList>
    		getValidPoissonSourceParameters() const;

  	// Suzey: need to assign values to private variables, so remove "const"
  	ScalarT chargeDistribution( const int numDim,
        const MeshScalarT* coord, const ScalarT& phi);

  	//! input
  	std::size_t numQPs;
  	std::size_t numDims;
  	PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim> coordVec;
  	PHX::MDField<ScalarT,Cell,QuadPoint> potential;	// scaled potential, no unit.

  	//! output
  	PHX::MDField<ScalarT,Cell,QuadPoint> poissonSource;

  	//! constant prefactor parameter in source function
  	ScalarT factor;
  	
  	//! string variable to differ the various devices implementation
  	std::string device;
  	
  	//! variables to hold the computed output quantities
  	ScalarT chargeDensity; 	    // space charge density in [cm-3]
  	ScalarT electronDensity;		// electron density in [cm-3]
  	ScalarT holeDensity;				// hole density in [cm-3]
  	ScalarT electricPotential;	// electric potential in [V]
	};
}

#endif
