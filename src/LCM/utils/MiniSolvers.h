//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#if !defined(LCM_MiniSolvers_h)
#define LCM_MiniSolvers_h

#include "Intrepid2_MiniTensor_Solvers.h"

//
// Define some nonlinear systems (NLS) to test nonlinear solution methods.
//
namespace LCM {

//
//
//
template<typename S>
class BananaNLS : public Intrepid2::Function_Base<BananaNLS<S>, S>
{
public:

  BananaNLS()
  {
  }

  static constexpr
  Intrepid2::Index
  DIMENSION{2};

  static constexpr
  char const * const
  NAME{"Rosenbrock's Banana"};

  // Default value.
  template<typename T, Intrepid2::Index N>
  T
  value(Intrepid2::Vector<T, N> const & x)
  {
    return Intrepid2::Function_Base<BananaNLS<S>, S>::value(*this, x);
  }

  // Explicit gradient.
  template<typename T, Intrepid2::Index N>
  Intrepid2::Vector<T, N>
  gradient(Intrepid2::Vector<T, N> const & x) const
  {
    Intrepid2::Index const
    dimension = x.get_dimension();

    assert(dimension == DIMENSION);

    Intrepid2::Vector<T, N>
    r(dimension);

    r(0) = 2.0 * (x(0) - 1.0) + 400.0 * x(0) * (x(0) * x(0) - x(1));
    r(1) = 200.0 * (x(1) - x(0) * x(0));

    return r;
  }

  // Default AD hessian.
  template<typename T, Intrepid2::Index N>
  Intrepid2::Tensor<T, N>
  hessian(Intrepid2::Vector<T, N> const & x)
  {
    return Intrepid2::Function_Base<BananaNLS<S>, S>::hessian(*this, x);
  }

};

} // namespace LCM

#endif // LCM_MiniSolvers_h
