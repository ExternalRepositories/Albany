//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef LANDICE_STOKES_FO_LATERAL_RESID_HPP
#define LANDICE_STOKES_FO_LATERAL_RESID_HPP 1

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Albany_Layouts.hpp"
#include "Albany_ScalarOrdinalTypes.hpp"
#include "Albany_SacadoTypes.hpp"
#include "PHAL_Dimension.hpp"

namespace LandIce
{

/** \brief The residual of the lateral BC

    This evaluator evaluates the residual of the Lateral bc for the StokesFO problem
*/

template<typename EvalT, typename Traits, typename ThicknessScalarT>
class StokesFOLateralResid : public PHX::EvaluatorWithBaseImpl<Traits>,
                             public PHX::EvaluatorDerived<EvalT, Traits>
{
public:

  typedef typename EvalT::ScalarT       ScalarT;
  typedef typename EvalT::MeshScalarT   MeshScalarT;

  StokesFOLateralResid (const Teuchos::ParameterList& p,
                        const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup (typename Traits::SetupData d,
                              PHX::FieldManager<Traits>& fm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename Albany::StrongestScalarType<ScalarT,MeshScalarT>::type OutputScalarT;

  void evaluate_with_given_immersed_ratio(typename Traits::EvalData d);
  void evaluate_with_computed_immersed_ratio(typename Traits::EvalData d);

  // Input:
  PHX::MDField<const MeshScalarT>        coords_qp;
  PHX::MDField<const ThicknessScalarT>   thickness;
  PHX::MDField<const MeshScalarT>        elevation;
  PHX::MDField<const RealType>           BF;
  PHX::MDField<const MeshScalarT>        normals;
  PHX::MDField<const MeshScalarT>        w_measure;

  // Output:
  PHX::MDField<OutputScalarT,Cell,Node,VecDim>              residual;

  Kokkos::View<int**, PHX::Device> sideNodes;
  std::string                     lateralSideName;

  bool useCollapsedSidesets;

  double rho_w;
  double rho_i;
  double g;
  double given_immersed_ratio;
  double X_0;
  double Y_0;
  double R2;

  bool immerse_ratio_provided;
  bool use_stereographic_map;

  int numSideNodes;
  int numSideQPs;
  int vecDimFO;

  Albany::LocalSideSetInfo sideSet;
  
};

} // Namespace LandIce

#endif // LANDICE_STOKES_FO_LATERAL_RESID_HPP
