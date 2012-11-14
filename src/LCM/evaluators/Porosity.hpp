//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef POROSITY_HPP
#define POROSITY_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Teuchos_ParameterList.hpp"
#include "Epetra_Vector.h"
#include "Sacado_ParameterAccessor.hpp"
#include "Stokhos_KL_ExponentialRandomField.hpp"
#include "Teuchos_Array.hpp"

namespace LCM {
/** 
 * \brief Evaluates porosity, either as a constant or a truncated
 * KL expansion.
 */

// Porosity update is the most important part for the poromechanics
// formulation. All poroelasticity parameters (Biot Coefficient,
// Biot modulus, permeability, and consistent tangential tensor)
// depend on porosity. The definition we used here is from
// Coussy's poromechanics p.85.

template<typename EvalT, typename Traits>
class Porosity :
  public PHX::EvaluatorWithBaseImpl<Traits>,
  public PHX::EvaluatorDerived<EvalT, Traits>,
  public Sacado::ParameterAccessor<EvalT, SPL_Traits> {
  
public:
  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  Porosity(Teuchos::ParameterList& p);
  
  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);
  
  void evaluateFields(typename Traits::EvalData d);
  
  ScalarT& getValue(const std::string &n);

private:

  std::size_t numQPs;
  std::size_t numDims;
  PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim> coordVec;
  PHX::MDField<ScalarT,Cell,QuadPoint> porosity;

  //! Is porosity constant, or random field
  bool is_constant;

  //! Constant value
  ScalarT constant_value;

  //! Optional dependence on strain and porePressure
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim,Dim> strain; // porosity holds linear relation to volumetric strain

  bool isPoroElastic;
  bool isCompressibleSolidPhase;
  bool isCompressibleFluidPhase;
  ScalarT initialPorosity_value;

  // For compressible grain
  PHX::MDField<ScalarT,Cell,QuadPoint> biotCoefficient;
  PHX::MDField<ScalarT,Cell,QuadPoint> porePressure;
  ScalarT GrainBulkModulus;

  // ScalarT dEdT_value;  NOTE: a term needed to be add later on...for now, keep it simple.

  //! Exponential random field
  Teuchos::RCP< Stokhos::KL::ExponentialRandomField<MeshScalarT> > exp_rf_kl;

  //! Values of the random variables
  Teuchos::Array<ScalarT> rv;
};
}

#endif
