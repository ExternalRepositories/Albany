//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef AERAS_DOF_INTERPOLATION_HPP
#define AERAS_DOF_INTERPOLATION_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Aeras_Layouts.hpp"

namespace Aeras {
/** \brief Finite Element Interpolation Evaluator

    This evaluator interpolates nodal DOF values to quad points.

*/

template<typename EvalT, typename Traits>
class DOFInterpolation : public PHX::EvaluatorWithBaseImpl<Traits>,
 			 public PHX::EvaluatorDerived<EvalT, Traits>  {

public:
  typedef typename EvalT::ScalarT ScalarT;

  DOFInterpolation(Teuchos::ParameterList& p,
                   const Teuchos::RCP<Aeras::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
                             PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:
  // Input:
  //! Values at nodes
  PHX::MDField<ScalarT> val_node;
  //! Basis Functions
  PHX::MDField<RealType,Cell,Node,QuadPoint> BF;

  // Output:
  //! Values at quadrature points
  PHX::MDField<ScalarT> val_qp;

  const int numNodes;
  const int numQPs;
  const int numLevels;
  const int numRank;

#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
public:
  typedef Kokkos::View<int***, PHX::Device>::execution_space ExecutionSpace;
  using Iterate = Kokkos::Experimental::Iterate;
#if defined(PHX_KOKKOS_DEVICE_TYPE_CUDA)
  static constexpr Iterate IterateDirection = Iterate::Left;
#else
  static constexpr Iterate IterateDirection = Iterate::Right;
#endif

  struct DOFInterpolation_numRank2_Tag{};
  struct DOFInterpolation_Tag{};

  using DOFInterpolation_Policy = Kokkos::Experimental::MDRangePolicy<
        Kokkos::Experimental::Rank<3, IterateDirection, IterateDirection>,
        Kokkos::IndexType<int>>;
  using DOFInterpolation_rank2_Policy = Kokkos::Experimental::MDRangePolicy<
        Kokkos::Experimental::Rank<2, IterateDirection, IterateDirection>,
        Kokkos::IndexType<int>>;

#if defined(PHX_KOKKOS_DEVICE_TYPE_CUDA)
  typename DOFInterpolation_Policy::tile_type 
    DOFInterpolation_TileSize{};
  typename DOFInterpolation_rank2_Policy::tile_type 
    DOFInterpolation_rank2_TileSize{};
#else
  typename DOFInterpolation_Policy::tile_type 
    DOFInterpolation_TileSize{};
  typename DOFInterpolation_rank2_Policy::tile_type 
    DOFInterpolation_rank2_TileSize{};
#endif

  KOKKOS_INLINE_FUNCTION
  void operator() (const int cell, const int qp, const int level) const;

  KOKKOS_INLINE_FUNCTION
  void operator() (const int cell, const int level) const;

#endif
};
}

#endif
