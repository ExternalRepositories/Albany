//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef FELIX_VISCOSITY_FO_HPP
#define FELIX_VISCOSITY_FO_HPP

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

template<typename EvalT, typename Traits, typename VelT, typename TemprT>
class ViscosityFO : public PHX::EvaluatorWithBaseImpl<Traits>,
                    public PHX::EvaluatorDerived<EvalT, Traits>,
                    public Sacado::ParameterAccessor<EvalT, SPL_Traits> {

public:
  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;
  typedef typename EvalT::ParamScalarT ParamScalarT;
  

  ViscosityFO(const Teuchos::ParameterList& p,
              const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

  typename EvalT::ScalarT& getValue(const std::string &n);


private:

  typename EvalT::ScalarT dummyParam;
  ScalarT printedH;

  bool extractStrainRateSq;
  bool useStereographicMap;
  Teuchos::ParameterList* stereographicMapList;

  //coefficients for Glen's law
  double A;
  double n;

  // Input:
  PHX::MDField<VelT,Cell,QuadPoint,VecDim,Dim> Ugrad;
  PHX::MDField<VelT,Cell,QuadPoint,VecDim> U;
  PHX::MDField<MeshScalarT,Cell,QuadPoint, Dim> coordVec;
  PHX::MDField<TemprT,Cell> temperature;
  PHX::MDField<TemprT,Cell> flowFactorA;  //this is the coefficient A.  To distinguish it from the scalar flowFactor defined in the body of the function, it is called flowFactorA.  Probably this should be changed at some point...

  // Output:
  PHX::MDField<ScalarT,Cell,QuadPoint> mu;
  PHX::MDField<ScalarT,Cell,QuadPoint> epsilonSq;

  PHX::MDField<ScalarT,Dim> homotopy;
  ScalarT printedFF;

  unsigned int numQPs, numDims, numNodes, numCells;

  enum VISCTYPE {CONSTANT, EXPTRIG, GLENSLAW, GLENSLAW_XZ};
  enum FLOWRATETYPE {UNIFORM, TEMPERATUREBASED, FROMFILE, FROMCISM};
  VISCTYPE visc_type;
  FLOWRATETYPE flowRate_type;

#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
public:
  typedef typename PHX::Device execution_space;
  typedef Kokkos::View<int***, PHX::Device>::execution_space ExecutionSpace;

  struct ViscosityFO_EXPTRIG_Tag{};
  struct ViscosityFO_CONSTANT_Tag{};
  struct ViscosityFO_GLENSLAW_UNIFORM_Tag{};
  struct ViscosityFO_GLENSLAW_TEMPERATUREBASED_Tag{};
  struct ViscosityFO_GLENSLAW_FROMFILE_Tag{};
  struct ViscosityFO_GLENSLAW_XZ_UNIFORM_Tag{};
  struct ViscosityFO_GLENSLAW_XZ_TEMPERATUREBASED_Tag{};
  struct ViscosityFO_GLENSLAW_XZ_FROMFILE_Tag{};
  struct ViscosityFO_GLENSLAW_XZ_FROMCISM_Tag{};

  typedef Kokkos::RangePolicy<ExecutionSpace, ViscosityFO_EXPTRIG_Tag> ViscosityFO_EXPTRIG_Policy;
  typedef Kokkos::RangePolicy<ExecutionSpace, ViscosityFO_CONSTANT_Tag> ViscosityFO_CONSTANT_Policy;
  typedef Kokkos::RangePolicy<ExecutionSpace, ViscosityFO_GLENSLAW_UNIFORM_Tag> ViscosityFO_GLENSLAW_UNIFORM_Policy;
  typedef Kokkos::RangePolicy<ExecutionSpace, ViscosityFO_GLENSLAW_TEMPERATUREBASED_Tag> ViscosityFO_GLENSLAW_TEMPERATUREBASED_Policy;
  typedef Kokkos::RangePolicy<ExecutionSpace, ViscosityFO_GLENSLAW_FROMFILE_Tag> ViscosityFO_GLENSLAW_FROMFILE_Policy;
  typedef Kokkos::RangePolicy<ExecutionSpace, ViscosityFO_GLENSLAW_XZ_UNIFORM_Tag> ViscosityFO_GLENSLAW_XZ_UNIFORM_Policy;
  typedef Kokkos::RangePolicy<ExecutionSpace, ViscosityFO_GLENSLAW_XZ_TEMPERATUREBASED_Tag> ViscosityFO_GLENSLAW_XZ_TEMPERATUREBASED_Policy;
  typedef Kokkos::RangePolicy<ExecutionSpace, ViscosityFO_GLENSLAW_XZ_FROMFILE_Tag> ViscosityFO_GLENSLAW_XZ_FROMFILE_Policy;
  typedef Kokkos::RangePolicy<ExecutionSpace, ViscosityFO_GLENSLAW_XZ_FROMCISM_Tag> ViscosityFO_GLENSLAW_XZ_FROMCISM_Policy;

  KOKKOS_INLINE_FUNCTION
  void operator() (const ViscosityFO_EXPTRIG_Tag& tag, const int& i) const;

  KOKKOS_INLINE_FUNCTION
  void operator() (const ViscosityFO_CONSTANT_Tag& tag, const int& i) const;

  KOKKOS_INLINE_FUNCTION
  void operator() (const ViscosityFO_GLENSLAW_UNIFORM_Tag& tag, const int& i) const;

  KOKKOS_INLINE_FUNCTION
  void operator() (const ViscosityFO_GLENSLAW_TEMPERATUREBASED_Tag& tag, const int& i) const;

  KOKKOS_INLINE_FUNCTION
  void operator() (const ViscosityFO_GLENSLAW_FROMFILE_Tag& tag, const int& i) const;

  KOKKOS_INLINE_FUNCTION
  void operator() (const ViscosityFO_GLENSLAW_XZ_UNIFORM_Tag& tag, const int& i) const;

  KOKKOS_INLINE_FUNCTION
  void operator() (const ViscosityFO_GLENSLAW_XZ_TEMPERATUREBASED_Tag& tag, const int& i) const;

  KOKKOS_INLINE_FUNCTION
  void operator() (const ViscosityFO_GLENSLAW_XZ_FROMFILE_Tag& tag, const int& i) const;

  KOKKOS_INLINE_FUNCTION
  void operator() (const ViscosityFO_GLENSLAW_XZ_FROMCISM_Tag& tag, const int& i) const;

  KOKKOS_INLINE_FUNCTION
  void glenslaw (const TemprT &flowFactorVec, const int& cell) const;

  KOKKOS_INLINE_FUNCTION
  void glenslaw_xz (const TemprT &flowFactorVec, const int& cell) const;

  double R, x_0, y_0, R2;

#endif

};

} // Namespace FELIX

#endif // FELIX_VISCOSITY_FO_HPP
