//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef DIFFUSIONCOEFFICIENT_HPP
#define DIFFUSIONCOEFFICIENT_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

namespace LCM {

  /// \brief Diffusion Coefficient Evaluator
  ///
  /// evaulate diffusion coefficient \f$ D_{o} \exp(-Q/RT) \f$
  ///
  template<typename EvalT, typename Traits>
  class DiffusionCoefficient : public PHX::EvaluatorWithBaseImpl<Traits>,
                               public PHX::EvaluatorDerived<EvalT, Traits>  {

  public:

    DiffusionCoefficient(const Teuchos::ParameterList& p);

    void postRegistrationSetup(typename Traits::SetupData d,
                               PHX::FieldManager<Traits>& vm);

    void evaluateFields(typename Traits::EvalData d);

  private:

    typedef typename EvalT::ScalarT ScalarT;
    typedef typename EvalT::MeshScalarT MeshScalarT;

    // Input:
    PHX::MDField<ScalarT,Cell,QuadPoint> Qdiff;
    PHX::MDField<ScalarT,Cell,QuadPoint> temperature;
    PHX::MDField<ScalarT,Cell,QuadPoint> Dpre;

    RealType Rideal;

    // Output:
    PHX::MDField<ScalarT,Cell,QuadPoint,Dim,Dim> diffusionCoefficient;

    unsigned int numQPs;
   // unsigned int numDims;
  };
}

#endif
