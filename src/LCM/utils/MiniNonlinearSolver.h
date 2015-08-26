//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#if !defined(Intrepid_NonlinearSolver_h)
#define Intrepid_NonlinearSolver_h

#include <utility>
#include "PHAL_AlbanyTraits.hpp"
#include <Intrepid_MiniTensor.h>

namespace LCM{

///
/// Residual interafce for mini nonlinear solver
/// To use the solver framework, derive from this class and perform
/// residual computations in the compute method.
///
template <typename T, Intrepid::Index N = Intrepid::DYNAMIC>
class Residual_Base
{
public:
  virtual
  Intrepid::Vector<T, N>
  compute(Intrepid::Vector<T, N> const & x) = 0;

  virtual
  ~Residual_Base() {}
};

///
/// Newton Solver Base class
///
template<typename EvalT, typename Residual,
Intrepid::Index N = Intrepid::DYNAMIC>
class NewtonSolver_Base
{
public:
  using ScalarT = typename EvalT::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;

  virtual
  ~NewtonSolver_Base() {}

  virtual
  void
  solve(Residual & residual, Intrepid::Vector<FadT, N> & x) {}

  template <typename T>
  void setMaximumNumberIterations(T && mni)
  {max_num_iter_ = std::forward<T>(mni);}

  Intrepid::Index
  getMaximumNumberIterations()
  {return max_num_iter_;}

  Intrepid::Index
  getNumberIterations()
  {return num_iter_;}

  template <typename T>
  void setRelativeTolerance(T && rt)
  {rel_tol_ = std::forward<T>(rt);}

  ValueT
  getRelativeTolerance() const
  {return rel_tol_;}

  ValueT
  getRelativeError() const
  {return rel_error_;}

  template <typename T>
  void setAbsoluteTolerance(T && at)
  {abs_tol_ = std::forward<T>(at);}

  ValueT
  getAbsoluteTolerance() const
  {return abs_tol_;}

  ValueT
  getAbsoluteError() const
  {return abs_error_;}

  bool
  isConverged() const
  {return converged_;}

protected:
  Intrepid::Index
  max_num_iter_{128};

  Intrepid::Index
  num_iter_{0};

  ValueT
  rel_tol_{1.0e-10};

  ValueT
  rel_error_{1.0};

  ValueT
  abs_tol_{1.0e-10};

  ValueT
  abs_error_{1.0};

  bool
  converged_{false};
};

//
// Specializations
//
template<typename EvalT, typename Residual,
Intrepid::Index N = Intrepid::DYNAMIC>
class NewtonSolver;

//
// Residual
//
template<typename Residual, Intrepid::Index N>
class NewtonSolver<PHAL::AlbanyTraits::Residual, Residual, N> :
    public NewtonSolver_Base<PHAL::AlbanyTraits::Residual, Residual, N>
{
public:
  using ScalarT = typename PHAL::AlbanyTraits::Residual::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;

  void
  solve(
      Residual & residual,
      Intrepid::Vector<FadT, N> & x) override;
};

//
// Jacobian
//
template<typename Residual, Intrepid::Index N>
class NewtonSolver<PHAL::AlbanyTraits::Jacobian, Residual, N> :
    public NewtonSolver_Base<PHAL::AlbanyTraits::Jacobian, Residual, N>
{
public:
  using ScalarT = typename PHAL::AlbanyTraits::Jacobian::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// Tangent
//
template<typename Residual, Intrepid::Index N>
class NewtonSolver<PHAL::AlbanyTraits::Tangent, Residual, N> :
    public NewtonSolver_Base<PHAL::AlbanyTraits::Tangent, Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::Tangent::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// Distribured Parameter Derivative
//
template<typename Residual, Intrepid::Index N>
class NewtonSolver<PHAL::AlbanyTraits::DistParamDeriv, Residual, N> :
    public NewtonSolver_Base<PHAL::AlbanyTraits::DistParamDeriv,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::DistParamDeriv::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

#ifdef ALBANY_SG
//
// SGResidual
//
template<typename Residual, Intrepid::Index N>
class NewtonSolver<PHAL::AlbanyTraits::SGResidual, Residual, N> :
    public NewtonSolver_Base<PHAL::AlbanyTraits::SGResidual,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::SGResidual::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// SGJacobian
//
template<typename Residual, Intrepid::Index N>
class NewtonSolver<PHAL::AlbanyTraits::SGJacobian, Residual, N> :
    public NewtonSolver_Base<PHAL::AlbanyTraits::SGJacobian,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::SGJacobian::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// SGTangent
//
template<typename Residual, Intrepid::Index N>
class NewtonSolver<PHAL::AlbanyTraits::SGTangent, Residual, N> :
    public NewtonSolver_Base<PHAL::AlbanyTraits::SGTangent,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::SGTangent::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};
#endif

#ifdef ALBANY_ENSEMBLE
//
// MPResidual
//
template<typename Residual, Intrepid::Index N>
class NewtonSolver<PHAL::AlbanyTraits::MPResidual, Residual, N> :
    public NewtonSolver_Base<PHAL::AlbanyTraits::MPResidual,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::MPResidual::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// MPJacobian
//
template<typename Residual, Intrepid::Index N>
class NewtonSolver<PHAL::AlbanyTraits::MPJacobian, Residual, N> :
    public NewtonSolver_Base<PHAL::AlbanyTraits::MPJacobian,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::MPJacobian::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// MPTangent
//
template<typename Residual, Intrepid::Index N>
class NewtonSolver<PHAL::AlbanyTraits::MPTangent, Residual, N> :
    public NewtonSolver_Base<PHAL::AlbanyTraits::MPTangent,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::MPTangent::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};
#endif

///
/// TrustRegion Solver Base class. See Nocedal's algorithm 11.5.
///
template<typename EvalT, typename Residual,
Intrepid::Index N = Intrepid::DYNAMIC>
class TrustRegionSolver_Base
{
public:
  using ScalarT = typename EvalT::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;

  virtual
  ~TrustRegionSolver_Base() {}

  virtual
  void
  solve(Residual & residual, Intrepid::Vector<FadT, N> & x) {}

  template <typename T>
  void setMaximumNumberIterations(T && mni)
  {max_num_iter_ = std::forward<T>(mni);}

  Intrepid::Index
  getMaximumNumberIterations()
  {return max_num_iter_;}

  template <typename T>
  void setMaximumNumberTrustRegionIterations(T && mntri)
  {max_num_trust_region_iter_ = std::forward<T>(mntri);}

  Intrepid::Index
  getMaximumNumberTrustRegionIterations()
  {return max_num_trust_region_iter_;}

  Intrepid::Index
  getNumberIterations()
  {return num_iter_;}

  template <typename T>
  void setMaximumStepLength(T && msl)
  {max_step_length_ = std::forward<T>(msl);}

  ValueT
  getMaximumStepLength() const
  {return max_step_length_;}

  template <typename T>
  void setInitialStepLength(T && isl)
  {initial_step_length_ = std::forward<T>(isl);}

  ValueT
  getInitialStepLength() const
  {return initial_step_length_;}

  template <typename T>
  void setMinimumReduction(T && mr)
  {min_reduction_ = std::forward<T>(mr);}

  ValueT
  getMinumumReduction() const
  {return min_reduction_;}

  template <typename T>
  void setRelativeTolerance(T && rt)
  {rel_tol_ = std::forward<T>(rt);}

  ValueT
  getRelativeTolerance() const
  {return rel_tol_;}

  ValueT
  getRelativeError() const
  {return rel_error_;}

  template <typename T>
  void setAbsoluteTolerance(T && at)
  {abs_tol_ = std::forward<T>(at);}

  ValueT
  getAbsoluteTolerance() const
  {return abs_tol_;}

  ValueT
  getAbsoluteError() const
  {return abs_error_;}

  bool
  isConverged() const
  {return converged_;}

protected:
  Intrepid::Index
  max_num_iter_{128};

  Intrepid::Index
  max_num_trust_region_iter_{4};

  Intrepid::Index
  num_iter_{0};

  ValueT
  max_step_length_{1.0};

  ValueT
  initial_step_length_{1.0};

  ValueT
  min_reduction_{0.25};

  ValueT
  rel_tol_{1.0e-10};

  ValueT
  rel_error_{1.0};

  ValueT
  abs_tol_{1.0e-10};

  ValueT
  abs_error_{1.0};

  bool
  converged_{false};
};

//
// Specializations
//
template<typename EvalT, typename Residual,
Intrepid::Index N = Intrepid::DYNAMIC>
class TrustRegionSolver;

//
// Residual
//
template<typename Residual, Intrepid::Index N>
class TrustRegionSolver<PHAL::AlbanyTraits::Residual, Residual, N> :
    public TrustRegionSolver_Base<PHAL::AlbanyTraits::Residual, Residual, N>
{
public:
  using ScalarT = typename PHAL::AlbanyTraits::Residual::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;

  void
  solve(
      Residual & residual,
      Intrepid::Vector<FadT, N> & x) override;
};

//
// Jacobian
//
template<typename Residual, Intrepid::Index N>
class TrustRegionSolver<PHAL::AlbanyTraits::Jacobian, Residual, N> :
    public TrustRegionSolver_Base<PHAL::AlbanyTraits::Jacobian, Residual, N>
{
public:
  using ScalarT = typename PHAL::AlbanyTraits::Jacobian::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// Tangent
//
template<typename Residual, Intrepid::Index N>
class TrustRegionSolver<PHAL::AlbanyTraits::Tangent, Residual, N> :
    public TrustRegionSolver_Base<PHAL::AlbanyTraits::Tangent, Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::Tangent::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// Distribured Parameter Derivative
//
template<typename Residual, Intrepid::Index N>
class TrustRegionSolver<PHAL::AlbanyTraits::DistParamDeriv, Residual, N> :
    public TrustRegionSolver_Base<PHAL::AlbanyTraits::DistParamDeriv,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::DistParamDeriv::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

#ifdef ALBANY_SG
//
// SGResidual
//
template<typename Residual, Intrepid::Index N>
class TrustRegionSolver<PHAL::AlbanyTraits::SGResidual, Residual, N> :
    public TrustRegionSolver_Base<PHAL::AlbanyTraits::SGResidual,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::SGResidual::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// SGJacobian
//
template<typename Residual, Intrepid::Index N>
class TrustRegionSolver<PHAL::AlbanyTraits::SGJacobian, Residual, N> :
    public TrustRegionSolver_Base<PHAL::AlbanyTraits::SGJacobian,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::SGJacobian::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// SGTangent
//
template<typename Residual, Intrepid::Index N>
class TrustRegionSolver<PHAL::AlbanyTraits::SGTangent, Residual, N> :
    public TrustRegionSolver_Base<PHAL::AlbanyTraits::SGTangent,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::SGTangent::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};
#endif

#ifdef ALBANY_ENSEMBLE
//
// MPResidual
//
template<typename Residual, Intrepid::Index N>
class TrustRegionSolver<PHAL::AlbanyTraits::MPResidual, Residual, N> :
    public TrustRegionSolver_Base<PHAL::AlbanyTraits::MPResidual,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::MPResidual::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// MPJacobian
//
template<typename Residual, Intrepid::Index N>
class TrustRegionSolver<PHAL::AlbanyTraits::MPJacobian, Residual, N> :
    public TrustRegionSolver_Base<PHAL::AlbanyTraits::MPJacobian,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::MPJacobian::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};

//
// MPTangent
//
template<typename Residual, Intrepid::Index N>
class TrustRegionSolver<PHAL::AlbanyTraits::MPTangent, Residual, N> :
    public TrustRegionSolver_Base<PHAL::AlbanyTraits::MPTangent,
    Residual, N>
{
  using ScalarT = typename PHAL::AlbanyTraits::MPTangent::ScalarT;
  using ValueT = typename Sacado::ValueType<ScalarT>::type;
  using FadT = typename Sacado::Fad::DFad<ValueT>;
};
#endif
} //namesapce LCM

#include "MiniNonlinearSolver.t.h"

#endif // Intrepid_NonlinearSolver_h
