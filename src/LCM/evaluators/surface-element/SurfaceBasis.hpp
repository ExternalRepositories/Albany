//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef SURFACE_BASIS_HPP
#define SURFACE_BASIS_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Albany_Layouts.hpp"

#include "Intrepid_CellTools.hpp"
#include "Intrepid_Cubature.hpp"

namespace LCM {

/// \brief Surface Basis Evaluator
///
/// This evaluator computes bases for surface elements
/// \tparam EvalT
/// \tparam Traits
///
template<typename EvalT, typename Traits>
class SurfaceBasis: public PHX::EvaluatorWithBaseImpl<Traits>,
    public PHX::EvaluatorDerived<EvalT, Traits> {

public:
  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;
  typedef Intrepid::FieldContainer<ScalarT> SFC;
  typedef Intrepid::FieldContainer<MeshScalarT> MFC;

  ///
  /// Constructor
  /// \param[in] p Teuchos::ParameterList
  /// \param[in] dl RCP to Albany::Layout
  ///
  SurfaceBasis(const Teuchos::ParameterList& p,
      const Teuchos::RCP<Albany::Layouts>& dl);

  ///
  /// Phalanx method to allocate space
  ///
  void postRegistrationSetup(typename Traits::SetupData d,
      PHX::FieldManager<Traits>& vm);

  ///
  /// Implementation of physics
  ///
  void evaluateFields(typename Traits::EvalData d);

  ///
  /// Takes the reference coordinates and computes the midplane
  /// \param refCoords
  /// \param midplaneCoords
  ///
  void computeReferenceMidplaneCoords(
      const PHX::MDField<MeshScalarT, Cell, Vertex, Dim> refCoords,
      MFC & midplaneCoords);

  ///
  /// Takes the current coordinates and computes the midplane
  /// \param currentCoords
  /// \param midplaneCoords
  ///
  void computeCurrentMidplaneCoords(
      const PHX::MDField<ScalarT, Cell, Vertex, Dim> currentCoords,
      SFC & midplaneCoords);

  ///
  /// Computes Reference configuration Bases from the reference midplane
  /// \param midplaneCoords
  /// \param basis
  ///
  void computeReferenceBaseVectors(const MFC & midplaneCoords,
      PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim, Dim> basis);

  ///
  /// Computes current configuration Bases from the current midplane
  /// \param midplaneCoords
  /// \param basis
  ///
  void computeCurrentBaseVectors(const SFC & midplaneCoords,
      PHX::MDField<ScalarT, Cell, QuadPoint, Dim, Dim> basis);

  ///
  /// Computes the Dual from the midplane and reference bases
  /// \param midplaneCoords
  /// \param basis
  /// \param normal
  /// \param dualBasis
  ///
  void computeDualBaseVectors(const MFC & midplaneCoords,
      const PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim, Dim> basis,
      PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim> normal,
      PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim, Dim> dualBasis);

  ///
  /// Computes the jacobian mapping - da/dA
  /// \param basis
  /// \param dualBasis
  /// \param area
  ///
  void computeJacobian(
      const PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim, Dim> basis,
      const PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim, Dim> dualBasis,
      PHX::MDField<MeshScalarT, Cell, QuadPoint> area);

private:
  unsigned int
  num_dims_, num_nodes_, num_qps_, num_surf_nodes_, num_surf_dims_;

  bool
  need_current_basis_;

  ///
  /// Input: Cordinates in the reference configuration
  ///
  PHX::MDField<MeshScalarT, Cell, Vertex, Dim>
  reference_coords_;

  ///
  /// Input: Numerical integration rule
  ///
  Teuchos::RCP<Intrepid::Cubature<RealType> >
  cubature_;

  ///
  /// Input: Finite element basis for the midplane
  ///
  Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
  intrepid_basis_;

  ///
  /// Local FieldContainer to store the reference midplaneCoords
  ///
  Intrepid::FieldContainer<MeshScalarT>
  ref_midplane_coords_;

  ///
  /// Local FieldContainer to store the current midplaneCoords
  ///
  Intrepid::FieldContainer<ScalarT>
  current_midplane_coords_;

  ///
  /// Output: Reference basis
  ///
  PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim, Dim>
  ref_basis_;

  ///
  /// Output: Reference integration area
  ///
  PHX::MDField<MeshScalarT, Cell, QuadPoint>
  ref_area_;

  ///
  /// Output: Reference dual basis
  ///
  PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim, Dim>
  ref_dual_basis_;

  ///
  /// Output: Reference normal
  ///
  PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim>
  ref_normal_;

  // if we need to compute the current bases (for mechanics)
  ///
  /// Optional Input: Coordinates in the current configuration
  ///
  PHX::MDField<ScalarT, Cell, Vertex, Dim>
  current_coords_;

  ///
  /// Optional Output: Current basis
  ///
  PHX::MDField<ScalarT, Cell, QuadPoint, Dim, Dim>
  current_basis_;

  ///
  /// Reference Cell FieldContainer for basis values
  ///
  Intrepid::FieldContainer<RealType>
  ref_values_;

  ///
  /// Reference Cell FieldContainer for basis gradients
  ///
  Intrepid::FieldContainer<RealType>
  ref_grads_;

  ///
  /// Reference Cell FieldContainer for integration point locations
  ///
  Intrepid::FieldContainer<RealType>
  ref_points_;

  ///
  /// Reference Cell FieldContainer for integration weights
  ///
  Intrepid::FieldContainer<RealType>
  ref_weights_;
};
}

#endif // SURFACE_BASIS_HPP
