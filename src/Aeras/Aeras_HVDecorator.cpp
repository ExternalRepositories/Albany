//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include "Aeras_HVDecorator.hpp"
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

Aeras::HVDecorator::HVDecorator(
    const Teuchos::RCP<Albany::Application>& app_,
    const Teuchos::RCP<Teuchos::ParameterList>& appParams)
    :Albany::ModelEvaluatorT(app_,appParams)
{

#ifdef OUTPUT_TO_SCREEN
  std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << "\n";
#endif

  std::cout << "In HVDecorator app name: " << app->getProblemPL()->get("Name", "") << std::endl;

//Let's keep this for later
  Teuchos::ParameterList &coupled_system_params = appParams->sublist("Coupled System");

}

//og: do I have to copy/paste this from AMET.cpp?
namespace {
// As of early Jan 2015, it seems there is some conflict between Thyra's use of
// NaN to initialize certain quantities and Tpetra's v.update(alpha, x, 0)
// implementation. In the past, 0 as the third argument seemed to trigger a code
// path that does a set v <- alpha x rather than an accumulation v <- alpha x +
// beta v. Hence any(isnan(v(:))) was not a problem if beta == 0. That seems not
// to be entirely true now. For some reason, this problem occurs only in DEBUG
// builds in the sensitivities. I have not had time to fully dissect this
// problem to determine why the problem occurs only there, but the solution is
// nonetheless quite suggestive: sanitize v before calling update. I do this at
// the highest level, here, rather than in the responses.
void sanitize_nans (const Thyra::ModelEvaluatorBase::Derivative<ST>& v) {
  if ( ! v.isEmpty() && Teuchos::nonnull(v.getMultiVector()))
    ConverterT::getTpetraMultiVector(v.getMultiVector())->putScalar(0.0);
}
} // namespace


// hide the original parental method AMET->evalModelImpl():
void
Aeras::HVDecorator::evalModelImpl(
    const Thyra::ModelEvaluatorBase::InArgs<ST>& inArgsT,
    const Thyra::ModelEvaluatorBase::OutArgs<ST>& outArgsT) const
{

  std::cout << "DEBUG WHICH HVDecorator: " << __PRETTY_FUNCTION__ << "\n";
	
  Teuchos::TimeMonitor Timer(*timer); //start timer
  //
  // Get the input arguments
  //
  const Teuchos::RCP<const Tpetra_Vector> xT =
    ConverterT::getConstTpetraVector(inArgsT.get_x());

  const Teuchos::RCP<const Tpetra_Vector> x_dotT =
    Teuchos::nonnull(inArgsT.get_x_dot()) ?
    ConverterT::getConstTpetraVector(inArgsT.get_x_dot()) :
    Teuchos::null;

  // AGS: x_dotdot time integrators not imlemented in Thyra ME yet
  //const Teuchos::RCP<const Tpetra_Vector> x_dotdotT =
  //  Teuchos::nonnull(inArgsT.get_x_dotdot()) ?
  //  ConverterT::getConstTpetraVector(inArgsT.get_x_dotdot()) :
  //  Teuchos::null;
  const Teuchos::RCP<const Tpetra_Vector> x_dotdotT = Teuchos::null;


  const double alpha = (Teuchos::nonnull(x_dotT) || Teuchos::nonnull(x_dotdotT)) ? inArgsT.get_alpha() : 0.0;
  // AGS: x_dotdot time integrators not imlemented in Thyra ME yet
  // const double omega = (Teuchos::nonnull(x_dotT) || Teuchos::nonnull(x_dotdotT)) ? inArgsT.get_omega() : 0.0;
  const double omega = 0.0;
  const double beta = (Teuchos::nonnull(x_dotT) || Teuchos::nonnull(x_dotdotT)) ? inArgsT.get_beta() : 1.0;
  const double curr_time = (Teuchos::nonnull(x_dotT) || Teuchos::nonnull(x_dotdotT)) ? inArgsT.get_t() : 0.0;

  for (int l = 0; l < inArgsT.Np(); ++l) {
    const Teuchos::RCP<const Thyra::VectorBase<ST> > p = inArgsT.get_p(l);
    if (Teuchos::nonnull(p)) {
      const Teuchos::RCP<const Tpetra_Vector> pT = ConverterT::getConstTpetraVector(p);
      const Teuchos::ArrayRCP<const ST> pT_constView = pT->get1dView();

      ParamVec &sacado_param_vector = sacado_param_vec[l];
      for (unsigned int k = 0; k < sacado_param_vector.size(); ++k) {
        sacado_param_vector[k].baseValue = pT_constView[k];
      }
    }
  }

  //
  // Get the output arguments
  //
  const Teuchos::RCP<Tpetra_Vector> fT_out =
    Teuchos::nonnull(outArgsT.get_f()) ?
    ConverterT::getTpetraVector(outArgsT.get_f()) :
    Teuchos::null;

  const Teuchos::RCP<Tpetra_Operator> W_op_outT =
    Teuchos::nonnull(outArgsT.get_W_op()) ?
    ConverterT::getTpetraOperator(outArgsT.get_W_op()) :
    Teuchos::null;

#ifdef WRITE_MASS_MATRIX_TO_MM_FILE
  //IK, 4/24/15: adding object to hold mass matrix to be written to matrix market file
  const Teuchos::RCP<Tpetra_Operator> Mass =
    Teuchos::nonnull(outArgsT.get_W_op()) ?
    ConverterT::getTpetraOperator(outArgsT.get_W_op()) :
    Teuchos::null;
  //IK, 4/24/15: needed for writing mass matrix out to matrix market file
  const Teuchos::RCP<Tpetra_Vector> ftmp =
    Teuchos::nonnull(outArgsT.get_f()) ?
    ConverterT::getTpetraVector(outArgsT.get_f()) :
    Teuchos::null;
#endif

  // Cast W to a CrsMatrix, throw an exception if this fails
  const Teuchos::RCP<Tpetra_CrsMatrix> W_op_out_crsT =
    Teuchos::nonnull(W_op_outT) ?
    Teuchos::rcp_dynamic_cast<Tpetra_CrsMatrix>(W_op_outT, true) :
    Teuchos::null;

#ifdef WRITE_MASS_MATRIX_TO_MM_FILE
  //IK, 4/24/15: adding object to hold mass matrix to be written to matrix market file
  const Teuchos::RCP<Tpetra_CrsMatrix> Mass_crs =
    Teuchos::nonnull(Mass) ?
    Teuchos::rcp_dynamic_cast<Tpetra_CrsMatrix>(Mass, true) :
    Teuchos::null;
#endif

  //
  // Compute the functions
  //
  bool f_already_computed = false;

  // W matrix
  if (Teuchos::nonnull(W_op_out_crsT)) {
    app->computeGlobalJacobianT(
        alpha, beta, omega, curr_time, x_dotT.get(), x_dotdotT.get(),  *xT,
        sacado_param_vec, fT_out.get(), *W_op_out_crsT);
    f_already_computed = true;
#ifdef WRITE_MASS_MATRIX_TO_MM_FILE
    //IK, 4/24/15: write mass matrix to matrix market file
    //Warning: to read this in to MATLAB correctly, code must be run in serial.
    //Otherwise Mass will have a distributed Map which would also need to be read in to MATLAB for proper
    //reading in of Mass.
    app->computeGlobalJacobianT(1.0, 0.0, 0.0, curr_time, x_dotT.get(), x_dotdotT.get(), *xT, 
                               sacado_param_vec, ftmp.get(), *Mass_crs);
    Tpetra_MatrixMarket_Writer::writeSparseFile("mass.mm", Mass_crs);
    Tpetra_MatrixMarket_Writer::writeMapFile("rowmap.mm", *Mass_crs->getRowMap()); 
    Tpetra_MatrixMarket_Writer::writeMapFile("colmap.mm", *Mass_crs->getColMap()); 
#endif
  }

  // df/dp
  for (int l = 0; l < outArgsT.Np(); ++l) {
    const Teuchos::RCP<Thyra::MultiVectorBase<ST> > dfdp_out =
      outArgsT.get_DfDp(l).getMultiVector();

    const Teuchos::RCP<Tpetra_MultiVector> dfdp_outT =
      Teuchos::nonnull(dfdp_out) ?
      ConverterT::getTpetraMultiVector(dfdp_out) :
      Teuchos::null;

    if (Teuchos::nonnull(dfdp_outT)) {
      const Teuchos::RCP<ParamVec> p_vec = Teuchos::rcpFromRef(sacado_param_vec[l]);

      app->computeGlobalTangentT(
          0.0, 0.0, 0.0, curr_time, false, x_dotT.get(), x_dotdotT.get(), *xT,
          sacado_param_vec, p_vec.get(),
          NULL, NULL, NULL, NULL, fT_out.get(), NULL,
          dfdp_outT.get());

      f_already_computed = true;
    }
  }

  // f
  if (app->is_adjoint) {
    const Thyra::ModelEvaluatorBase::Derivative<ST> f_derivT(
        outArgsT.get_f(),
        Thyra::ModelEvaluatorBase::DERIV_TRANS_MV_BY_ROW);

    const Thyra::ModelEvaluatorBase::Derivative<ST> dummy_derivT;

    const int response_index = 0; // need to add capability for sending this in
    app->evaluateResponseDerivativeT(
        response_index, curr_time, x_dotT.get(), x_dotdotT.get(), *xT,
        sacado_param_vec, NULL,
        NULL, f_derivT, dummy_derivT, dummy_derivT, dummy_derivT);
  } else {
    if (Teuchos::nonnull(fT_out) && !f_already_computed) {
      app->computeGlobalResidualT(
          curr_time, x_dotT.get(), x_dotdotT.get(), *xT,
          sacado_param_vec, *fT_out);
    }
  }

  // Response functions
  for (int j = 0; j < outArgsT.Ng(); ++j) {
    const Teuchos::RCP<Thyra::VectorBase<ST> > g_out = outArgsT.get_g(j);
    Teuchos::RCP<Tpetra_Vector> gT_out =
      Teuchos::nonnull(g_out) ?
      ConverterT::getTpetraVector(g_out) :
      Teuchos::null;

    const Thyra::ModelEvaluatorBase::Derivative<ST> dgdxT_out = outArgsT.get_DgDx(j);
    const Thyra::ModelEvaluatorBase::Derivative<ST> dgdxdotT_out = outArgsT.get_DgDx_dot(j);
    // AGS: x_dotdot time integrators not imlemented in Thyra ME yet
    const Thyra::ModelEvaluatorBase::Derivative<ST> dgdxdotdotT_out;
    sanitize_nans(dgdxT_out);
    sanitize_nans(dgdxdotT_out);
    sanitize_nans(dgdxdotdotT_out);

    // dg/dx, dg/dxdot
    if (!dgdxT_out.isEmpty() || !dgdxdotT_out.isEmpty()) {
      const Thyra::ModelEvaluatorBase::Derivative<ST> dummy_derivT;
      app->evaluateResponseDerivativeT(
          j, curr_time, x_dotT.get(), x_dotdotT.get(), *xT,
          sacado_param_vec, NULL,
          gT_out.get(), dgdxT_out,
          dgdxdotT_out, dgdxdotdotT_out, dummy_derivT);
      // Set gT_out to null to indicate that g_out was evaluated.
      gT_out = Teuchos::null;
    }

    // dg/dp
    for (int l = 0; l < outArgsT.Np(); ++l) {
      const Teuchos::RCP<Thyra::MultiVectorBase<ST> > dgdp_out =
        outArgsT.get_DgDp(j, l).getMultiVector();
      const Teuchos::RCP<Tpetra_MultiVector> dgdpT_out =
        Teuchos::nonnull(dgdp_out) ?
        ConverterT::getTpetraMultiVector(dgdp_out) :
        Teuchos::null;

      if (Teuchos::nonnull(dgdpT_out)) {
        const Teuchos::RCP<ParamVec> p_vec = Teuchos::rcpFromRef(sacado_param_vec[l]);
        app->evaluateResponseTangentT(
            j, alpha, beta, omega, curr_time, false,
            x_dotT.get(), x_dotdotT.get(), *xT,
            sacado_param_vec, p_vec.get(),
            NULL, NULL, NULL, NULL, gT_out.get(), NULL,
            dgdpT_out.get());
        gT_out = Teuchos::null;
      }
    }

    if (Teuchos::nonnull(gT_out)) {
      app->evaluateResponseT(
          j, curr_time, x_dotT.get(), x_dotdotT.get(), *xT,
          sacado_param_vec, *gT_out);
    }
  }
}



