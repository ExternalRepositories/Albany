//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#if !defined(LCM_StrongDBC_hpp)
#define LCM_StrongDBC_hpp

#include "PHAL_AlbanyTraits.hpp"
#include "PHAL_Dirichlet.hpp"
#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Sacado_ParameterAccessor.hpp"
#include "Teuchos_ParameterList.hpp"

namespace LCM {

//
// \brief Schwarz for models BC Dirichlet evaluator
//

//
// Specialization of the DirichletBase class
//
template<typename EvalT, typename Traits> class StrongDBC;

template<typename EvalT, typename Traits>
class StrongDBC_Base: public PHAL::DirichletBase<EvalT, Traits>
{
public:
  typedef typename EvalT::ScalarT ScalarT;

  StrongDBC_Base(Teuchos::ParameterList & p);

};

//
// Residual
//
template<typename Traits>
class StrongDBC<PHAL::AlbanyTraits::Residual, Traits>
: public StrongDBC_Base<PHAL::AlbanyTraits::Residual, Traits>
{
public:
  StrongDBC(Teuchos::ParameterList & p);
  typedef typename PHAL::AlbanyTraits::Residual::ScalarT ScalarT;
  void evaluateFields(typename Traits::EvalData d);
};

//
// Jacobian
//
template<typename Traits>
class StrongDBC<PHAL::AlbanyTraits::Jacobian, Traits>
: public StrongDBC_Base<PHAL::AlbanyTraits::Jacobian, Traits>
{
public:
  StrongDBC(Teuchos::ParameterList & p);
  typedef typename PHAL::AlbanyTraits::Jacobian::ScalarT ScalarT;
  void evaluateFields(typename Traits::EvalData d);
};

//
// Tangent
//
template<typename Traits>
class StrongDBC<PHAL::AlbanyTraits::Tangent, Traits>
: public StrongDBC_Base<PHAL::AlbanyTraits::Tangent, Traits>
{
public:
  StrongDBC(Teuchos::ParameterList & p);
  typedef typename PHAL::AlbanyTraits::Tangent::ScalarT ScalarT;
  void evaluateFields(typename Traits::EvalData d);
};

//
// Distributed Parameter Derivative
//
template<typename Traits>
class StrongDBC<PHAL::AlbanyTraits::DistParamDeriv, Traits>
: public StrongDBC_Base<PHAL::AlbanyTraits::DistParamDeriv, Traits>
{
public:
  StrongDBC(Teuchos::ParameterList & p);
  typedef typename PHAL::AlbanyTraits::DistParamDeriv::ScalarT ScalarT;
  void evaluateFields(typename Traits::EvalData d);
};

} // namespace LCM

#endif // LCM_StrongDBC_hpp
