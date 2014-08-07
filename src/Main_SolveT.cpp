//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <iostream>
#include <string>


#include "Albany_Utils.hpp"
#include "Albany_SolverFactory.hpp"

#include "Piro_PerformSolve.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Teuchos_GlobalMPISession.hpp"
#include "Teuchos_TimeMonitor.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_StandardCatchMacros.hpp"

// Uncomment for run time nan checking
// This is set in the toplevel CMakeLists.txt file
//#define ALBANY_CHECK_FPE

#ifdef ALBANY_CHECK_FPE
#include <math.h>
//#include <Accelerate/Accelerate.h>
#include <xmmintrin.h>
#endif

#include "Albany_DataTypes.hpp"

// Global variable that denotes this is the Tpetra executable
bool TpetraBuild = true;

void tpetraFromThyra(
  const Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST> > > &thyraResponses,
  const Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<ST> > > > &thyraSensitivities,
  Teuchos::Array<Teuchos::RCP<const Tpetra_Vector> > &responses,
  Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector> > > &sensitivities)
{
  responses.clear();
  responses.reserve(thyraResponses.size());
  typedef Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST> > > ThyraResponseArray;
  for (ThyraResponseArray::const_iterator it_begin = thyraResponses.begin(),
      it_end = thyraResponses.end(),
      it = it_begin;
      it != it_end;
      ++it) {
    responses.push_back(Teuchos::nonnull(*it) ? ConverterT::getConstTpetraVector(*it) : Teuchos::null);
  }

  sensitivities.clear();
  sensitivities.reserve(thyraSensitivities.size());
  typedef Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<ST> > > > ThyraSensitivityArray;
  for (ThyraSensitivityArray::const_iterator it_begin = thyraSensitivities.begin(),
      it_end = thyraSensitivities.end(),
      it = it_begin;
      it != it_end;
      ++it) {
    ThyraSensitivityArray::const_reference sens_thyra = *it;
    Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector> > sens;
    sens.reserve(sens_thyra.size());
    for (ThyraSensitivityArray::value_type::const_iterator jt = sens_thyra.begin(),
        jt_end = sens_thyra.end();
        jt != jt_end;
        ++jt) {
        sens.push_back(Teuchos::nonnull(*jt) ? ConverterT::getConstTpetraMultiVector(*jt) : Teuchos::null);
    }
    sensitivities.push_back(sens);
  }
}

int main(int argc, char *argv[]) {

  int status=0; // 0 = pass, failures are incremented
  bool success = true;

  Teuchos::GlobalMPISession mpiSession(&argc,&argv);

#ifdef ALBANY_CHECK_FPE
//	_mm_setcsr(_MM_MASK_MASK &~
//		(_MM_MASK_OVERFLOW | _MM_MASK_INVALID | _MM_MASK_DIV_ZERO) );
    _MM_SET_EXCEPTION_MASK(_MM_GET_EXCEPTION_MASK() & ~_MM_MASK_INVALID);
#endif

  using Teuchos::RCP;
  using Teuchos::rcp;

  RCP<Teuchos::FancyOStream> out(Teuchos::VerboseObjectBase::getDefaultOStream());

  // Command-line argument for input file
  std::string xmlfilename;
  if(argc > 1){

    if(!strcmp(argv[1],"--help")){
      printf("albany [inputfile.xml]\n");
      exit(1);
    }
    else
      xmlfilename = argv[1];

  }
  else
    xmlfilename = "input.xml";

  try {
    RCP<Teuchos::Time> totalTime =
      Teuchos::TimeMonitor::getNewTimer("Albany: ***Total Time***");

    RCP<Teuchos::Time> setupTime =
      Teuchos::TimeMonitor::getNewTimer("Albany: Setup Time");
    Teuchos::TimeMonitor totalTimer(*totalTime); //start timer
    Teuchos::TimeMonitor setupTimer(*setupTime); //start timer

    RCP<const Teuchos_Comm> comm =
      Tpetra::DefaultPlatform::getDefaultPlatform().getComm();

    Albany::SolverFactory slvrfctry(xmlfilename, comm);
    RCP<Albany::Application> app;
    const RCP<Thyra::ResponseOnlyModelEvaluatorBase<ST> > solver =
      slvrfctry.createAndGetAlbanyAppT(app, comm, comm);

    setupTimer.~TimeMonitor();

    Teuchos::ParameterList &solveParams =
      slvrfctry.getAnalysisParameters().sublist("Solve", /*mustAlreadyExist =*/ false);
    // By default, request the sensitivities if not explicitly disabled
    solveParams.get("Compute Sensitivities", true);

    Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST> > > thyraResponses;
    Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<ST> > > > thyraSensitivities;
    Piro::PerformSolve(*solver, solveParams, thyraResponses, thyraSensitivities);

    Teuchos::Array<Teuchos::RCP<const Tpetra_Vector> > responses;
    Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector> > > sensitivities;
    tpetraFromThyra(thyraResponses, thyraSensitivities, responses, sensitivities);

    const int num_p = solver->Np(); // Number of *vectors* of parameters
    const int num_g = solver->Ng(); // Number of *vectors* of responses

    *out << "Finished eval of first model: Params, Responses "
      << std::setprecision(12) << std::endl;

    Teuchos::RCP<Teuchos::ParameterList> problemParams = app->getProblemPL();

    Teuchos::ParameterList& parameterParams =
       problemParams->sublist("Parameters");
    Teuchos::ParameterList& responseParams =
      problemParams->sublist("Response Functions");

    int num_param_vecs =
       parameterParams.get("Number of Parameter Vectors", 0);
    bool using_old_parameter_list = false;
    if (parameterParams.isType<int>("Number")) {
      int numParameters = parameterParams.get<int>("Number");
      if (numParameters > 0) {
        num_param_vecs = 1;
        using_old_parameter_list = true;
      }
    }

    int num_response_vecs =
       responseParams.get("Number of Response Vectors", 0);
    bool using_old_response_list = false;
    if (responseParams.isType<int>("Number")) {
      int numParameters = responseParams.get<int>("Number");
      if (numParameters > 0) {
        num_response_vecs = 1;
        using_old_response_list = true;
      }
    }

    Teuchos::Array<Teuchos::RCP<Teuchos::Array<std::string> > > param_names;
    param_names.resize(num_param_vecs);
    for (int l = 0; l < num_param_vecs; ++l) {
      const Teuchos::ParameterList* pList =
        using_old_parameter_list ?
        &parameterParams :
        &(parameterParams.sublist(Albany::strint("Parameter Vector", l)));

      const int numParameters = pList->get<int>("Number");
      TEUCHOS_TEST_FOR_EXCEPTION(
          numParameters == 0,
          Teuchos::Exceptions::InvalidParameter,
          std::endl << "Error!  In Albany::ModelEvaluatorT constructor:  " <<
          "Parameter vector " << l << " has zero parameters!" << std::endl);

      param_names[l] = Teuchos::rcp(new Teuchos::Array<std::string>(numParameters));
      for (int k = 0; k < numParameters; ++k) {
        (*param_names[l])[k] =
          pList->get<std::string>(Albany::strint("Parameter", k));
      }
    }

    Teuchos::Array<Teuchos::RCP<Teuchos::Array<std::string> > > response_names;
    response_names.resize(num_response_vecs);
    for (int l = 0; l < num_response_vecs; ++l) {
      const Teuchos::ParameterList* pList =
        using_old_response_list ?
        &responseParams :
        &(responseParams.sublist(Albany::strint("Response Vector", l)));

      bool number_exists = pList->getEntryPtr("Number");

      if(number_exists){

        const int numParameters = pList->get<int>("Number");
        TEUCHOS_TEST_FOR_EXCEPTION(
          numParameters == 0,
          Teuchos::Exceptions::InvalidParameter,
          std::endl << "Error!  In Albany::ModelEvaluatorT constructor:  " <<
          "Response vector " << l << " has zero parameters!" << std::endl);

        response_names[l] = Teuchos::rcp(new Teuchos::Array<std::string>(numParameters));
        for (int k = 0; k < numParameters; ++k) {
          (*response_names[l])[k] =
            pList->get<std::string>(Albany::strint("Response", k));
        }
      }
      else response_names[l] = Teuchos::null;
    }

    const Thyra::ModelEvaluatorBase::InArgs<double> nominal = solver->getNominalValues();

    for (int i=0; i<num_p; i++) {
      Albany::printTpetraVector(*out << "\nParameter vector " << i << ":\n", *param_names[i],
           ConverterT::getConstTpetraVector(nominal.get_p(i)));
    }

    for (int i=0; i<num_g-1; i++) {
      const RCP<const Tpetra_Vector> g = responses[i];
      bool is_scalar = true;

      if (app != Teuchos::null)
        is_scalar = app->getResponse(i)->isScalarResponse();

      if (is_scalar) {

        if(response_names[i] != Teuchos::null)
          Albany::printTpetraVector(*out << "\nResponse vector " << i << ":\n", *response_names[i], g);
        else
          Albany::printTpetraVector(*out << "\nResponse vector " << i << ":\n", g);

        if (num_p == 0) {
          // Just calculate regression data
          status += slvrfctry.checkSolveTestResultsT(i, 0, g.get(), NULL);
        } else {
          for (int j=0; j<num_p; j++) {
            const RCP<const Tpetra_MultiVector> dgdp = sensitivities[i][j];
            if (Teuchos::nonnull(dgdp)) {

              Albany::printTpetraVector(*out << "\nSensitivities (" << i << "," << j << "):!\n", dgdp);

            }
            status += slvrfctry.checkSolveTestResultsT(i, j, g.get(), dgdp.get());
          }
        }
      }
    }

    const RCP<const Tpetra_Vector> xfinal = responses.back();
    double mnv = xfinal->meanValue();
    *out << "Main_Solve: MeanValue of final solution " << mnv << std::endl;
    *out << "\nNumber of Failed Comparisons: " << status << std::endl;
  }
  TEUCHOS_STANDARD_CATCH_STATEMENTS(true, std::cerr, success);
  if (!success) status+=10000;

  Teuchos::TimeMonitor::summarize(*out,false,true,false/*zero timers*/);
  return status;
}
