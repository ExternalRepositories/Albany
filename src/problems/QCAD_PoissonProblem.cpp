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


#include "QCAD_PoissonProblem.hpp"
#include "QCAD_MaterialDatabase.hpp"
#include "Albany_BoundaryFlux1DResponseFunction.hpp"
#include "Albany_SolutionAverageResponseFunction.hpp"
#include "Albany_SolutionTwoNormResponseFunction.hpp"
#include "Albany_InitialCondition.hpp"
#include "Albany_Utils.hpp"

QCAD::PoissonProblem::
PoissonProblem( const Teuchos::RCP<Teuchos::ParameterList>& params_,
             const Teuchos::RCP<ParamLib>& paramLib_,
             const int numDim_) :
  Albany::AbstractProblem(params_, paramLib_, 1),
  haveSource(false),
  numDim(numDim_)
{
  if (numDim==1) periodic = params->get("Periodic BC", false);
  else           periodic = false;
  if (periodic) *out <<" Periodic Boundary Conditions being used." <<std::endl;

  haveSource =  params->isSublist("Poisson Source");

  TEST_FOR_EXCEPTION(params->isSublist("Source Functions"), Teuchos::Exceptions::InvalidParameter,
		     "\nError! Poisson problem does not parse Source Functions sublist\n" 
                     << "\tjust Poisson Source sublist " << std::endl);

  //get length scale for problem (length unit for in/out mesh)
  length_unit_in_m = 1e-6; //default to um
  if(params->isType<double>("LengthUnitInMeters"))
    length_unit_in_m = params->get<double>("LengthUnitInMeters");

  temperature = 300; //default to 300K
  if(params->isType<double>("Temperature"))
    temperature = params->get<double>("Temperature");

  mtrlDbFilename = "materials.xml";
  if(params->isType<string>("MaterialDB Filename"))
    mtrlDbFilename = params->get<string>("MaterialDB Filename");

  //Schrodinger coupling
  nEigenvectorsToInputFromStates = 0;
  bUseSchrodingerSource = false;
  eigenvalFilename = "evals.txtdump";
  if(params->isSublist("Schrodinger Coupling")) {
    Teuchos::ParameterList& cList = params->sublist("Schrodinger Coupling");
    if(cList.isType<bool>("Schrodinger source in quantum blocks"))
      bUseSchrodingerSource = cList.get<bool>("Schrodinger source in quantum blocks");

    if(cList.isType<int>("Eigenvectors from States"))
      nEigenvectorsToInputFromStates = cList.get<int>("Eigenvectors from States");

    if(cList.isType<string>("Eigenvalues file"))
      eigenvalFilename = cList.get<string>("Eigenvalues file");
  }

  std::cout << "Length unit = " << length_unit_in_m << " meters" << endl;

  // neq=1 set in AbstractProblem constructor
  dofNames.resize(neq);
  dofNames[0] = "Phi";

  // STATE OUTPUT
  nstates = 8;
  nstates += nEigenvectorsToInputFromStates*2; //Re and Im parts (input @ nodes)
  nstates += nEigenvectorsToInputFromStates*2; //Re and Im parts (output @ qps)
}

QCAD::PoissonProblem::
~PoissonProblem()
{
}

void
QCAD::PoissonProblem::
buildProblem(
    const int worksetSize,
    Albany::StateManager& stateMgr,
    const Albany::AbstractDiscretization& disc,
    std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses)
{
  /* Construct All Phalanx Evaluators */
  constructEvaluators(worksetSize, disc.getCubatureDegree(), disc.getCellTopologyData(), stateMgr);
  constructDirichletEvaluators(disc.getNodeSetIDs());
 
  const Epetra_Map& dofMap = *(disc.getMap());
  int left_node = dofMap.MinAllGID();
  int right_node = dofMap.MaxAllGID();

  // Build response functions
  Teuchos::ParameterList& responseList = params->sublist("Response Functions");
  int num_responses = responseList.get("Number", 0);
  responses.resize(num_responses);
  for (int i=0; i<num_responses; i++) 
  {
     std::string name = responseList.get(Albany::strint("Response",i), "??");

     if (name == "Boundary Flux 1D" && numDim==1) 
     {
       // Need real size, not 1.0
       double h =  1.0 / (dofMap.NumGlobalElements() - 1);
       responses[i] =
         Teuchos::rcp(new Albany::BoundaryFlux1DResponseFunction(left_node,
                                                         right_node,
                                                         0, 1, h,
                                                         dofMap));
     }

     else if (name == "Solution Average")
       responses[i] = Teuchos::rcp(new Albany::SolutionAverageResponseFunction());

     else if (name == "Solution Two Norm")
       responses[i] = Teuchos::rcp(new Albany::SolutionTwoNormResponseFunction());

     else 
     {
       TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
                          std::endl <<
                          "Error!  Unknown response function " << name <<
                          "!" << std::endl << "Supplied parameter list is " <<
                          std::endl << responseList);
     }

  }
}


void
QCAD::PoissonProblem::constructEvaluators(
       const int worksetSize, const int cubDegree,
       const CellTopologyData& ctd,
       Albany::StateManager& stateMgr)
{
   using Teuchos::RCP;
   using Teuchos::rcp;
   using Teuchos::ParameterList;
   using PHX::DataLayout;
   using PHX::MDALayout;
   using std::vector;
   using std::map;
   using PHAL::FactoryTraits;
   using PHAL::AlbanyTraits;

   RCP<shards::CellTopology> cellType = rcp(new shards::CellTopology (&ctd));
   RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
     intrepidBasis = this->getIntrepidBasis(ctd);

   const int numNodes = intrepidBasis->getCardinality();

   Intrepid::DefaultCubatureFactory<RealType> cubFactory;
   RCP <Intrepid::Cubature<RealType> > cubature = cubFactory.create(*cellType, cubDegree);

   const int numQPts = cubature->getNumPoints();
   const int numVertices = cellType->getVertexCount();

   *out << "Field Dimensions: Workset=" << worksetSize 
        << ", Vertices= " << numVertices
        << ", Nodes= " << numNodes
        << ", QuadPts= " << numQPts
        << ", Dim= " << numDim << endl;

   // Parser will build parameter list that determines the field
   // evaluators to build
   map<string, RCP<ParameterList> > evaluators_to_build;

   RCP<DataLayout> node_scalar = rcp(new MDALayout<Cell,Node>(worksetSize,numNodes));
   RCP<DataLayout> qp_scalar = rcp(new MDALayout<Cell,QuadPoint>(worksetSize,numQPts));

   RCP<DataLayout> node_vector = rcp(new MDALayout<Cell,Node,Dim>(worksetSize,numNodes,numDim));
   RCP<DataLayout> qp_vector = rcp(new MDALayout<Cell,QuadPoint,Dim>(worksetSize,numQPts,numDim));

   RCP<DataLayout> vertices_vector = 
     rcp(new MDALayout<Cell,Vertex, Dim>(worksetSize,numVertices,numDim));
   // Basis functions, Basis function gradient
   RCP<DataLayout> node_qp_scalar =
     rcp(new MDALayout<Cell,Node,QuadPoint>(worksetSize,numNodes, numQPts));
   RCP<DataLayout> node_qp_vector =
     rcp(new MDALayout<Cell,Node,QuadPoint,Dim>(worksetSize,numNodes, numQPts,numDim));

   RCP<DataLayout> dummy = rcp(new MDALayout<Dummy>(0));


   // Create Material Database
   RCP<QCAD::MaterialDatabase> materialDB = rcp(new QCAD::MaterialDatabase(mtrlDbFilename));

  { // Gather Solution
   RCP< vector<string> > dof_names = rcp(new vector<string>(neq));
     (*dof_names)[0] = "Potential";

    RCP<ParameterList> p = rcp(new ParameterList);
    int type = FactoryTraits<AlbanyTraits>::id_gather_solution;
    p->set<int>("Type", type);
    p->set< RCP< vector<string> > >("Solution Names", dof_names);
    p->set< RCP<DataLayout> >("Data Layout", node_scalar);

    // Poisson solve does not have transient terms
    p->set<bool>("Disable Transient", true);

    evaluators_to_build["Gather Solution"] = p;
  }

  { // Gather Coordinate Vector
    RCP<ParameterList> p = rcp(new ParameterList("Poisson Gather Coordinate Vector"));
    int type = FactoryTraits<AlbanyTraits>::id_gather_coordinate_vector;
    p->set<int>                ("Type", type);
    // Input: Periodic BC flag
    p->set<bool>("Periodic BC", false);

    // Output:: Coordindate Vector at vertices
    p->set< RCP<DataLayout> >  ("Coordinate Data Layout",  vertices_vector);
    p->set< string >("Coordinate Vector Name", "Coord Vec");
    evaluators_to_build["Gather Coordinate Vector"] = p;
  }

  { // Map To Physical Frame: Interpolate X, Y to QuadPoints
    RCP<ParameterList> p = rcp(new ParameterList("Poisson 1D Map To Physical Frame"));

    int type = FactoryTraits<AlbanyTraits>::id_map_to_physical_frame;
    p->set<int>   ("Type", type);

    // Input: X, Y at vertices
    p->set< string >("Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Coordinate Data Layout", vertices_vector);

    p->set<RCP <Intrepid::Cubature<RealType> > >("Cubature", cubature);
    p->set<RCP<shards::CellTopology> >("Cell Type", cellType);

    // Output: X, Y at Quad Points (same name as input)
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    evaluators_to_build["Map To Physical Frame"] = p;
  }

  { // Compute Basis Functions
    RCP<ParameterList> p = rcp(new ParameterList("Poisson Compute Basis Functions"));

    int type = FactoryTraits<AlbanyTraits>::id_compute_basis_functions;
    p->set<int>   ("Type", type);

    // Inputs: X, Y at nodes, Cubature, and Basis
    p->set<string>("Coordinate Vector Name","Coord Vec");
    p->set< RCP<DataLayout> >("Coordinate Data Layout", vertices_vector);
    p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", cubature);

    p->set< RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >
        ("Intrepid Basis", intrepidBasis);

    p->set<RCP<shards::CellTopology> >("Cell Type", cellType);

    // Outputs: BF, weightBF, Grad BF, weighted-Grad BF, all in physical space
    p->set<string>("Weights Name",          "Weights");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);
    p->set<string>("BF Name",          "BF");
    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    p->set<string>("Gradient BF Name",          "Grad BF");
    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);

    evaluators_to_build["Compute Basis Functions"] = p;
  }


  { // Permittivity
    RCP<ParameterList> p = rcp(new ParameterList);

    int type = FactoryTraits<AlbanyTraits>::id_qcad_permittivity;
    p->set<int>("Type", type);

    p->set<string>("QP Variable Name", "Permittivity");
    p->set<string>("Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    p->set< RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Permittivity");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    p->set< RCP<QCAD::MaterialDatabase> >("MaterialDB", materialDB);

    evaluators_to_build["Permittivity"] = p;
  }

  { // DOF: Interpolate nodal Potential values to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Poisson DOFInterpolation Potential"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Potential");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("BF Name", "BF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    // Output (assumes same Name as input)
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    evaluators_to_build["DOF Potential"] = p;
  }

  { // DOF: Interpolate nodal Potential gradients to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Poisson DOFInterpolation Potential Grad"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_grad_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Potential");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("Gradient BF Name", "Grad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);

    // Output
    p->set<string>("Gradient Variable Name", "Potential Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    evaluators_to_build["DOF Grad Potential"] = p;
  }

  if (haveSource) 
  { // Source
    RCP<ParameterList> p = rcp(new ParameterList);

    int type = FactoryTraits<AlbanyTraits>::id_qcad_poisson_source;
    p->set<int>("Type", type);

    //Input
    p->set< string >("Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    p->set<string>("Variable Name", "Potential");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);

    Teuchos::ParameterList& paramList = params->sublist("Poisson Source");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    //Output
    p->set<string>("Source Name", "Poisson Source");

    //Global Problem Parameters
    p->set<double>("Length unit in m", length_unit_in_m);
    p->set<double>("Temperature", temperature);
    p->set< RCP<QCAD::MaterialDatabase> >("MaterialDB", materialDB);

    // Schrodinger coupling
    p->set<bool>("Use Schrodinger source", bUseSchrodingerSource);
    p->set<string>("Eigenvalues file", eigenvalFilename);
    p->set<int>("Schrodinger eigenvectors", nEigenvectorsToInputFromStates);
    p->set<string>("Eigenvector state name root", "Evec");

    evaluators_to_build["Poisson Source"] = p;

    // EIGENSTATE INPUT from states
    if( nEigenvectorsToInputFromStates > 0 ) {
      int ilsf = FactoryTraits<AlbanyTraits>::id_loadstatefield;
      char evecStateName[100]; char evecFieldName[100]; char evalName[100];
      for( int k = 0; k < nEigenvectorsToInputFromStates; k++) {
        sprintf(evecStateName,"Eigenvector_Re%d",k);
        sprintf(evecFieldName,"Evec_Re%d",k);
        sprintf(evalName,"Input Evec_Re%d",k);
        evaluators_to_build[evecStateName] =
          stateMgr.registerStateVariable(evecStateName, node_scalar, dummy, ilsf, "zero", evecFieldName);

        sprintf(evecStateName,"Eigenvector_Im%d",k);
        sprintf(evecFieldName,"Evec_Im%d",k);
        sprintf(evalName,"Input Evec_Im%d",k);
        evaluators_to_build[evecStateName] =
          stateMgr.registerStateVariable(evecStateName, node_scalar, dummy, ilsf, "zero", evecFieldName);
      }
    }


    // STATE OUTPUT
    int issf = FactoryTraits<AlbanyTraits>::id_savestatefield;
    evaluators_to_build["Save Charge Density"] =
      stateMgr.registerStateVariable("Charge Density", qp_scalar, dummy, issf);
    evaluators_to_build["Save Electron Density"] =
      stateMgr.registerStateVariable("Electron Density", qp_scalar, dummy, issf);
    evaluators_to_build["Save Hole Density"] =
      stateMgr.registerStateVariable("Hole Density", qp_scalar, dummy, issf);
    evaluators_to_build["Save Electric Potential"] =
      stateMgr.registerStateVariable("Electric Potential", qp_scalar, dummy, issf);
    evaluators_to_build["Save Ionized Donor"] =
      stateMgr.registerStateVariable("Ionized Donor", qp_scalar, dummy, issf);
    evaluators_to_build["Save Ionized Acceptor"] =
      stateMgr.registerStateVariable("Ionized Acceptor", qp_scalar, dummy, issf);
    evaluators_to_build["Save Condution Band"] =
      stateMgr.registerStateVariable("Conduction Band", qp_scalar, dummy, issf);
    evaluators_to_build["Save Valence Band"] =
      stateMgr.registerStateVariable("Valence Band", qp_scalar, dummy, issf);
  }

  // Interpolate Input Eigenvectors (if any) to quad points
  if( nEigenvectorsToInputFromStates > 0 ) {
    char buf[100];
    
    for( int k = 0; k < nEigenvectorsToInputFromStates; k++)
    { 
      // DOF: Interpolate nodal Eigenvector values to quad points
      RCP<ParameterList> p;
      int type;

      //REAL PART
      sprintf(buf, "Poisson Eigenvector Re %d interpolate to qps", k);
      p = rcp(new ParameterList(buf));

      type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
      p->set<int>   ("Type", type);

      // Input
      sprintf(buf, "Evec_Re%d", k);
      p->set<string>("Variable Name", buf);
      p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

      p->set<string>("BF Name", "BF");
      p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

      // Output (assumes same Name as input)
      p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

      sprintf(buf, "Eigenvector Re %d interpolate to qps", k);
      evaluators_to_build[buf] = p;


      //IMAGINARY PART
      sprintf(buf, "Eigenvector Im %d interpolate to qps", k);
      p = rcp(new ParameterList(buf));

      type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
      p->set<int>   ("Type", type);

      // Input
      sprintf(buf, "Evec_Im%d", k);
      p->set<string>("Variable Name", buf);
      p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

      p->set<string>("BF Name", "BF");
      p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

      // Output (assumes same Name as input)
      p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

      sprintf(buf, "Eigenvector Im %d interpolate to qps", k);
      evaluators_to_build[buf] = p;


      //Save evaluators - to debug whether evecs has been interpolated to qps correctly
      char saveName[100];
      int issf = FactoryTraits<AlbanyTraits>::id_savestatefield;
      sprintf(saveName, "Save QP Evector Re %d", k);
      sprintf(buf, "Evec_Re%d", k);
      evaluators_to_build[saveName] =
	stateMgr.registerStateVariable(buf, qp_scalar, dummy, issf);

      sprintf(saveName, "Save QP Evector Im %d", k);
      sprintf(buf, "Evec_Im%d", k);
      evaluators_to_build[saveName] =
      	stateMgr.registerStateVariable(buf, qp_scalar, dummy, issf);
    }
  }

  { // Potential Resid
    RCP<ParameterList> p = rcp(new ParameterList("Potential Resid"));

    int type = FactoryTraits<AlbanyTraits>::id_qcad_poisson_resid;
    p->set<int>("Type", type);

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);
    p->set<string>("QP Variable Name", "Potential");

    p->set<string>("QP Time Derivative Variable Name", "Potential_dot");

    p->set<bool>("Have Source", haveSource);
    p->set<string>("Source Name", "Poisson Source");

    p->set<string>("Permittivity Name", "Permittivity");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    p->set<string>("Gradient QP Variable Name", "Potential Gradient");
    p->set<string>("Flux QP Variable Name", "Potential Flux");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);

    //Output
    p->set<string>("Residual Name", "Potential Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", node_scalar);

    evaluators_to_build["Poisson Resid"] = p;
  }

  { // Scatter Residual
   RCP< vector<string> > resid_names = rcp(new vector<string>(neq));
     (*resid_names)[0] = "Potential Residual";

    RCP<ParameterList> p = rcp(new ParameterList);
    int type = FactoryTraits<AlbanyTraits>::id_scatter_residual;
    p->set<int>("Type", type);
    p->set< RCP< vector<string> > >("Residual Names", resid_names);

    p->set< RCP<DataLayout> >("Dummy Data Layout", dummy);
    p->set< RCP<DataLayout> >("Data Layout", node_scalar);

    evaluators_to_build["Scatter Residual"] = p;
  }

   // Build Field Evaluators for each evaluation type
   PHX::EvaluatorFactory<AlbanyTraits,FactoryTraits<AlbanyTraits> > factory;
   RCP< vector< RCP<PHX::Evaluator_TemplateManager<AlbanyTraits> > > >
     evaluators;
   evaluators = factory.buildEvaluators(evaluators_to_build);

   // Create a FieldManager
   fm = Teuchos::rcp(new PHX::FieldManager<AlbanyTraits>);

   // Register all Evaluators
   PHX::registerEvaluators(evaluators, *fm);

   PHX::Tag<AlbanyTraits::Residual::ScalarT> res_tag("Scatter", dummy);
   fm->requireField<AlbanyTraits::Residual>(res_tag);
   PHX::Tag<AlbanyTraits::Jacobian::ScalarT> jac_tag("Scatter", dummy);
   fm->requireField<AlbanyTraits::Jacobian>(jac_tag);
   PHX::Tag<AlbanyTraits::Tangent::ScalarT> tan_tag("Scatter", dummy);
   fm->requireField<AlbanyTraits::Tangent>(tan_tag);
   PHX::Tag<AlbanyTraits::SGResidual::ScalarT> sgres_tag("Scatter", dummy);
   fm->requireField<AlbanyTraits::SGResidual>(sgres_tag);
   PHX::Tag<AlbanyTraits::SGJacobian::ScalarT> sgjac_tag("Scatter", dummy);
   fm->requireField<AlbanyTraits::SGJacobian>(sgjac_tag);
   PHX::Tag<AlbanyTraits::MPResidual::ScalarT> mpres_tag("Scatter", dummy);
   fm->requireField<AlbanyTraits::MPResidual>(mpres_tag);
   PHX::Tag<AlbanyTraits::MPJacobian::ScalarT> mpjac_tag("Scatter", dummy);
   fm->requireField<AlbanyTraits::MPJacobian>(mpjac_tag);


   const Albany::StateManager::RegisteredStates& reg = stateMgr.getRegisteredStates();
   Albany::StateManager::RegisteredStates::const_iterator st = reg.begin();
   while (st != reg.end()) {
     if( (st->first).find("Eigenvector") == 0 ) { st++; continue; } //skip "EigenvectorX" states since they're inputs
     PHX::Tag<AlbanyTraits::Residual::ScalarT> res_out_tag(st->first, dummy);
     fm->requireField<AlbanyTraits::Residual>(res_out_tag);
     st++;
   }
}

Teuchos::RCP<const Teuchos::ParameterList>
QCAD::PoissonProblem::getValidProblemParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
    this->getGenericProblemParams("ValidPoissonProblemParams");

  if (numDim==1)
    validPL->set<bool>("Periodic BC", false, "Flag to indicate periodic BC for 1D problems");
  validPL->sublist("Permittivity", false, "");
  validPL->sublist("Poisson Source", false, "");
  validPL->set<double>("LengthUnitInMeters",1e-6,"Length unit in meters");
  validPL->set<double>("Temperature",300,"Temperature in Kelvin");
  validPL->set<string>("MaterialDB Filename","materials.xml","Filename of material database xml file");

  validPL->sublist("Schrodinger Coupling", false, "");
  validPL->sublist("Schrodinger Coupling").set<bool>("Schrodinger source in quantum blocks",false,"Use eigenvector data to compute charge distribution within quantum blocks");
  validPL->sublist("Schrodinger Coupling").set<int>("Eigenvectors from States",0,"Number of eigenvectors to use for quantum region source");
  validPL->sublist("Schrodinger Coupling").set<string>("Eigenvalues file","evals.txtdump","File specifying eigevalues, output by Schrodinger problem");

  return validPL;
}

