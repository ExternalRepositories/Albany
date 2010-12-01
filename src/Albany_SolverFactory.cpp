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


#include "Albany_SolverFactory.hpp"
#include "ENAT_SGNOXSolver.hpp"
#include "Albany_RythmosObserver.hpp"
#include "Albany_NOXObserver.hpp"
#include "Thyra_DetachedVectorView.hpp"

#include "Piro_Epetra_NOXSolver.hpp"
#include "Piro_Epetra_LOCASolver.hpp"
#include "Piro_Epetra_RythmosSolver.hpp"

#include "Teuchos_XMLParameterListHelpers.hpp"
#ifdef ALBANY_MPI
#include "Teuchos_DefaultMpiComm.hpp"
#else
#include "Teuchos_DefaultSerialComm.hpp"
#endif


using Teuchos::RCP;
using Teuchos::rcp;
using Teuchos::ParameterList;

Albany::SolverFactory::SolverFactory(
			  const std::string inputFile, 
			  const Teuchos::RCP<const Epetra_Comm>& comm) 
  : Comm(comm),
    out(Teuchos::VerboseObjectBase::getDefaultOStream())
{
#ifdef ALBANY_MPI
    Teuchos::RCP<const Epetra_MpiComm> mpiComm =
      Teuchos::rcp_dynamic_cast<const Epetra_MpiComm>(Comm, true);
    Teuchos::MpiComm<int> tcomm = 
      Teuchos::MpiComm<int>(Teuchos::opaqueWrapper(mpiComm->Comm()));
#else
    Teuchos::SerialComm<int> tcomm = Teuchos::SerialComm<int>();
#endif

    // Set up application parameters: read and broadcast XML file, and set defaults
    appParams = rcp(new ParameterList("Albany Parameters"));
    Teuchos::updateParametersFromXmlFileAndBroadcast(inputFile, appParams.get(), tcomm);

    RCP<ParameterList> defaultSolverParams = rcp(new ParameterList());
    setSolverParamDefaults(defaultSolverParams.get());
    appParams->setParametersNotAlreadySet(*defaultSolverParams);

    appParams->validateParametersAndSetDefaults(*getValidAppParameters(),0);

    // Get solver type
    ParameterList& problemParams = appParams->sublist("Problem");
    transient = problemParams.get("Transient", false);
    continuation = problemParams.get("Continuation", false);
    stochastic = problemParams.get("Stochastic", false);
}

void Albany::SolverFactory::createModel(
  const Teuchos::RCP<const Epetra_Comm>& appComm_,
  const Teuchos::RCP<const Epetra_Vector>& initial_guess)
{
  Teuchos::RCP<const Epetra_Comm> appComm = appComm_;
  if (appComm == Teuchos::null)
    appComm = Comm;

    // Create application
    app = rcp(new Albany::Application(appComm, appParams, initial_guess));

    //set up parameters
    ParameterList& problemParams = appParams->sublist("Problem");
    ParameterList& parameterParams = problemParams.sublist("Parameters");
    parameterParams.validateParameters(*getValidParameterParameters(),0);

    numParameters = parameterParams.get("Number", 0);
    RCP< Teuchos::Array<std::string> > free_param_names;
    if (numParameters>0) {
      free_param_names = rcp(new Teuchos::Array<std::string>);
      for (int i=0; i<numParameters; i++) {
        std::ostringstream ss;
        ss << "Parameter " << i;
        free_param_names->push_back(parameterParams.get(ss.str(), "??"));
      }
    }
    *out << "Number of Parameters in ENAT = " << numParameters << endl;

    //set up SG parameters
    ParameterList& sgParams =
      problemParams.sublist("Stochastic Galerkin");
    ParameterList& sg_parameterParams =
      sgParams.sublist("SG Parameters");
 
    sg_parameterParams.validateParameters(*getValidParameterParameters(),0);
    int sg_numParameters = sg_parameterParams.get("Number", 0);
    RCP< Teuchos::Array<std::string> > sg_param_names;
    if (sg_numParameters>0) {
      sg_param_names = rcp(new Teuchos::Array<std::string>);
      for (int i=0; i<sg_numParameters; i++) {
        std::ostringstream ss;
        ss << "Parameter " << i;
      sg_param_names->push_back(sg_parameterParams.get(ss.str(), "??"));
      }
    }
    *out << "Number of SG Parameters in ENAT = " << sg_numParameters << endl;

    // Validate Response list: may move inside individual Problem class
    problemParams.sublist("Response Functions").
      validateParameters(*getValidResponseParameters(),0);

    // Create model evaluator
    model = rcp<EpetraExt::ModelEvaluator>
      (new Albany::ModelEvaluator(app, free_param_names, sg_param_names));

    // Create observer for output from time-stepper
    RCP<Albany_VTK> vtk;

    if (appParams->sublist("VTK").get("Do Visualization", false) == true) {
      vtk = rcp(new Albany_VTK(appParams->sublist("VTK")));
    }
    if (transient)
      Rythmos_observer = rcp<Rythmos::IntegrationObserverBase<Scalar> >
        (new Albany_RythmosObserver(vtk, app));
    else  // both NOX and LOCA can use this observer...
      NOX_observer = rcp<NOX::Epetra::Observer>
        (new Albany_NOXObserver(vtk, app));
}


RCP<EpetraExt::ModelEvaluator> Albany::SolverFactory::create()
{

    if (transient) 
      return  rcp(new Piro::Epetra::RythmosSolver(appParams, model, Rythmos_observer));
    else if (continuation)
      // add save eigen data here
      return  rcp(new Piro::Epetra::LOCASolver(appParams, model, NOX_observer));
    else if (stochastic)
      return  rcp(new ENAT::SGNOXSolver(appParams, model, Comm, NOX_observer));
    else // default to NOX
      return  rcp(new Piro::Epetra::NOXSolver(appParams, model, NOX_observer));
}


int Albany::SolverFactory::checkTestResults(
  const Epetra_Vector* g,
  const Epetra_MultiVector* dgdp,
  const Teuchos::SerialDenseVector<int,double>* drdv,
  const Teuchos::RCP<Thyra::VectorBase<double> >& tvec,
  const Teuchos::RCP<Stokhos::EpetraVectorOrthogPoly>& g_sg) const
{
  ParameterList& testParams = appParams->sublist("Regression Results");
  testParams.validateParametersAndSetDefaults(*getValidRegressionResultsParameters(),0);

  int failures = 0;
  int comparisons = 0;
  double relTol = testParams.get<double>("Relative Tolerance");
  double absTol = testParams.get<double>("Absolute Tolerance");

  // Get number of responses (g) to test
  int numResponseTests = testParams.get<int>("Number of Comparisons");
  if (numResponseTests > 0 && g != NULL) {

    if (numResponseTests > g->MyLength()) failures +=1000;
    else { // do comparisons
      // Read accepted test results
      Teuchos::Array<double> testValues =
         Teuchos::getArrayFromStringParameter<double> (testParams,
                                "Test Values", numResponseTests, true);

      for (int i=0; i<numResponseTests; i++) {
        failures += scaledCompare((*g)[i], testValues[i], relTol, absTol);
        comparisons++;
      }
    }
  }

  // Repeat comparisons for sensitivities
  int numSensTests = testParams.get<int>("Number of Sensitivity Comparisons");
  if (numSensTests > 0 && dgdp != NULL) {

    if (numSensTests > dgdp->MyLength() ||
        numParameters != dgdp->NumVectors() ) failures += 10000;
    else {
      Teuchos::Array<double> testSensValues;
      for (int i=0; i<numSensTests; i++) {
        std::stringstream tsv;
        tsv << "Sensitivity Test Values " << i;
        testSensValues = Teuchos::getArrayFromStringParameter<double> (testParams,
                                    tsv.str(), numParameters, true);
        for (int j=0; j<numParameters; j++) {
          failures += scaledCompare((*dgdp)[j][i], testSensValues[j], relTol, absTol);
          comparisons++;
        }
      }
    }
  }

  // Repeat comparisons for Dakota runs
  int numDakotaTests = testParams.get<int>("Number of Dakota Comparisons");
  if (numDakotaTests > 0 && drdv != NULL) {

    if (numDakotaTests > drdv->length()) failures += 100000;
    else { // do comparisons
      // Read accepted test results
      Teuchos::Array<double> testValues =
         Teuchos::getArrayFromStringParameter<double> (testParams,
                                "Dakota Test Values", numDakotaTests, true);

      for (int i=0; i<numDakotaTests; i++) {
        failures += scaledCompare((*drdv)[i], testValues[i], relTol, absTol);
        comparisons++;
      }
    }
  }

  // Repeat comparisons for Piro Analysis runs
  int numPiroTests = testParams.get<int>("Number of Piro Analysis Comparisons");
  if (numPiroTests > 0 && tvec != Teuchos::null) {

     // Create indexable thyra vector
      ::Thyra::DetachedVectorView<double> p(tvec);

    if (numPiroTests > p.subDim()) failures += 300000;
    else { // do comparisons
      // Read accepted test results
      Teuchos::Array<double> testValues =
         Teuchos::getArrayFromStringParameter<double> (testParams,
                                "Piro Analysis Test Values", numPiroTests, true);

      for (int i=0; i<numPiroTests; i++) {
        failures += scaledCompare(p[i], testValues[i], relTol, absTol);
        comparisons++;
      }
    }
  }

  // Repeat comparisons for SG expansions
  int numSGTests = testParams.get<int>("Number of Stochastic Galerkin Comparisons");
  if (numSGTests > 0 && g_sg != Teuchos::null) {
    if (numSGTests > (*g_sg)[0].MyLength()) failures += 10000;
    else {
      Teuchos::Array<double> testSGValues;
      for (int i=0; i<numSGTests; i++) {
	std::stringstream tsgv;
	tsgv << "Stochastic Galerkin Expansion Test Values " << i;
	testSGValues = Teuchos::getArrayFromStringParameter<double> (
	  testParams, tsgv.str(), g_sg->size(), true);
	for (int j=0; j<g_sg->size(); j++) {
	  failures += 
	    scaledCompare((*g_sg)[j][i], testSGValues[j], relTol, absTol);
          comparisons++;
        }
      }
    }
  }

  // Store failures in param list (this requires mutable appParams!)
  testParams.set("Number of Failures", failures);
  testParams.set("Number of Comparisons Attempted", comparisons);
  *out << "\nCheckTestResults: Number of Comparisons Attempted = "
       << comparisons << endl;
  return failures;
}

int Albany::SolverFactory::scaledCompare(double x1, double x2, double relTol, double absTol) const
{
  double diff = fabs(x1 - x2) / (0.5*fabs(x1) + 0.5*fabs(x2) + fabs(absTol));

  if (diff < relTol) return 0; //pass
  else               return 1; //fail
}


void Albany::SolverFactory::setSolverParamDefaults(ParameterList* appParams_)
{
    // Set the nonlinear solver method
    ParameterList& noxParams = appParams_->sublist("NOX");
    noxParams.set("Nonlinear Solver", "Line Search Based");

    // Set the printing parameters in the "Printing" sublist
    ParameterList& printParams = noxParams.sublist("Printing");
    printParams.set("MyPID", Comm->MyPID()); 
    printParams.set("Output Precision", 3);
    printParams.set("Output Processor", 0);
    printParams.set("Output Information", 
		    NOX::Utils::OuterIteration + 
		    NOX::Utils::OuterIterationStatusTest + 
		    NOX::Utils::InnerIteration +
		    NOX::Utils::Parameters + 
		    NOX::Utils::Details + 
		    NOX::Utils::LinearSolverDetails +
		    NOX::Utils::Warning + 
		    NOX::Utils::Error);

    // Sublist for line search 
    ParameterList& searchParams = noxParams.sublist("Line Search");
    searchParams.set("Method", "Full Step");

    // Sublist for direction
    ParameterList& dirParams = noxParams.sublist("Direction");
    dirParams.set("Method", "Newton");
    ParameterList& newtonParams = dirParams.sublist("Newton");
    newtonParams.set("Forcing Term Method", "Constant");

    // Sublist for linear solver for the Newton method
    ParameterList& lsParams = newtonParams.sublist("Linear Solver");
    lsParams.set("Aztec Solver", "GMRES");  
    lsParams.set("Max Iterations", 43);  
    lsParams.set("Tolerance", 1e-4); 
    lsParams.set("Output Frequency", 20);
    lsParams.set("Preconditioner", "Ifpack");

    // Sublist for status tests
    ParameterList& statusParams = noxParams.sublist("Status Tests");
    statusParams.set("Test Type", "Combo");
    statusParams.set("Combo Type", "OR");
    statusParams.set("Number of Tests", 2);
    ParameterList& normF = statusParams.sublist("Test 0");
    normF.set("Test Type", "NormF");
    normF.set("Tolerance", 1.0e-8);
    normF.set("Norm Type", "Two Norm");
    normF.set("Scale Type", "Unscaled");
    ParameterList& maxiters = statusParams.sublist("Test 1");
    maxiters.set("Test Type", "MaxIters");
    maxiters.set("Maximum Iterations", 10);

}

RCP<const ParameterList>
Albany::SolverFactory::getValidAppParameters() const
{
  RCP<ParameterList> validPL = rcp(new ParameterList("ValidAppParams"));;
  validPL->sublist("Problem",            false, "Problem sublist");
  validPL->sublist("Discretization",     false, "Discretization sublist");
  validPL->sublist("Quadrature",         false, "Quadrature sublist");
  validPL->sublist("Regression Results", false, "Regression Results sublist");
  validPL->sublist("VTK",                false, "VTK sublist");
  validPL->sublist("Rythmos",            false, "Rythmos sublist");
  validPL->sublist("LOCA",               false, "LOCA sublist");
  validPL->sublist("NOX",                false, "NOX sublist");
  validPL->sublist("Analysis",           false, "Analysis sublist");

  validPL->set<string>("Jacobian Operator", "Have Jacobian", "Flag to alloe Matrix-Free specification in Piro");
  validPL->set<double>("Matrix-Free Perturbation", 3.0e-7, "delta in matrix-free formula");

  return validPL;
}

RCP<const ParameterList>
Albany::SolverFactory::getValidRegressionResultsParameters() const
{
  RCP<ParameterList> validPL = rcp(new ParameterList("ValidRegressionParams"));;
  std::string ta; // string to be converted to teuchos array

  validPL->set<double>("Relative Tolerance", 1.0e-4,
          "Relative Tolerance used in regression testing");
  validPL->set<double>("Absolute Tolerance", 1.0e-8,
          "Absolute Tolerance used in regression testing");

  validPL->set<int>("Number of Comparisons", 0,
          "Number of responses to regress against");
  validPL->set<std::string>("Test Values", ta,
          "Array of regression values for responses");

  validPL->set<int>("Number of Sensitivity Comparisons", 0,
          "Number of sensitivity vectors to regress against");

  const int maxSensTests=10;
  for (int i=0; i<maxSensTests; i++) {
    std::stringstream stv, sd;
    stv << "Sensitivity Test Values " << i;
    sd <<  "Array of regression values for Sensitivities w.r.t parameter " << i;
    validPL->set<std::string>(stv.str(), ta, sd.str());
  }

  validPL->set<int>("Number of Dakota Comparisons", 0,
          "Number of paramters from Dakota runs to regress against");
  validPL->set<std::string>("Dakota Test Values", ta,
          "Array of regression values for final parameters from Dakota runs");

  validPL->set<int>("Number of Piro Analysis Comparisons", 0,
          "Number of paramters from Analysis to regress against");
  validPL->set<std::string>("Piro Analysis Test Values", ta,
          "Array of regression values for final parameters from Analysis runs");

  validPL->set<int>("Number of Stochastic Galerkin Comparisons", 0,
          "Number of stochastic Galerkin expansions to regress against");

  const int maxSGTests=10;
  for (int i=0; i<maxSGTests; i++) {
    std::stringstream sgtv, sgd;
    sgtv << "Stochastic Galerkin Expansion Test Values " << i;
    sgd <<  "Array of regression values for stochastic Galerkin expansions " 
	<< i;
    validPL->set<std::string>(sgtv.str(), ta, sgd.str());
  }

  // These two are typically not set on input, just output.
  validPL->set<int>("Number of Failures", 0,
     "Output information from regression tests reporting number of failed tests");
  validPL->set<int>("Number of Comparisons", 0,
     "Output information from regression tests reporting number of comparisons attempted");

  return validPL;
}

RCP<const ParameterList>
Albany::SolverFactory::getValidParameterParameters() const
{
  RCP<ParameterList> validPL = rcp(new ParameterList("ValidParameterParams"));;

  validPL->set<int>("Number", 0);
  const int maxParameters = 100;
  for (int i=0; i<maxParameters; i++) {
    std::ostringstream ss;
    ss << "Parameter " << i;
    validPL->set<std::string>(ss.str(), "");
  }
  return validPL;
}

RCP<const ParameterList>
Albany::SolverFactory::getValidResponseParameters() const
{
  RCP<ParameterList> validPL = rcp(new ParameterList("ValidResponseParams"));;

  validPL->set<int>("Number", 0);
  const int maxParameters = 100;
  for (int i=0; i<maxParameters; i++) {
    std::ostringstream ss;
    ss << "Response " << i;
    validPL->set<std::string>(ss.str(), "");
  }
  return validPL;
}
