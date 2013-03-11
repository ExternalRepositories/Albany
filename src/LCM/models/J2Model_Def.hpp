//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <Intrepid_MiniTensor.h>
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include "LocalNonlinearSolver.h"

namespace LCM {

  //----------------------------------------------------------------------------
  template<typename EvalT, typename Traits>
  J2Model<EvalT, Traits>::
  J2Model(const Teuchos::ParameterList* p,
          const Teuchos::RCP<Albany::Layouts>& dl):
    LCM::ConstitutiveModel<EvalT,Traits>(p,dl)
  {
    // extract number of integration points and dimensions
    std::vector<PHX::DataLayout::size_type> dims;
    dl->qp_tensor->dimensions(dims);
    num_pts_  = dims[1];
    num_dims_ = dims[2];

    // define the dependent fields
    this->dep_field_map_.insert( std::make_pair("F", dl->qp_tensor) );
    this->dep_field_map_.insert( std::make_pair("J", dl->qp_scalar) );
    this->dep_field_map_.insert( std::make_pair("Poissons Ratio", dl->qp_scalar) );
    this->dep_field_map_.insert( std::make_pair("Elastic Modulus", dl->qp_scalar) );
    this->dep_field_map_.insert( std::make_pair("Yield Strength", dl->qp_scalar) );
    this->dep_field_map_.insert( std::make_pair("Hardening Modulus", dl->qp_scalar) );
    this->dep_field_map_.insert( std::make_pair("Saturation Modulus", dl->qp_scalar) );
    this->dep_field_map_.insert( std::make_pair("Saturation Exponent", dl->qp_scalar) );

    // define the evaluated fields
    this->eval_field_map_.insert( std::make_pair("Cauchy_Stress", dl->qp_tensor) );
    this->eval_field_map_.insert( std::make_pair("Fp", dl->qp_tensor) );
    this->eval_field_map_.insert( std::make_pair("eqps", dl->qp_scalar) );

    // define the state variables
    //
    // stress
    this->num_state_variables_++;
    this->state_var_names_.push_back("Cauchy_Stress");
    this->state_var_layouts_.push_back(dl->qp_tensor);
    this->state_var_init_types_.push_back("scalar");
    this->state_var_init_values_.push_back(0.0);
    this->state_var_old_state_flags_.push_back(false);
    this->state_var_output_flags_.push_back(true);
    //
    // Fp
    this->num_state_variables_++;
    this->state_var_names_.push_back("Fp");
    this->state_var_layouts_.push_back(dl->qp_tensor);
    this->state_var_init_types_.push_back("identity");
    this->state_var_init_values_.push_back(0.0);
    this->state_var_old_state_flags_.push_back(true);
    this->state_var_output_flags_.push_back(false);
    //
    // stress
    this->num_state_variables_++;
    this->state_var_names_.push_back("eqps");
    this->state_var_layouts_.push_back(dl->qp_scalar);
    this->state_var_init_types_.push_back("scalar");
    this->state_var_init_values_.push_back(0.0);
    this->state_var_old_state_flags_.push_back(true);
    this->state_var_output_flags_.push_back(true);
  }
  //----------------------------------------------------------------------------
  template<typename EvalT, typename Traits>
  void J2Model<EvalT, Traits>::
  computeEnergy(typename Traits::EvalData workset,
                std::vector<Teuchos::RCP<PHX::MDField<ScalarT> > > dep_fields,
                std::vector<Teuchos::RCP<PHX::MDField<ScalarT> > > eval_fields)
  {
    // not implemented
  }
  //----------------------------------------------------------------------------
  template<typename EvalT, typename Traits>
  void J2Model<EvalT, Traits>::
  computeState(typename Traits::EvalData workset,
               std::vector<Teuchos::RCP<PHX::MDField<ScalarT> > > dep_fields,
               std::vector<Teuchos::RCP<PHX::MDField<ScalarT> > > eval_fields)
  {
    std::cout << "In J2Model::computeState" << std::endl;
    // extract dependent MDFields
    PHX::MDField<ScalarT> defGrad          = *dep_fields[0];
    PHX::MDField<ScalarT> J                = *dep_fields[1];
    PHX::MDField<ScalarT> poissonsRatio    = *dep_fields[2];
    PHX::MDField<ScalarT> elasticModulus   = *dep_fields[3];
    PHX::MDField<ScalarT> yieldStrength    = *dep_fields[4];
    PHX::MDField<ScalarT> hardeningModulus = *dep_fields[5];
    PHX::MDField<ScalarT> satMod           = *dep_fields[6];
    PHX::MDField<ScalarT> satExp           = *dep_fields[7];
    // extract evaluated MDFields
    PHX::MDField<ScalarT> stress = *eval_fields[0];
    PHX::MDField<ScalarT> Fp     = *eval_fields[1];
    PHX::MDField<ScalarT> eqps   = *eval_fields[2];

    // get State Variables
    Albany::MDArray Fpold   = 
      (*workset.stateArrayPtr)[this->state_var_names_[1]+"_old"];
    Albany::MDArray eqpsold = 
      (*workset.stateArrayPtr)[this->state_var_names_[2]+"_old"];

    ScalarT kappa, mu, mubar, K, Y, siginf, delta;
    ScalarT Jm23, trace, smag2, smag, f, p, dgam;
    ScalarT sq23(std::sqrt(2./3.));

    Intrepid::Tensor<ScalarT> F(num_dims_), be(num_dims_), s(num_dims_), sigma(num_dims_);
    Intrepid::Tensor<ScalarT> N(num_dims_), A(num_dims_), expA(num_dims_), Fpnew(num_dims_);
    Intrepid::Tensor<ScalarT> I(Intrepid::eye<ScalarT>(num_dims_));
    Intrepid::Tensor<ScalarT> Fpn(num_dims_), Fpinv(num_dims_), Cpinv(num_dims_);

    for (std::size_t cell(0); cell < workset.numCells; ++cell) {
      for (std::size_t pt(0); pt < num_pts_; ++pt) {
        kappa =  elasticModulus(cell,pt)/(3.*(1.-2.*poissonsRatio(cell,pt)));
        mu = elasticModulus(cell,pt)/(2.*(1.+poissonsRatio(cell,pt)));
        K = hardeningModulus(cell,pt);
        Y = yieldStrength(cell,pt);
        siginf = satMod(cell,pt);
        delta = satExp(cell,pt);
        Jm23 = std::pow(J(cell,pt),-2./3.);

        // fill local tensors
        F.fill( &defGrad(cell,pt,0,0) );
        //Fpn.fill( &Fpold(cell,pt,std::size_t(0),std::size_t(0)) );
        for ( std::size_t i(0); i < num_dims_; ++i) {
          for ( std::size_t j(0); j < num_dims_; ++j) {
            Fpn(i,j) = ScalarT(Fpold(cell,pt,i,j));
          }                    
        }
        
        // compute trial state
        Fpinv = Intrepid::inverse(Fpn);
        Cpinv = Fpinv * Intrepid::transpose(Fpinv);
        be = Jm23 * F * Cpinv * Intrepid::transpose(F);
        s = mu * Intrepid::dev(be);

        // check yield condition
        smag = Intrepid::norm(s);
        f = smag - sq23*(Y + K*eqpsold(cell,pt) 
                         + siginf*(1.-std::exp(-delta*eqpsold(cell,pt))));

        if (f > 1E-12) {
          // return mapping algorithm
          bool converged = false;
          ScalarT g = f;
          ScalarT H = 0.0;
          ScalarT dH = 0.0;
          ScalarT alpha = 0.0;
          ScalarT res = 0.0;
          int count = 0;
          dgam = 0.0;

          LocalNonlinearSolver<EvalT, Traits> solver;

          std::vector<ScalarT> F(1);
          std::vector<ScalarT> dFdX(1);
          std::vector<ScalarT> X(1);

          F[0] = f;
          X[0] = 0.0;
          dFdX[0] = ( -2. * mubar ) * ( 1. + H / ( 3. * mubar ) );
          while (!converged && count < 30)
          {
            count++;
            solver.solve(dFdX,X,F);
            alpha = eqpsold(cell, pt) + sq23 * X[0];
            H = K * alpha + siginf*( 1. - exp( -delta * alpha ) );
            dH = K + delta * siginf * exp( -delta * alpha );
            F[0] = smag -  ( 2. * mubar * X[0] + sq23 * ( Y + H ) );
            dFdX[0] = -2. * mubar * ( 1. + dH / ( 3. * mubar ) );

            res = std::abs(F[0]);
            if ( res < 1.e-11 || res/f < 1.E-11 )
              converged = true;

            TEUCHOS_TEST_FOR_EXCEPTION( count > 30, std::runtime_error,
                                        std::endl << 
                                        "Error in return mapping, count = " <<
                                        count <<
                                        "\nres = " << res <<
                                        "\nrelres = " << res/f <<
                                        "\ng = " << F[0] <<
                                        "\ndg = " << dFdX[0] <<
                                        "\nalpha = " << alpha << std::endl);
          }
          solver.computeFadInfo(dFdX,X,F);
          dgam = X[0];

          // plastic direction
          N = (1/smag) * s;

          // update s
          s -= 2*mubar*dgam*N;

          // update eqps
          eqps(cell,pt) = alpha;

          // exponential map to get Fpnew
          A = dgam*N;
          expA = Intrepid::exp(A);
          Fpnew = expA * Fpn;
          for (std::size_t i(0); i < num_dims_; ++i) {
            for (std::size_t j(0); j < num_dims_; ++j) {
              Fp(cell,pt,i,j) = Fpnew(i,j);
            }
          }
        } else {
          eqps(cell, pt) = eqpsold(cell,pt);
          for (std::size_t i(0); i < num_dims_; ++i) {
            for (std::size_t j(0); j < num_dims_; ++j) {
              Fp(cell,pt,i,j) = Fpn(i,j);
            }
          }
        }

        // compute pressure
        p = 0.5*kappa*(J(cell,pt)-1./(J(cell,pt)));

        // compute stress
        sigma = p*I + s/J(cell,pt);
        for (std::size_t i(0); i < num_dims_; ++i) {
          for (std::size_t j(0); j < num_dims_; ++j) {
            stress(cell,pt,i,j) = sigma(i,j);
          }
        }
      }
    }
  }
  //----------------------------------------------------------------------------
  template<typename EvalT, typename Traits>
  void J2Model<EvalT, Traits>::
  computeTangent(typename Traits::EvalData workset,
                 std::vector<Teuchos::RCP<PHX::MDField<ScalarT> > > dep_fields,
                 std::vector<Teuchos::RCP<PHX::MDField<ScalarT> > > eval_fields)
  {
    // not implemented
  }
  //----------------------------------------------------------------------------
} 

