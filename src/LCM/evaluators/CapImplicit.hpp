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
 * ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This notice    *
 * including this sentence must appear on any copies of this software.*
 *    Questions to Andy Salinger, agsalin@sandia.gov                  *
 \********************************************************************/

#ifndef CAPIMPLICIT_HPP
#define CAPIMPLICIT_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Tensor.h"
#include "Sacado.hpp"

namespace LCM {
  /** \brief CapImplicit stress response

   This evaluator computes stress based on a cap plasticity model.

   */

  template<typename EvalT, typename Traits>
  class CapImplicit: public PHX::EvaluatorWithBaseImpl<Traits>,
      public PHX::EvaluatorDerived<EvalT, Traits> {

  public:

    CapImplicit(const Teuchos::ParameterList& p);

    void postRegistrationSetup(typename Traits::SetupData d,
        PHX::FieldManager<Traits>& vm);

    void evaluateFields(typename Traits::EvalData d);

  private:

    typedef typename EvalT::ScalarT ScalarT;
    typedef typename EvalT::MeshScalarT MeshScalarT;
    typedef typename Sacado::Fad::DFad<ScalarT> DFadType;
    typedef typename Sacado::Fad::DFad<DFadType> D2FadType;

    // all local functions used in computing cap model stress:
    ScalarT
    compute_f(LCM::Tensor<ScalarT, 3> & sigma, LCM::Tensor<ScalarT, 3> & alpha,
        ScalarT & kappa);

    std::vector<ScalarT>
    initialize(LCM::Tensor<ScalarT, 3> & sigmaVal,
        LCM::Tensor<ScalarT, 3> & alphaVal, ScalarT & kappaVal,
        ScalarT & dgammaVal);

    void
    compute_ResidJacobian(std::vector<ScalarT> const & XXVal,
        std::vector<ScalarT> & R, std::vector<ScalarT> & dRdX,
        const LCM::Tensor<ScalarT, 3> & sigmaVal,
        const LCM::Tensor<ScalarT, 3> & alphaVal, const ScalarT & kappaVal,
        LCM::Tensor4<ScalarT, 3> const & Celastic, bool kappa_flag);

    DFadType
    compute_f(LCM::Tensor<DFadType, 3> & sigma,
        LCM::Tensor<DFadType, 3> & alpha, DFadType & kappa);

    D2FadType
    compute_g(LCM::Tensor<D2FadType, 3> & sigma,
        LCM::Tensor<D2FadType, 3> & alpha, D2FadType & kappa);

    LCM::Tensor<DFadType, 3>
    compute_dgdsigma(std::vector<DFadType> const & XX);

    DFadType
    compute_Galpha(DFadType J2_alpha);

    LCM::Tensor<DFadType, 3>
    compute_halpha(LCM::Tensor<DFadType, 3> const & dgdsigma,
        DFadType const J2_alpha);

    DFadType
    compute_dedkappa(DFadType const kappa);

    DFadType
    compute_hkappa(DFadType const I1_dgdsigma, DFadType const dedkappa);

    //Input
    PHX::MDField<ScalarT, Cell, QuadPoint, Dim, Dim> strain;
    PHX::MDField<ScalarT, Cell, QuadPoint> elasticModulus;
    PHX::MDField<ScalarT, Cell, QuadPoint> poissonsRatio;

    unsigned int numQPs;
    unsigned int numDims;

    RealType A;
    RealType B;
    RealType C;
    RealType theta;
    RealType R;
    RealType kappa0;
    RealType W;
    RealType D1;
    RealType D2;
    RealType calpha;
    RealType psi;
    RealType N;
    RealType L;
    RealType phi;
    RealType Q;

    std::string strainName, stressName;
    std::string backStressName, capParameterName;

    //output
    PHX::MDField<ScalarT, Cell, QuadPoint, Dim, Dim> stress;
    PHX::MDField<ScalarT, Cell, QuadPoint, Dim, Dim> backStress;
    PHX::MDField<ScalarT, Cell, QuadPoint> capParameter;

  };
}

#endif

