//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#define DEBUG_FREQ 10000000
#include <Intrepid_MiniTensor.h>
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include "LocalNonlinearSolver.hpp"

namespace LCM
{

static void aprintd(double x)
{
  if (-1e-5 < x && x < 0)
    x = 0;
  fprintf(stderr, "%.5f", x);
}

static void aprints(double x)
{
  aprintd(x);
  fprintf(stderr,"\n");
}

static void aprints(FadType const& x)
{
  aprintd(x.val());
  fprintf(stderr," [");
  for (int i = 0; i < x.size(); ++i) {
    fprintf(stderr," ");
    aprintd(x.dx(i));
  }
  fprintf(stderr,"]\n");
}

static void stripDeriv(double& x)
{
  (void)x;
}

static void stripDeriv(FadType& x)
{
  x.resize(0);
}

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
  static int times_called = 0;
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

  if (typeid(ScalarT) == typeid(double))
    std::cerr << "Model double times_called " << times_called << '\n';
  else
    std::cerr << "Model FAD times_called " << times_called << '\n';

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
      temp_adj_relaxation_para_ = relaxation_para_ *  std::exp( - activation_para_ / 303.0);

      // fill local tensors
      F.fill(def_grad, cell, pt, 0, 0);

      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {
          Fpn(i, j) = ScalarT(Fpold(cell, pt, i, j));
        }
      }

      // compute trial state
      Fpinv = Intrepid::inverse(Fpn);
      Cpinv = Fpinv * Intrepid::transpose(Fpinv);
      be = Jm23 * F * Cpinv * Intrepid::transpose(F);

      a0 = Intrepid::norm(Intrepid::dev(be));
      a1 = Intrepid::trace(be);

      s = mu * Intrepid::dev(be);

      mubar = Intrepid::trace(be) * mu / (num_dims_);

      smag = Intrepid::norm(s);

      f = smag - sq23 * (Y + K * eqpsold(cell, pt));

      bool doit = (typeid(ScalarT) == typeid(FadType) && times_called == 5000
          && cell == 0 && pt == 0);
      // check yield condition
      if (f <= 0.0) {
        if (doit)
          std::cerr << "f <= 0.0\n";
        if (a0 > 1E-12) {
          if (doit)
            std::cerr << "a0 > 1E-12\n";
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
            //count++;
            F[0] = X[0] - smag + 2. * mubar * delta_time(0) * temp_adj_relaxation_para_ * std::pow(X[0], strain_rate_expo_) ;

            dFdX[0] = 1. + strain_rate_expo_ * 2. * mubar * delta_time(0) * temp_adj_relaxation_para_ * std::pow(X[0], strain_rate_expo_ - 1.);

            //solver.solve(dFdX, X, F);
            solver.solve(dFdX, X, F);
            count++;

            if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"Creep Solver count = "<<count<<std::endl;
            if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"X[0] = "<<X[0]<<std::endl;
            if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"F[0] = "<<F[0]<<std::endl;
            if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"dFdX[0] = "<<dFdX[0]<<std::endl;
        //    if(debug_output_counter%DEBUG_FREQ == 0)std::cout<<"***finish return mapping***"<<std::endl;
   

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
          //solver.computeFadInfo(dFdX, X, F);
          dgam = delta_time(0) * temp_adj_relaxation_para_ * std::pow(X[0], strain_rate_expo_ );

          // plastic direction
          N =  s / Intrepid::norm(s);

          // update s
          // s -= 2 * mubar * dgam * N;
          s = X[0]*N;

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
          if (doit)
            std::cerr << "a0 <= 1E-12\n";
          eqps(cell, pt) = eqpsold(cell, pt);
          for (int i(0); i < num_dims_; ++i) {
            for (int j(0); j < num_dims_; ++j) {
              Fp(cell, pt, i, j) = Fpn(i, j);
            }
          }
        }
      } else {
        if (doit)
          std::cerr << "f > 0\n";
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

        F[0] = f;
        X[0] = 0.0;
        dFdX[0] = (-2. * mubar) * (1. + H / (3. * mubar));

        while (!converged)
        {
          count++;
          H = 2. * mubar * delta_time(0) * temp_adj_relaxation_para_ * std::pow(smag + 2./3.*(K * X[0]) - f, strain_rate_expo_ );
          dH =  strain_rate_expo_ * 2. * mubar * delta_time(0) * temp_adj_relaxation_para_ * (2.* K)/3. * std::pow(smag + 2./3.*(K * X[0]) - f, strain_rate_expo_ - 1. );
          F[0] = f - 2. * mubar * (1. + K/(3. * mubar)) * X[0] - H;
          dFdX[0] = -2. * mubar * (1. + K/(3. * mubar)) - dH;

          if (doit) {
            std::cerr << "before count " << count << '\n';
            std::cerr << "dFdX[0]: " << dFdX[0] << '\n';
            std::cerr << "X[0]: " << X[0] << '\n';
            std::cerr << "F[0]: " << F[0] << '\n';
          }
          solver.solve(dFdX, X, F);
          if (doit) {
            std::cerr << "after count " << count << '\n';
            std::cerr << "dFdX[0]: " << dFdX[0] << '\n';
            std::cerr << "X[0]: " << X[0] << '\n';
            std::cerr << "F[0]: " << F[0] << '\n';
          }
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
              "\ndg = " << dFdX[0] << std::endl);
        }
        solver.computeFadInfo(dFdX, X, F);
        if (doit) {
          std::cerr << "X[0] after fadinfo: ";
          aprints(X[0]);
        }

        dgam_plastic = X[0];

        // plastic direction
        N =  s / Intrepid::norm(s);

        // update s

        if (doit) {
          std::cerr << "s before:\n";
          for (int i(0); i < num_dims_; ++i)
            for (int j(0); j < num_dims_; ++j)
              aprints(s(i,j));
        }

        s -= 2 * mubar * dgam_plastic * N + f * N - 2. * mubar * ( 1. + K/(3. * mubar)) * dgam_plastic * N;

        if (doit) {
          std::cerr << "s after:\n";
          for (int i(0); i < num_dims_; ++i)
            for (int j(0); j < num_dims_; ++j)
              aprints(s(i,j));
        }

        dgam = dgam_plastic + delta_time(0) * temp_adj_relaxation_para_ * std::pow(Intrepid::norm(s), strain_rate_expo_ );

        if (doit) {
          std::cerr << "eqpsold: " << eqpsold(cell, pt) << '\n';
          std::cerr << "dgam_plastic: " << dgam_plastic << '\n';
          std::cerr << "dgam: " << dgam_plastic << '\n';
        }

        if (doit) {
          std::cerr << "K: " << K << '\n';
          std::cerr << "f: " << f << '\n';
          std::cerr << "delta_time(0): " << delta_time(0) << '\n';
          std::cerr << "temp_adj_relaxation_para_: "
            << temp_adj_relaxation_para_ << '\n';
          std::cerr << "strain_rate_expo_: " << strain_rate_expo_ << '\n';
        }
        alpha = eqpsold(cell, pt) + sq23 * dgam_plastic ;
        stripDeriv(alpha);
        if (doit)
          std::cerr << "alpha " << alpha << '\n';

        // plastic direction
        N =  s / Intrepid::norm(s);
        if (doit) {
          std::cerr << "N:\n";
          for (int i(0); i < num_dims_; ++i)
            for (int j(0); j < num_dims_; ++j)
              aprints(N(i,j));
        }

        // update eqps
        eqps(cell, pt) = alpha;

        // exponential map to get Fpnew
        A = dgam * N;
        expA = Intrepid::exp(A);
        if (doit) {
          std::cerr << "dgam: ";
          aprints(dgam);
          std::cerr << "expA:\n";
          for (int i(0); i < num_dims_; ++i)
            for (int j(0); j < num_dims_; ++j)
              aprints(expA(i,j));
          std::cerr << "Fpn:\n";
          for (int i(0); i < num_dims_; ++i)
            for (int j(0); j < num_dims_; ++j)
              aprints(Fpn(i,j));
        }
        Fpnew = expA * Fpn;
        for (int i(0); i < num_dims_; ++i) {
          for (int j(0); j < num_dims_; ++j) {
            Fp(cell, pt, i, j) = Fpnew(i, j);
          }
        }
      }

      // compute pressure
      if (doit) {
        std::cerr << "kappa: ";
        aprints(kappa);
        std::cerr << "J: ";
        aprints(J(cell,pt));
      }
      p = 0.5 * kappa * (J(cell, pt) - 1. / (J(cell, pt)));
      if (doit) {
        std::cerr << "p: " << p << '\n';
        std::cerr << "s:\n";
        for (int i(0); i < num_dims_; ++i)
          for (int j(0); j < num_dims_; ++j)
            aprints(s(i,j));
      }

      // compute stress
      sigma = p * I + s / J(cell, pt);
      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {
          stress(cell, pt, i, j) = sigma(i, j);
        }
      }
      if (doit) {
        std::cerr << "final stress:\n";
        for (int i = 0; i < num_dims_; ++i)
          for (int j = 0; j < num_dims_; ++j)
            aprints(stress(cell,pt,i,j));
      }
    }
  }

  if ((typeid(ScalarT) == typeid(FadType)) && times_called == 5000) {
    std::cerr << "stress:\n";
    for (int cell = 0; cell < workset.numCells; ++cell) {
      std::cerr << "cell " << cell << ":\n";
      for (int pt = 0; pt < num_pts_; ++pt) {
        std::cerr << "pt " << pt << ":\n";
        for (int i = 0; i < num_dims_; ++i)
          for (int j = 0; j < num_dims_; ++j)
            aprints(stress(cell,pt,i,j));
      }
    }
    std::cerr << "Fp:\n";
    for (int cell = 0; cell < workset.numCells; ++cell) {
      std::cerr << "cell " << cell << ":\n";
      for (int pt = 0; pt < num_pts_; ++pt) {
        std::cerr << "pt " << pt << ":\n";
        for (int i = 0; i < num_dims_; ++i)
          for (int j = 0; j < num_dims_; ++j)
            aprints(Fp(cell,pt,i,j));
      }
    }
    std::cerr << "eqps:\n";
    for (int cell = 0; cell < workset.numCells; ++cell) {
      std::cerr << "cell " << cell << ":\n";
      for (int pt = 0; pt < num_pts_; ++pt)
        aprints(eqps(cell,pt));
    }
  }

  if ((typeid(ScalarT) == typeid(FadType)) && times_called == 5000)
    abort();

  ++times_called;
}


} //namespace LCM
