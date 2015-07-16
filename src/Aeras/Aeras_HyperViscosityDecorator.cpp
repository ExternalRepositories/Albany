//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include "Aeras_HyperViscosityDecorator.hpp"
#include "Albany_SolverFactory.hpp"
#include "Albany_ModelFactory.hpp"
#include "Teuchos_TestForException.hpp"
#include "Teuchos_VerboseObject.hpp"
#include <sstream>

//uncomment the following to write stuff out to matrix market to debug
//#define WRITE_TO_MATRIX_MARKET

#ifdef WRITE_TO_MATRIX_MARKET
static
int mm_counter = 0;
#endif // WRITE_TO_MATRIX_MARKET

#define OUTPUT_TO_SCREEN 

Aeras::
HyperViscosityDecorator::
HyperViscosityDecorator(
    Teuchos::RCP<Teuchos::ParameterList> const & app_params,
    Teuchos::RCP<Teuchos::Comm<int> const> const & commT,
    Teuchos::RCP<Tpetra_Vector const> const & initial_guessT,
    Teuchos::RCP<Thyra::LinearOpWithSolveFactoryBase<ST> const> const &
    solver_factory)
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif

  commT_ = commT;

  solver_factory_ = solver_factory;

  //IKT, 7/9/15:
  //For now, read in "Coupled System" sublist in "master" input file for the problem.  
  //This is in case we end up creating a separate problem for the Laplace operator.  
  //If we don't do that, I'll clean this up so there is just 1 input file. 
 
  Teuchos::ParameterList &coupled_system_params = app_params->sublist("Coupled System");
  // Get names of individual model xml input files from problem parameterlist
  Teuchos::Array<std::string> model_filenames = coupled_system_params.get<Teuchos::Array<std::string>>("Model XML Files");

  //number of models
  num_models_ = model_filenames.size();

#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: num_models_: " << num_models_ << '\n';
#endif

  //Set up application and model objects
  //(similar logic to that in Albany::SolverFactory::createAlbanyAppAndModelT)
  Teuchos::RCP<Teuchos::ParameterList> model_app_params; 
  Teuchos::RCP<Teuchos::ParameterList> model_problem_params; 
  //get parameterlist from model *.xml file
  Albany::SolverFactory solver_fact(model_filenames[0], commT_);
  Teuchos::ParameterList &app_parameters = solver_fact.getParameters();
  model_app_params = Teuchos::rcp(&(app_parameters), false);
  Teuchos::RCP<Teuchos::ParameterList> problem_params = Teuchos::sublist(model_app_params, "Problem");
  model_problem_params = problem_params;
  std::string const &problem_name = problem_params->get("Name", "");
  std::string solution_method = problem_params->get("Solution Method", "Steady");
#ifdef OUTPUT_TO_SCREEN
  //Debug output
  std::cout << "Name of problem:" << problem_name << '\n';
  std::cout << "Solution method: " << solution_method << '\n'; 
#endif

  //Create application object
  app_ = Teuchos::rcp(new Albany::Application(commT, model_app_params, initial_guessT));

  //Create model evaluator
  Albany::ModelFactory model_factory(model_app_params, app_);
  model_ = model_factory.createT();

#ifdef OUTPUT_TO_SCREEN
  std::cout << "Finished creating Albany app and model!\n";
#endif

  //FIXME: create Stiffness and Mass matrix for Laplace operator problem and 
  //evaluate Mass^(-1)*Stiffness.  Store this as a Tpetra_CrsMatrix, call is Jac_Laplace
  //(make this a member function). 
  //
  //Laplace operator Stiffness/Mass matrices can be obtained either by adding Laplace operator
  //to Aeras::ShallowWaterResid class and having it be turned off/on with omega variable, or 
  //by writing a separate problem called "Laplace" with its own evaluator.  In the former approach, 
  //you'd get the Laplace Stiffness with a call like: 
  //app->computeGlobalJacobianT(0.0, 0.0, 1.0, curr_time, x_dotT.get(), x_dotdotT.get(), *xT, 
  //                             sacado_param_vec, ftmp.get(), *Jac_Laplace);
  //with all the arguments curr_time, x_dotT, xdotdotT, xT, etc as dummies.
  //The mass matrix can be obtained with: 
  //app->computeGlobalJacobianT(1.0, 0.0, 0.0, curr_time, x_dotT.get(), x_dotdotT.get(), *xT, 
  //                             sacado_param_vec, ftmp.get(), *Mass);
  //An omega*Laplacian(U)*WGradBF or omega*Laplacian(UDotDot)*WGradBF term would need to be added in the Aeras::ShallowWaterResid 
  //(we will need to check which one).  In Aeras::ShallowWaterResid, it should be possible to get omega from the workset as follows: 
  //omega = workset.n_coeff 

}

Aeras::HyperViscosityDecorator::~HyperViscosityDecorator()
{
}

// Overridden from Thyra::ModelEvaluator<ST>
Teuchos::RCP<Thyra::VectorSpaceBase<ST> const>
Aeras::HyperViscosityDecorator::get_x_space() const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->get_x_space(); 
}

Teuchos::RCP<Thyra::VectorSpaceBase<ST> const>
Aeras::HyperViscosityDecorator::get_f_space() const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->get_f_space(); 
}


Teuchos::RCP<Thyra::VectorSpaceBase<ST> const>
Aeras::HyperViscosityDecorator::get_p_space(int l) const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->get_p_space(l); 
}

Teuchos::RCP<Thyra::VectorSpaceBase<ST> const>
Aeras::HyperViscosityDecorator::get_g_space(int l) const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->get_g_space(l); 
}

Teuchos::RCP<const Teuchos::Array<std::string>>
Aeras::HyperViscosityDecorator::get_p_names(int l) const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->get_p_names(l); 
}

Thyra::ModelEvaluatorBase::InArgs<ST>
Aeras::HyperViscosityDecorator::getNominalValues() const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->getNominalValues(); 
}

Thyra::ModelEvaluatorBase::InArgs<ST>
Aeras::HyperViscosityDecorator::getLowerBounds() const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->getLowerBounds(); 
}

Thyra::ModelEvaluatorBase::InArgs<ST>
Aeras::HyperViscosityDecorator::getUpperBounds() const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->getUpperBounds(); 
}

Teuchos::RCP<Thyra::LinearOpBase<ST>>
Aeras::HyperViscosityDecorator::create_W_op() const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->create_W_op(); 
}

Teuchos::RCP<Thyra::PreconditionerBase<ST>>
Aeras::HyperViscosityDecorator::create_W_prec() const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->create_W_prec(); 
}

Teuchos::RCP<const Thyra::LinearOpWithSolveFactoryBase<ST>>
Aeras::HyperViscosityDecorator::get_W_factory() const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->get_W_factory(); 
}


Thyra::ModelEvaluatorBase::InArgs<ST>
Aeras::HyperViscosityDecorator::createInArgs() const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->createInArgs(); 
}



void
Aeras::HyperViscosityDecorator::reportFinalPoint(
    Thyra::ModelEvaluatorBase::InArgs<ST> const & final_point,
    bool const was_solved)
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->reportFinalPoint(final_point, was_solved); 
}


/// Create operator form of dg/dx for distributed responses
Teuchos::RCP<Thyra::LinearOpBase<ST>>
Aeras::HyperViscosityDecorator::
create_DgDx_op_impl(int j) const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->create_DgDx_op(j); 
}

/// Create operator form of dg/dx_dot for distributed responses
Teuchos::RCP<Thyra::LinearOpBase<ST>>
Aeras::HyperViscosityDecorator::
create_DgDx_dot_op_impl(int j) const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->create_DgDx_dot_op(j); 
}

/// Create OutArgs
Thyra::ModelEvaluatorBase::OutArgs<ST>
Aeras::HyperViscosityDecorator::
createOutArgsImpl() const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif
  return model_->createOutArgs(); 
}

/// Evaluate model on InArgs
void
Aeras::HyperViscosityDecorator::
evalModelImpl(
    Thyra::ModelEvaluatorBase::InArgs<ST> const & in_args,
    Thyra::ModelEvaluatorBase::OutArgs<ST> const & out_args) const
{
#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << '\n';
#endif
  model_->evalModel(in_args, out_args);
  //FIXME: get x from in_args.
  //Set xtilde = Jac_Laplace*x 
  //Get f from out_args.
  //Add tau*Jac_Laplace*xtilde to residual f
}

