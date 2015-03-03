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
* ASSUMES ANY LIABILITY L1L2R THE USE OF THIS SOFTWARE. This notice    *
* including this sentence must appear on any copies of this software.*
*    Questions to Andy Salinger, agsalin@sandia.gov                  *
\********************************************************************/


#ifndef FELIX_VISCOSITYL1L2_HPP
#define FELIX_VISCOSITYL1L2_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Sacado_ParameterAccessor.hpp" 
#include "Albany_Layouts.hpp"

namespace FELIX {
/** \brief Finite Element Interpolation Evaluator

    This evaluator interpolates nodal DOF values to quad points.

*/

template<typename EvalT, typename Traits>
class ViscosityL1L2 : public PHX::EvaluatorWithBaseImpl<Traits>,
		    public PHX::EvaluatorDerived<EvalT, Traits>,
		    public Sacado::ParameterAccessor<EvalT, SPL_Traits> {

public:

  typedef typename EvalT::ScalarT ScalarT;

  ViscosityL1L2(const Teuchos::ParameterList& p,
                const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

  ScalarT& getValue(const std::string &n); 

private:
 
  typedef typename EvalT::MeshScalarT MeshScalarT;

  ScalarT homotopyParam;

  //coefficients for Glen's law
  double A; 
  double n;

  //coefficients for ISMIP-HOM test cases
  double L; 
  double alpha;

  std::size_t numQPsZ; //number of quadrature points for z-integral 
  std::string surfType; //type of surface, e.g., Test A 

  // Input:
  PHX::MDField<MeshScalarT,Cell,QuadPoint, Dim> coordVec;
  PHX::MDField<ScalarT,Cell,QuadPoint> epsilonB;

  // Output:
  PHX::MDField<ScalarT,Cell,QuadPoint> mu;

  unsigned int numQPs, numDims, numNodes;
  
  enum VISCTYPE {CONSTANT, GLENSLAW};
  VISCTYPE visc_type;
  enum SURFTYPE {BOX, TESTA};
  SURFTYPE surf_type;
 
};
}

#endif
