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
#include "Albany_SolutionAverageResponseFunction.hpp"
#include "Albany_SolutionTwoNormResponseFunction.hpp"
#include "Albany_InitialCondition.hpp"
#include "Albany_Utils.hpp"

QCAD::PoissonProblem::
PoissonProblem( const Teuchos::RCP<Teuchos::ParameterList>& params_,
		const Teuchos::RCP<ParamLib>& paramLib_,
		const int numDim_,
		const Teuchos::RCP<const Epetra_Comm>& comm_) :
  Albany::AbstractProblem(params_, paramLib_, 1),
  comm(comm_),
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
  nEigenvectors = 0;
  bUseSchrodingerSource = false;
  if(params->isSublist("Schrodinger Coupling")) {
    Teuchos::ParameterList& cList = params->sublist("Schrodinger Coupling");
    if(cList.isType<bool>("Schrodinger source in quantum blocks"))
      bUseSchrodingerSource = cList.get<bool>("Schrodinger source in quantum blocks");
    std::cout << "bSchod in quantum = " << bUseSchrodingerSource << std::endl;
    
    if(bUseSchrodingerSource && cList.isType<int>("Eigenvectors from States"))
      nEigenvectors = cList.get<int>("Eigenvectors from States");
  }

  std::cout << "Length unit = " << length_unit_in_m << " meters" << endl;

  // neq=1 set in AbstractProblem constructor
  dofNames.resize(neq);
  dofNames[0] = "Phi";
}

QCAD::PoissonProblem::
~PoissonProblem()
{
}

void
QCAD::PoissonProblem::
buildProblem(
    const Albany::MeshSpecsStruct& meshSpecs,
    Albany::StateManager& stateMgr,
    std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses)
{
  /* Construct All Phalanx Evaluators */
  constructEvaluators(meshSpecs, stateMgr, responses);
  constructDirichletEvaluators(meshSpecs.nsNames);
}


void
QCAD::PoissonProblem::constructEvaluators(
       const Albany::MeshSpecsStruct& meshSpecs,
       Albany::StateManager& stateMgr,
       std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses)
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

   RCP<shards::CellTopology> cellType = rcp(new shards::CellTopology (&meshSpecs.ctd));
   RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
     intrepidBasis = this->getIntrepidBasis(meshSpecs.ctd);

   const int numNodes = intrepidBasis->getCardinality();
   const int worksetSize = meshSpecs.worksetSize;

   Intrepid::DefaultCubatureFactory<RealType> cubFactory;
   RCP <Intrepid::Cubature<RealType> > cubature = cubFactory.create(*cellType, meshSpecs.cubatureDegree);

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
   RCP<DataLayout> cell_scalar = rcp(new MDALayout<Cell,QuadPoint>(worksetSize,1));

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
   RCP<DataLayout> shared_param = rcp(new MDALayout<Dim>(1));

   // Create Material Database
   RCP<QCAD::MaterialDatabase> materialDB = rcp(new QCAD::MaterialDatabase(mtrlDbFilename, comm));

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

  { // Gather Eigenvectors
    RCP<ParameterList> p = rcp(new ParameterList);
    int type = FactoryTraits<AlbanyTraits>::id_gather_eigenvectors;
    p->set<int>("Type", type);
    p->set<string>("Eigenvector field name root", "Evec");
    p->set<int>("Number of eigenvectors", nEigenvectors);
    p->set< RCP<DataLayout> >("Data Layout", node_scalar);

    evaluators_to_build["Gather Eigenvectors"] = p;
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

  { // Temperature shared parameter (single scalar value, not spatially varying)
    RCP<ParameterList> p = rcp(new ParameterList);

    int type = FactoryTraits<AlbanyTraits>::id_sharedparameter;
    p->set<int>("Type", type);

    p->set<string>("Parameter Name", "Temperature");
    p->set<double>("Parameter Value", temperature);
    p->set< RCP<DataLayout> >("Data Layout", shared_param);
    p->set< RCP<ParamLib> >("Parameter Library", paramLib);

    evaluators_to_build["Temperature"] = p;
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
    p->set<string>("Temperature Name", "Temperature");
    p->set< RCP<DataLayout> >("Shared Param Data Layout", shared_param);
    p->set< RCP<QCAD::MaterialDatabase> >("MaterialDB", materialDB);

    // Schrodinger coupling
    p->set<bool>("Use Schrodinger source", bUseSchrodingerSource);
    p->set<int>("Schrodinger eigenvectors", nEigenvectors);
    p->set<string>("Eigenvector field name root", "Evec");

    evaluators_to_build["Poisson Source"] = p;
  }

  // Interpolate Input Eigenvectors (if any) to quad points
  char buf[100];  
  for( int k = 0; k < nEigenvectors; k++)
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


   // Parameters for Response Evaluators
   //  Iterate through list of responses (from input xml file).  For each, create a response
   //  function and possibly a parameter list to construct a response evaluator.
   Teuchos::ParameterList& responseList = params->sublist("Response Functions");
   int num_responses = responseList.get("Number", 0);
   responses.resize(num_responses);

   map<string, RCP<ParameterList> > response_evaluators_to_build;
   vector<string> responseIDs_to_require;

   for (int i=0; i<num_responses; i++) 
   {
     std::string responseID = Albany::strint("Response",i);
     std::string responseParamsID = Albany::strint("ResponseParams",i);
     std::string name = responseList.get(responseID, "??");

     if (name == "See List" && responseList.isSublist(responseParamsID) ) {

       Teuchos::RCP<Albany::EvaluatedResponseFunction> 
	 evResponse = Teuchos::rcp(new Albany::EvaluatedResponseFunction());
       responses[i] = evResponse;

       Teuchos::ParameterList& responseParams = responseList.sublist(responseParamsID);
       std::string type = responseParams.get("Type", "??");

       // Common parameters to all response evaluators
       RCP<ParameterList> p = rcp(new ParameterList);
       p->set<string>("Response ID", responseID);
       p->set<int>   ("Response Index", i);
       p->set< Teuchos::RCP<Albany::EvaluatedResponseFunction> >("Response Function", evResponse);
       p->set<Teuchos::ParameterList*>("Parameter List", &responseParams);
       p->set< RCP<DataLayout> >("Dummy Data Layout", dummy);

       // Parameters specific to the particular type of response evaluator
       if (type == "Field Integral")
       { 
         int type = FactoryTraits<AlbanyTraits>::id_qcad_response_fieldintegral;
         p->set<int>("Type", type);
         p->set<string>("Weights Name",   "Weights");
         p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);
         p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);
         
         //Global Problem Parameters
         p->set<double>("Length unit in m", length_unit_in_m);
       }

       else if (type == "Field Value")
       { 
         int type = FactoryTraits<AlbanyTraits>::id_qcad_response_fieldvalue;
         p->set<int>("Type", type);
         p->set<string>("Coordinate Vector Name", "Coord Vec");
         p->set<string>("Weights Name",   "Weights");
         p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);
         p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);
       }

       else if (type == "Save Field")
       { 
         int type = FactoryTraits<AlbanyTraits>::id_qcad_response_savefield;
         p->set<int>("Type", type);
         p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);
         p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);
         p->set< RCP<DataLayout> >("Cell Scalar Data Layout", cell_scalar);
	 p->set< Albany::StateManager* >("State Manager Ptr", &stateMgr );
         p->set<string>("Weights Name",   "Weights");
       }

       else {
         TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
           std::endl <<
           "Error!  Unknown evaluated response type " << type <<
           "!" << std::endl << "Supplied parameter list is " <<
           std::endl << responseList);
       }

       response_evaluators_to_build[responseID] = p;
       responseIDs_to_require.push_back(responseID);
     }

     else // Response<i> is not a sublist, so process by just building a response function
     { 
       if (name == "Solution Average")
         responses[i] = Teuchos::rcp(new Albany::SolutionAverageResponseFunction());

       else if (name == "Solution Two Norm")
         responses[i] = Teuchos::rcp(new Albany::SolutionTwoNormResponseFunction());

       else {
         TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
           std::endl <<
           "Error!  Unknown response function " << name <<
           "!" << std::endl << "Supplied parameter list is " <<
           std::endl << responseList);
       }

     }

   } // end of loop over responses

   // Build Response Evaluators for each evaluation type
   RCP< vector< RCP<PHX::Evaluator_TemplateManager<AlbanyTraits> > > >
     response_evaluators;
   
   response_evaluators_to_build.insert(evaluators_to_build.begin(), evaluators_to_build.end());
   response_evaluators = factory.buildEvaluators(response_evaluators_to_build);
   //response_evaluators->insert(response_evaluators->begin(), evaluators->begin(), evaluators->end());

   // Create a Response FieldManager
   rfm = Teuchos::rcp(new PHX::FieldManager<AlbanyTraits>);

   // Register all Evaluators
   PHX::registerEvaluators(response_evaluators, *rfm);

   // Set required fields: ( Response<i>, dummy ), for responses evaluated by the response evaluators
   vector<string>::const_iterator it;
   for (it = responseIDs_to_require.begin(); it != responseIDs_to_require.end(); it++)
   {
     const std::string& responseID = *it;

     PHX::Tag<AlbanyTraits::Residual::ScalarT> res_response_tag(responseID, dummy);
     rfm->requireField<AlbanyTraits::Residual>(res_response_tag);
     PHX::Tag<AlbanyTraits::Jacobian::ScalarT> jac_response_tag(responseID, dummy);
     rfm->requireField<AlbanyTraits::Jacobian>(jac_response_tag);
     PHX::Tag<AlbanyTraits::Tangent::ScalarT> tan_response_tag(responseID, dummy);
     rfm->requireField<AlbanyTraits::Tangent>(tan_response_tag);
     PHX::Tag<AlbanyTraits::SGResidual::ScalarT> sgres_response_tag(responseID, dummy);
     rfm->requireField<AlbanyTraits::SGResidual>(sgres_response_tag);
     PHX::Tag<AlbanyTraits::SGJacobian::ScalarT> sgjac_response_tag(responseID, dummy);
     rfm->requireField<AlbanyTraits::SGJacobian>(sgjac_response_tag);
     PHX::Tag<AlbanyTraits::MPResidual::ScalarT> mpres_response_tag(responseID, dummy);
     rfm->requireField<AlbanyTraits::MPResidual>(mpres_response_tag);
     PHX::Tag<AlbanyTraits::MPJacobian::ScalarT> mpjac_response_tag(responseID, dummy);
     rfm->requireField<AlbanyTraits::MPJacobian>(mpjac_response_tag);
   }
}


void
QCAD::PoissonProblem::constructDirichletEvaluators(
  const std::vector<std::string>& nodeSetIDs)
{
   using Teuchos::RCP;
   using Teuchos::rcp;
   using Teuchos::ParameterList;
   using PHX::DataLayout;
   using PHX::MDALayout;
   using std::vector;
   using std::map;
   using std::string;

   using PHAL::FactoryTraits;
   using PHAL::AlbanyTraits;

   //! DBCparams a member of base class Albany::AbstractProblem, as is getValidDirichletBCParameters
   DBCparams.validateParameters(*(getValidDirichletBCParameters(nodeSetIDs)),0); //TODO: Poisson version??

   // Create Material Database
   RCP<QCAD::MaterialDatabase> materialDB = rcp(new QCAD::MaterialDatabase(mtrlDbFilename, comm));

   map<string, RCP<ParameterList> > evaluators_to_build;
   RCP<DataLayout> dummy = rcp(new MDALayout<Dummy>(0));
   vector<string> dbcs;

   // Check for all possible standard BCs (every dof on every nodeset) to see which is set
   for (std::size_t i=0; i<nodeSetIDs.size(); i++) {
     for (std::size_t j=0; j<dofNames.size(); j++) {

       std::stringstream sstrm; sstrm << "DBC on NS " << nodeSetIDs[i] << " for DOF " << dofNames[j];
       std::string ss = sstrm.str();

       if (DBCparams.isParameter(ss)) {
         RCP<ParameterList> p = rcp(new ParameterList);
         int type = FactoryTraits<AlbanyTraits>::id_qcad_poisson_dirichlet;
         p->set<int>("Type", type);

         p->set< RCP<DataLayout> >("Data Layout", dummy);
         p->set< string >  ("Dirichlet Name", ss);
         p->set< RealType >("Dirichlet Value", DBCparams.get<double>(ss));
         p->set< string >  ("Node Set ID", nodeSetIDs[i]);
         p->set< int >     ("Number of Equations", dofNames.size());
         p->set< int >     ("Equation Offset", j);

         p->set<RCP<ParamLib> >("Parameter Library", paramLib);

         //! Additional parameters needed for Poisson Dirichlet BCs
         Teuchos::ParameterList& paramList = params->sublist("Poisson Source");
         p->set<Teuchos::ParameterList*>("Poisson Source Parameter List", &paramList);
         //p->set<string>("Temperature Name", "Temperature");  //to add if use shared param for DBC
         p->set<double>("Temperature", temperature);
         p->set< RCP<QCAD::MaterialDatabase> >("MaterialDB", materialDB);

         std::stringstream ess; ess << "Evaluator for " << ss;
         evaluators_to_build[ess.str()] = p;

         dbcs.push_back(ss);
       }
     }
   }

   //From here down, identical to Albany::AbstractProblem version of this function
   string allDBC="Evaluator for all Dirichlet BCs";
   {
      RCP<ParameterList> p = rcp(new ParameterList);
      int type = FactoryTraits<AlbanyTraits>::id_dirichlet_aggregator;
      p->set<int>("Type", type);

      p->set<vector<string>* >("DBC Names", &dbcs);
      p->set< RCP<DataLayout> >("Data Layout", dummy);
      p->set<string>("DBC Aggregator Name", allDBC);
      evaluators_to_build[allDBC] = p;
   }

   // Build Field Evaluators for each evaluation type
   PHX::EvaluatorFactory<AlbanyTraits,FactoryTraits<AlbanyTraits> > factory;
   RCP< vector< RCP<PHX::Evaluator_TemplateManager<AlbanyTraits> > > > evaluators;
   evaluators = factory.buildEvaluators(evaluators_to_build);

   // Create a DirichletFieldManager
   dfm = Teuchos::rcp(new PHX::FieldManager<AlbanyTraits>);

   // Register all Evaluators
   PHX::registerEvaluators(evaluators, *dfm);

   PHX::Tag<AlbanyTraits::Residual::ScalarT> res_tag0(allDBC, dummy);
   dfm->requireField<AlbanyTraits::Residual>(res_tag0);

   PHX::Tag<AlbanyTraits::Jacobian::ScalarT> jac_tag0(allDBC, dummy);
   dfm->requireField<AlbanyTraits::Jacobian>(jac_tag0);

   PHX::Tag<AlbanyTraits::Tangent::ScalarT> tan_tag0(allDBC, dummy);
   dfm->requireField<AlbanyTraits::Tangent>(tan_tag0);

   PHX::Tag<AlbanyTraits::SGResidual::ScalarT> sgres_tag0(allDBC, dummy);
   dfm->requireField<AlbanyTraits::SGResidual>(sgres_tag0);

   PHX::Tag<AlbanyTraits::SGJacobian::ScalarT> sgjac_tag0(allDBC, dummy);
   dfm->requireField<AlbanyTraits::SGJacobian>(sgjac_tag0);

   PHX::Tag<AlbanyTraits::MPResidual::ScalarT> mpres_tag0(allDBC, dummy);
   dfm->requireField<AlbanyTraits::MPResidual>(mpres_tag0);

   PHX::Tag<AlbanyTraits::MPJacobian::ScalarT> mpjac_tag0(allDBC, dummy);
   dfm->requireField<AlbanyTraits::MPJacobian>(mpjac_tag0);
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
  
  //For poisson schrodinger interations
  validPL->sublist("Dummy Dirichlet BCs", false, "");
  validPL->sublist("Dummy Parameters", false, "");

  return validPL;
}

