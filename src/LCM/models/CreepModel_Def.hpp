//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#define DEBUG_FREQ 1000000
#include <Intrepid_MiniTensor.h>
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include "LocalNonlinearSolver.hpp"

namespace LCM
{

//------------------------------------------------------------------------------
template<typename EvalT, typename Traits>
CreepModel<EvalT, Traits>::
CreepModel(Teuchos::ParameterList* p,
    const Teuchos::RCP<Albany::Layouts>& dl) :
    LCM::ConstitutiveModel<EvalT, Traits>(p, dl),
    creep_initial_guess_(p->get<RealType>("Initial Creep Guess", 1.1e-4)),
    // relax_time_scale_(p->get<RealType>("Relaxation Time Scale", 0.0025)),
     
    //sat_mod_(p->get<RealType>("Saturation Modulus", 0.0)),
    //sat_exp_(p->get<RealType>("Saturation Exponent", 0.0)),
  
    // below is what we called C_2 in the functions
    strain_rate_expo_(p->get<RealType>("Strain Rate Exponent", 1.0)),
    // below is what we called A in the functions
    relaxation_para_(p->get<RealType>("Relaxation Parameter of Material_A", 0.1)),
    // below is what we called Q/R in the functions, users can give them values here
    activation_para_(p->get<RealType>("Activation Parameter of Material_Q/R", 500.0))
    
{
  // retrive appropriate field name strings
  std::string cauchy_string = (*field_name_map_)["Cauchy_Stress"];
  std::string Fp_string = (*field_name_map_)["Fp"];
  std::string eqps_string = (*field_name_map_)["eqps"];
  std::string source_string = (*field_name_map_)["Mechanical_Source"];
  std::string F_string = (*field_name_map_)["F"];
  std::string J_string = (*field_name_map_)["J"];

  // define the dependent fields
  this->dep_field_map_.insert(std::make_pair(F_string, dl->qp_tensor));
  this->dep_field_map_.insert(std::make_pair(J_string, dl->qp_scalar));
  this->dep_field_map_.insert(std::make_pair("Poissons Ratio", dl->qp_scalar));
  this->dep_field_map_.insert(std::make_pair("Elastic Modulus", dl->qp_scalar));
  this->dep_field_map_.insert(std::make_pair("Yield Strength", dl->qp_scalar));
  this->dep_field_map_.insert(std::make_pair("Hardening Modulus", dl->qp_scalar));
  this->dep_field_map_.insert(std::make_pair("Delta Time", dl->workset_scalar));

  // define the evaluated fields
  this->eval_field_map_.insert(std::make_pair(cauchy_string, dl->qp_tensor));
  this->eval_field_map_.insert(std::make_pair(Fp_string, dl->qp_tensor));
  this->eval_field_map_.insert(std::make_pair(eqps_string, dl->qp_scalar));
  if (have_temperature_) {
    this->eval_field_map_.insert(std::make_pair(source_string, dl->qp_scalar));
  }

  // define the state variables
  //
  // stress
  this->num_state_variables_++;
  this->state_var_names_.push_back(cauchy_string);
  this->state_var_layouts_.push_back(dl->qp_tensor);
  this->state_var_init_types_.push_back("scalar");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(false);
  this->state_var_output_flags_.push_back(p->get<bool>("Output Cauchy Stress", false));
  //
  // Fp
  this->num_state_variables_++;
  this->state_var_names_.push_back(Fp_string);
  this->state_var_layouts_.push_back(dl->qp_tensor);
  this->state_var_init_types_.push_back("identity");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(true);
  this->state_var_output_flags_.push_back(p->get<bool>("Output Fp", false));
  //
  // eqps
  this->num_state_variables_++;
  this->state_var_names_.push_back(eqps_string);
  this->state_var_layouts_.push_back(dl->qp_scalar);
  this->state_var_init_types_.push_back("scalar");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(true);
  this->state_var_output_flags_.push_back(p->get<bool>("Output eqps", false));
  //
  // mechanical source
  if (have_temperature_) {
    this->num_state_variables_++;
    this->state_var_names_.push_back(source_string);
    this->state_var_layouts_.push_back(dl->qp_scalar);
    this->state_var_init_types_.push_back("scalar");
    this->state_var_init_values_.push_back(0.0);
    this->state_var_old_state_flags_.push_back(false);
    this->state_var_output_flags_.push_back(p->get<bool>("Output Mechanical Source", false));
  }
}
//------------------------------------------------------------------------------
template<typename EvalT, typename Traits>
void CreepModel<EvalT, Traits>::
computeState(typename Traits::EvalData workset,
    std::map<std::string, Teuchos::RCP<PHX::MDField<ScalarT> > > dep_fields,
    std::map<std::string, Teuchos::RCP<PHX::MDField<ScalarT> > > eval_fields)
{
  std::string cauchy_string = (*field_name_map_)["Cauchy_Stress"];
  std::string Fp_string     = (*field_name_map_)["Fp"];
  std::string eqps_string   = (*field_name_map_)["eqps"];
  std::string source_string = (*field_name_map_)["Mechanical_Source"];
  std::string F_string      = (*field_name_map_)["F"];
  std::string J_string      = (*field_name_map_)["J"];

  // extract dependent MDFields
  PHX::MDField<ScalarT> def_grad         = *dep_fields[F_string];
  PHX::MDField<ScalarT> J                = *dep_fields[J_string];
  PHX::MDField<ScalarT> poissons_ratio   = *dep_fields["Poissons Ratio"];
  PHX::MDField<ScalarT> elastic_modulus  = *dep_fields["Elastic Modulus"];
  PHX::MDField<ScalarT> yieldStrength    = *dep_fields["Yield Strength"];
  PHX::MDField<ScalarT> hardeningModulus = *dep_fields["Hardening Modulus"];
  PHX::MDField<ScalarT> delta_time       = *dep_fields["Delta Time"];

  // extract evaluated MDFields
  PHX::MDField<ScalarT> stress = *eval_fields[cauchy_string];
  PHX::MDField<ScalarT> Fp     = *eval_fields[Fp_string];
  PHX::MDField<ScalarT> eqps   = *eval_fields[eqps_string];
  PHX::MDField<ScalarT> source;
  if (have_temperature_) {
    source = *eval_fields[source_string];
  }

  // get State Variables
  Albany::MDArray Fpold = (*workset.stateArrayPtr)[Fp_string + "_old"];
  Albany::MDArray eqpsold = (*workset.stateArrayPtr)[eqps_string + "_old"];

  ScalarT kappa, mu, mubar, K, Y;
  // new parameters introduced here for being the temperature dependent, they are the last two listed below
  ScalarT Jm23, p, dgam, dgam_plastic, a0, a1, f, smag, smag_new, temp_adj_relaxation_para_;
  ScalarT sq23(std::sqrt(2. / 3.));

  Intrepid::Tensor<ScalarT> F(num_dims_), be(num_dims_), s(num_dims_), sigma(
      num_dims_);
  Intrepid::Tensor<ScalarT> N(num_dims_), A(num_dims_), expA(num_dims_), Fpnew(
      num_dims_);
  Intrepid::Tensor<ScalarT> I(Intrepid::eye<ScalarT>(num_dims_));
  Intrepid::Tensor<ScalarT> Fpn(num_dims_), Fpinv(num_dims_), Cpinv(num_dims_);

  long int debug_output_counter = 0;
  //std::cout << "Entering CreepModel_Def code" << std::endl;


  //check delta_time(0)
  //if (delta_time(0) == 0){
    
  //  std::cout << "delta_time(0) == 0, do J2" << std::endl;

  //if(debug_output_counter%DEBUG_FREQ == 0) std::cout << "delta_time is not 0" << std::endl;  
  for (int cell(0); cell < workset.numCells; ++cell) {
    for (int pt(0); pt < num_pts_; ++pt) {
      debug_output_counter++;
      kappa = elastic_modulus(cell, pt)
          / (3. * (1. - 2. * poissons_ratio(cell, pt)));
      mu = elastic_modulus(cell, pt) / (2. * (1. + poissons_ratio(cell, pt)));
      K = hardeningModulus(cell, pt);
      Y = yieldStrength(cell, pt);
      Jm23 = std::pow(J(cell, pt), -2. / 3.);
      
      
      // ----------------------------  temperature dependent coefficient ------------------------
      
      // the effective 'B' we had before in the previous models, with mu
      if(have_temperature_) {
	temp_adj_relaxation_para_ = relaxation_para_ *  std::exp( - activation_para_ / temperature_(cell,pt)) ;
      }else {
	temp_adj_relaxation_para_ = relaxation_para_ *  std::exp( - activation_para_ / 303.0);
      }

      
       
       if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"B = "<<temp_adj_relaxation_para_<<std::endl;


      // fill local tensors
      F.fill(def_grad, cell, pt, 0, 0);
      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {
          Fpn(i, j) = ScalarT(Fpold(cell, pt, i, j));
        }
      }

      if(debug_output_counter%DEBUG_FREQ == 0) std::cout<<"F = "<<F<<std::endl;
      if(debug_output_counter%DEBUG_FREQ == 0) std::cout<<"Fp = "<<Fpn<<std::endl;

      // compute trial state
      Fpinv = Intrepid::inverse(Fpn);
      Cpinv = Fpinv * Intrepid::transpose(Fpinv);
      be = Jm23 * F * Cpinv * Intrepid::transpose(F);

      a0 = Intrepid::norm(Intrepid::dev(be));
      a1 = Intrepid::trace(be); 
      
      if(debug_output_counter%DEBUG_FREQ == 0) std::cout<<"a0 = "<<a0<<std::endl;
      if(debug_output_counter%DEBUG_FREQ == 0) std::cout<<"a1 = "<<a1<<std::endl;


      s = mu * Intrepid::dev(be);
      if(debug_output_counter%DEBUG_FREQ == 0) std::cout<<"s_trial = "<<s<<std::endl;
      mubar = Intrepid::trace(be) * mu / (num_dims_);
      if(debug_output_counter%DEBUG_FREQ == 0) std::cout<<"mubar = "<<mubar<<std::endl;

      smag = Intrepid::norm(s);
      if(debug_output_counter%DEBUG_FREQ == 0) std::cout<<"smag = "<<smag<<std::endl;
      
      f = smag - sq23 * (Y + K * eqpsold(cell, pt));
      if(debug_output_counter%DEBUG_FREQ == 0) std::cout<<"f(yield condition)= "<<f<<std::endl;

      // check yield condition
      if (f <= 0.0) {
        if (a0 > 1E-12){
        // return mapping algorithm
        bool converged = false;
        ScalarT alpha = 0.0;
        ScalarT res = 0.0;
        int count = 0;
        // ScalarT H = 0.0;
        dgam = 0.0;

        LocalNonlinearSolver<EvalT, Traits> solver;

        std::vector<ScalarT> F(1);
        std::vector<ScalarT> dFdX(1);
        std::vector<ScalarT> X(1);

        X[0] = smag;
        
        while (!converged && count <= 30)
        {
         
         F[0] = X[0] - smag + 2. * mubar * delta_time(0) * temp_adj_relaxation_para_ * std::pow(X[0], strain_rate_expo_) ;

         dFdX[0] = 1. + strain_rate_expo_ * 2 * mubar * delta_time(0) * temp_adj_relaxation_para_ * std::pow(X[0], strain_rate_expo_ - 1);

          solver.solve(dFdX, X, F);
          count++;

          if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"Creep Solver count = "<<count<<std::endl;
          if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"X[0] = "<<X[0]<<std::endl;
          if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"F[0] = "<<F[0]<<std::endl;
          if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"dFdX[0] = "<<dFdX[0]<<std::endl;
          if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"***finish return mapping***"<<std::endl;

          res = std::abs(F[0]);
          if (res < 1.e-10 )
            converged = true;

          TEUCHOS_TEST_FOR_EXCEPTION(count == 30, std::runtime_error,
              std::endl <<
              "Error in return mapping, count = " <<
              count <<
              "\nres = " << res <<
              "\ng = " << F[0] <<
              "\ndg = " << dFdX[0] <<
              "\nalpha = " << alpha << std::endl);

        }

        dgam = delta_time(0) * temp_adj_relaxation_para_ * std::pow(X[0], strain_rate_expo_ );

        // plastic direction
        N =  s / Intrepid::norm(s);

        // update s
        // s -= 2 * mubar * dgam * N;
        s = X[0]*N;

        if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"s = "<<s<<std::endl;


        // mechanical source
        if (have_temperature_ && delta_time(0) > 0) {
          source(cell, pt) = 0.0 * (sq23 * dgam / delta_time(0)
            * (Y + temperature_(cell,pt))) / (density_ * heat_capacity_);
        }

        // exponential map to get Fpnew
        A = dgam * N;
        expA = Intrepid::exp(A);
        Fpnew = expA * Fpn;
        for (int i(0); i < num_dims_; ++i) {
          for (int j(0); j < num_dims_; ++j) {
            Fp(cell, pt, i, j) = Fpnew(i, j);
          }
        }
      } else {
        
        if(debug_output_counter%DEBUG_FREQ == 0) std::cout << "hit alternate condition in creep" << std::endl;
        eqps(cell, pt) = eqpsold(cell, pt);
        if (have_temperature_) source(cell, pt) = 0.0;
        for (int i(0); i < num_dims_; ++i) {
          for (int j(0); j < num_dims_; ++j) {
            Fp(cell, pt, i, j) = Fpn(i, j);
          }
        }
       }
      } else {
        if(debug_output_counter%DEBUG_FREQ == 0) std::cout << " beyond the yield condition here, should do combination now" << std::endl;
  
        bool converged = false;
        ScalarT H = 0.0;
        ScalarT dH = 0.0;
        ScalarT alpha = 0.0;
        ScalarT res = 0.0;
        int count = 0;
        dgam = 0.0;
        smag_new = 0.0;
        dgam_plastic = 0.0;

        LocalNonlinearSolver<EvalT, Traits> solver;

        std::vector<ScalarT> F(1);
        std::vector<ScalarT> dFdX(1);
        std::vector<ScalarT> X(1);

	X[0] = smag;
       
        // X[0] = creep_initial_guess_;

	if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"Creep/Plasticity Solver initial X[0] = "<<X[0]<<std::endl;
        
        while (!converged && count < 30)
        {

          //alpha = eqpsold(cell, pt) + sq23 * X[0];
          //if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"eqps_old = "<<eqpsold(cell, pt)<<std::endl;
         
          //H = std::pow( (f - (2. * mubar + 2./3. * K)*X[0]) , 1./strain_rate_expo_ );

          H = (K/( K + 3* mubar))* 2* mubar * delta_time(0) * temp_adj_relaxation_para_ * std::pow(X[0], strain_rate_expo_ );          

          //if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"H = "<<H<<std::endl;

          //dH =  (1./strain_rate_expo_ )
          // * std::pow( (f - (2. * mubar + 2./3. * K)*X[0]), 1./strain_rate_expo_ - 1.0)
          // * (-2. * mubar - 2./3. * K);
    
          dH = strain_rate_expo_ * (K/( K + 3* mubar))* 2* mubar * delta_time(0) * temp_adj_relaxation_para_ * std::pow(X[0], strain_rate_expo_ -1 );
         
          //F[0] = (std::pow( 2. * mubar * delta_time(0) * temp_adj_relaxation_para_, 1./strain_rate_expo_ )*( f - smag - 2./3. * K * X[0] ) + H);
          //dFdX[0] = (std::pow( 2. * mubar * delta_time(0) * temp_adj_relaxation_para_, 1./strain_rate_expo_ ) * (- 2./3. * K) + dH);

          F[0] = X[0] - smag + H + (3*mubar/( K + 3* mubar))*f;
          dFdX[0] = 1 + dH;


          solver.solve(dFdX, X, F);
          count++;

	  if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"Creep/Plasticity Solver count = "<<count<<std::endl;
          if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"X[0] = "<<X[0]<<std::endl;
          if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"F[0] = "<<F[0]<<std::endl;
          if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"dFdX[0] = "<<dFdX[0]<<std::endl;
          if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"H = "<<H<<std::endl;
          if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"dH = "<<dH<<std::endl; 
          if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"***finish return mapping***"<<std::endl;

          res = std::abs(F[0]);
          if (res < 1.e-08 || res / f < 1.E-11)
            converged = true;

          TEUCHOS_TEST_FOR_EXCEPTION(count > 30, std::runtime_error,
              std::endl <<
              "Error in return mapping, count = " <<
              count <<
              "\nres = " << res <<
              "\nrelres = " << res/f <<
              "\ng = " << F[0] <<
              "\ndg = " << dFdX[0] <<
              "\nalpha = " << alpha << std::endl);
        }
        smag_new = X[0];

        // update dgam
        // dgam = delta_time(0) * temp_adj_relaxation_para_ * std::pow(smag_new, strain_rate_expo_ ) + (1./(2 * mubar + 2*K/3)) * (f - 2 * mubar * delta_time(0) * temp_adj_relaxation_para_ * std::pow(smag_new, strain_rate_expo_ ));
        
        dgam = ( smag_new - smag ) / (-2 * mubar);   
        if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"dgam = "<<dgam<<std::endl; 

        dgam_plastic = (1./(2 * mubar + 2*K/3)) * (f - 2 * mubar * delta_time(0) * temp_adj_relaxation_para_ * std::pow(smag_new, strain_rate_expo_ )) ;
        if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"dgam_plastic = "<<dgam_plastic<<std::endl;      


        alpha = eqpsold(cell, pt) + sq23 * dgam_plastic ;
        
        // plastic direction
        N =  s / Intrepid::norm(s);

        // update s
        
        //s = std::pow( 1./(temp_adj_relaxation_para_ * 2. * mubar * delta_time(0)), 1./strain_rate_expo_ ) 
        //  * std::pow( (f - (2. * mubar + 2./3. * K)*X[0]), 1./strain_rate_expo_ ) * N;
     
        s = smag_new * N;
        if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"s_tensor = "<<s<<std::endl;

        // update eqps
        eqps(cell, pt) = alpha;
        if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"eqps_updated = "<<eqps(cell, pt)<<std::endl;

        // mechanical source
        if (have_temperature_ && delta_time(0) > 0) {
          source(cell, pt) = 0.0 * (sq23 * dgam / delta_time(0)
            * (Y + H + temperature_(cell,pt))) / (density_ * heat_capacity_);
        }

        // exponential map to get Fpnew
        A = dgam * N;
        expA = Intrepid::exp(A);
        Fpnew = expA * Fpn;
        for (int i(0); i < num_dims_; ++i) {
          for (int j(0); j < num_dims_; ++j) {
            Fp(cell, pt, i, j) = Fpnew(i, j);
          }
        }
      }
      
      // compute pressure
      p = 0.5 * kappa * (J(cell, pt) - 1. / (J(cell, pt)));

      // compute stress
      sigma = p * I + s / J(cell, pt);
      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {
          stress(cell, pt, i, j) = sigma(i, j);
        }
      }

      if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"sigma(combine) = "<<sigma<<std::endl;

    }
  }



 if (have_temperature_) {
    for (int cell(0); cell < workset.numCells; ++cell) {
      for (int pt(0); pt < num_pts_; ++pt) {
        F.fill(def_grad,cell,pt,0,0);
        ScalarT J = Intrepid::det(F);
        sigma.fill(stress,cell,pt,0,0);
        sigma -= 3.0 * expansion_coeff_ * (1.0 + 1.0 / (J*J))
          * (temperature_(cell,pt) - ref_temperature_) * I;
        for (int i = 0; i < num_dims_; ++i) {
          for (int j = 0; j < num_dims_; ++j) {
            stress(cell, pt, i, j) = sigma(i, j);
          }
        }
      }
    }
  }

}
//------------------------------------------------------------------------------
}
