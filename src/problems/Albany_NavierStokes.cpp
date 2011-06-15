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


#include "Albany_NavierStokes.hpp"
#include "Albany_BoundaryFlux1DResponseFunction.hpp"
#include "Albany_SolutionAverageResponseFunction.hpp"
#include "Albany_SolutionTwoNormResponseFunction.hpp"
#include "Albany_InitialCondition.hpp"

#include "Intrepid_FieldContainer.hpp"
#include "Intrepid_DefaultCubatureFactory.hpp"
#include "Shards_CellTopology.hpp"
#include "PHAL_FactoryTraits.hpp"
#include "Albany_Utils.hpp"


Albany::NavierStokes::
NavierStokes( const Teuchos::RCP<Teuchos::ParameterList>& params_,
             const Teuchos::RCP<ParamLib>& paramLib_,
             const int numDim_) :
  Albany::AbstractProblem(params_, paramLib_, numDim_+2),
  haveSource(false),
  numDim(numDim_)
{
  if (numDim==1) periodic = params->get("Periodic BC", false);
  else           periodic = false;
  if (periodic) *out <<" Periodic Boundary Conditions being used." <<std::endl;

  haveSource =  params->isSublist("Source Functions");

  // neq=numDim+2 set in AbstractProblem constructor
  dofNames.resize(neq);
  dofNames[0] = "T";
  dofNames[1] = "p";
  dofNames[2] = "ux";
  if (numDim>=2)
    dofNames[3] = "uy";
  if (numDim==3)
    dofNames[4] = "uz";
 
}

Albany::NavierStokes::
~NavierStokes()
{
}

void
Albany::NavierStokes::
buildProblem(
    const int worksetSize,
    Albany::StateManager& stateMgr,
    const Albany::AbstractDiscretization& disc,
    std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses)
{
  /* Construct All Phalanx Evaluators */
  constructEvaluators(worksetSize, disc.getCubatureDegree(), disc.getCellTopologyData());
  constructDirichletEvaluators(disc.getNodeSetIDs());
 
  const Epetra_Map& dofMap = *(disc.getMap());
  int left_node = dofMap.MinAllGID();
  int right_node = dofMap.MaxAllGID();

  // Build response functions
  Teuchos::ParameterList& responseList = params->sublist("Response Functions");
  int num_responses = responseList.get("Number", 0);
  responses.resize(num_responses);
  for (int i=0; i<num_responses; i++) {
     std::string name = responseList.get(Albany::strint("Response",i), "??");

     if (name == "Boundary Flux 1D" && numDim==1) {
       // Need real size, not 1.0
       double h =  1.0 / (dofMap.NumGlobalElements() - 1);
       responses[i] =
         Teuchos::rcp(new BoundaryFlux1DResponseFunction(left_node,
                                                         right_node,
                                                         0, 1, h,
                                                         dofMap));
     }

     else if (name == "Solution Average")
       responses[i] = Teuchos::rcp(new SolutionAverageResponseFunction());

     else if (name == "Solution Two Norm")
       responses[i] = Teuchos::rcp(new SolutionTwoNormResponseFunction());

     else {
       TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
                          std::endl <<
                          "Error!  Unknown response function " << name <<
                          "!" << std::endl << "Supplied parameter list is " <<
                          std::endl << responseList);
     }

  }
}


void
Albany::NavierStokes::constructEvaluators(
       const int worksetSize, const int cubDegree, const CellTopologyData& ctd)
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

   RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
     intrepidBasis = this->getIntrepidBasis(ctd);
   RCP<shards::CellTopology> cellType = rcp(new shards::CellTopology (&ctd));

   const int numNodes = intrepidBasis->getCardinality();

   Intrepid::DefaultCubatureFactory<RealType> cubFactory;
   RCP <Intrepid::Cubature<RealType> > cubature = cubFactory.create(*cellType, cubDegree);

   const int numQPts = cubature->getNumPoints();
   const int numVertices = cellType->getNodeCount();

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

  { // Gather Solution
   RCP< vector<string> > dof_names = rcp(new vector<string>(neq));
     (*dof_names)[0] = "Temperature";
     (*dof_names)[1] = "Pressure";
     (*dof_names)[2] = "Velocity x";
     if (numDim >=2)
       (*dof_names)[3] = "Velocity y";
     if (numDim==3)
      (*dof_names)[4] = "Velocity z";

    RCP<ParameterList> p = rcp(new ParameterList);
    int type = FactoryTraits<AlbanyTraits>::id_gather_solution;
    p->set<int>("Type", type);
    p->set< RCP< vector<string> > >("Solution Names", dof_names);
    p->set< RCP<DataLayout> >("Data Layout", node_scalar);

   RCP< vector<string> > dof_names_dot = rcp(new vector<string>(neq));
     (*dof_names_dot)[0] = "Temperature_dot";
     (*dof_names_dot)[1] = "Pressure_dot";
     (*dof_names_dot)[2] = "Velocity_dot x";
     if (numDim >=2)
       (*dof_names_dot)[3] = "Velocity_dot y";
     if (numDim==3)
      (*dof_names_dot)[4] = "Velocity_dot z";


   p->set< RCP< vector<string> > >("Time Dependent Solution Names", dof_names_dot);

    evaluators_to_build["Gather Solution"] = p;
  }

  { // Gather Coordinate Vector
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes Gather Coordinate Vector"));
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
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes Map To Physical Frame"));

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
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes Compute Basis Functions"));

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


  { // Thermal conductivity
    RCP<ParameterList> p = rcp(new ParameterList);

    int type = FactoryTraits<AlbanyTraits>::id_thermal_conductivity;
    p->set<int>("Type", type);

    p->set<string>("QP Variable Name", "Thermal Conductivity");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Thermal Conductivity");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    evaluators_to_build["Thermal Conductivity"] = p;
  }

  { // DOF: Interpolate nodal Temperature values to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Temperature"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Temperature");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("BF Name", "BF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    // Output (assumes same Name as input)
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    evaluators_to_build["DOF Temperature"] = p;
  }

  {
   // DOF: Interpolate nodal Temperature Dot  values to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Temperature Dot"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Temperature_dot");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("BF Name", "BF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    // Output (assumes same Name as input)
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    evaluators_to_build["DOF Temperature_dot"] = p;
  }

  { // DOF: Interpolate nodal Temperature gradients to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Temperature Grad"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_grad_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Temperature");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("Gradient BF Name", "Grad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);

    // Output
    p->set<string>("Gradient Variable Name", "Temperature Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    evaluators_to_build["DOF Grad Temperature"] = p;
  }
  
   { // DOF: Interpolate nodal Pressure values to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Pressure"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Pressure");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("BF Name", "BF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    // Output (assumes same Name as input)
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    evaluators_to_build["DOF Pressure"] = p;
  }

  {
   // DOF: Interpolate nodal Pressure Dot  values to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Pressure Dot"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Pressure_dot");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("BF Name", "BF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    // Output (assumes same Name as input)
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    evaluators_to_build["DOF Pressure_dot"] = p;
  }

  { // DOF: Interpolate nodal Pressure gradients to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Pressure Grad"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_grad_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Pressure");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("Gradient BF Name", "Grad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);

    // Output
    p->set<string>("Gradient Variable Name", "Pressure Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    evaluators_to_build["DOF Grad Pressure"] = p;
  }

   { // DOF: Interpolate nodal Velocity x values to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Velocity x"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Velocity x");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("BF Name", "BF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    // Output (assumes same Name as input)
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    evaluators_to_build["DOF Velocity x"] = p;
  }

  {
   // DOF: Interpolate nodal Velocity x Dot  values to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Velocity x Dot"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Velocity_dot x");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("BF Name", "BF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    // Output (assumes same Name as input)
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    evaluators_to_build["DOF Velocity_dot x"] = p;
  }

  { // DOF: Interpolate nodal Velocity x gradients to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Velocity x Grad"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_grad_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Velocity x");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("Gradient BF Name", "Grad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);

    // Output
    p->set<string>("Gradient Variable Name", "Velocity x Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    evaluators_to_build["DOF Grad Velocity x"] = p;
  }

    if (numDim>=2){ // DOF: Interpolate nodal Velocity y values to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Velocity y"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Velocity y");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("BF Name", "BF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    // Output (assumes same Name as input)
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    evaluators_to_build["DOF Velocity y"] = p;
  }

  if (numDim>=2){
   // DOF: Interpolate nodal Velocity y Dot  values to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Velocity y Dot"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Velocity_dot y");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("BF Name", "BF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    // Output (assumes same Name as input)
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    evaluators_to_build["DOF Velocity_dot y"] = p;
  }

  if (numDim>=2){ // DOF: Interpolate nodal Velocity y gradients to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Velocity y Grad"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_grad_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Velocity y");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("Gradient BF Name", "Grad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);

    // Output
    p->set<string>("Gradient Variable Name", "Velocity y Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    evaluators_to_build["DOF Grad Velocity y"] = p;
  }

    if (numDim==3){ // DOF: Interpolate nodal Velocity z values to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Velocity z"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Velocity z");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("BF Name", "BF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    // Output (assumes same Name as input)
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    evaluators_to_build["DOF Velocity z"] = p;
  }

  if (numDim==3){
   // DOF: Interpolate nodal Velocity x Dot  values to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Velocity z Dot"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Velocity_dot z");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("BF Name", "BF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);

    // Output (assumes same Name as input)
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    evaluators_to_build["DOF Velocity_dot z"] = p;
  }

  if (numDim==3){ // DOF: Interpolate nodal Velocity z gradients to quad points
    RCP<ParameterList> p = rcp(new ParameterList("Navier-Stokes DOFInterpolation Velocity z Grad"));

    int type = FactoryTraits<AlbanyTraits>::id_dof_grad_interpolation;
    p->set<int>   ("Type", type);

    // Input
    p->set<string>("Variable Name", "Velocity z");
    p->set< RCP<DataLayout> >("Node Data Layout",      node_scalar);

    p->set<string>("Gradient BF Name", "Grad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);

    // Output
    p->set<string>("Gradient Variable Name", "Velocity z Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    evaluators_to_build["DOF Grad Velocity z"] = p;
  }  

  if (haveSource) { // Source
    RCP<ParameterList> p = rcp(new ParameterList);

    int type = FactoryTraits<AlbanyTraits>::id_source;
    p->set<int>("Type", type);

    p->set<string>("Source Name", "Source");
    p->set<string>("Variable Name", "Temperature");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Source Functions");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    evaluators_to_build["Source"] = p;
  }

  { // Temperature Resid
    RCP<ParameterList> p = rcp(new ParameterList("Temperature Resid"));

    int type = FactoryTraits<AlbanyTraits>::id_heateqresid;
    p->set<int>("Type", type);

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);
    p->set<string>("QP Variable Name", "Temperature");

    p->set<string>("QP Time Derivative Variable Name", "Temperature_dot");

    p->set<bool>("Have Source", haveSource);
    p->set<string>("Source Name", "Source");

    p->set<string>("Thermal Conductivity Name", "Thermal Conductivity");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    p->set<string>("Gradient QP Variable Name", "Temperature Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);
    if (params->isType<string>("Convection Velocity"))
    	p->set<string>("Convection Velocity",
                       params->get<string>("Convection Velocity"));
    if (params->isType<bool>("Have Rho Cp"))
    	p->set<bool>("Have Rho Cp", params->get<bool>("Have Rho Cp"));

    //Output
    p->set<string>("Residual Name", "Temperature Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", node_scalar);

    evaluators_to_build["Heat Resid"] = p;
  }

  { // Pressure Resid
    RCP<ParameterList> p = rcp(new ParameterList("Pressure Resid"));

    int type = FactoryTraits<AlbanyTraits>::id_heateqresid;
    p->set<int>("Type", type);

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);
    p->set<string>("QP Variable Name", "Pressure");

    p->set<string>("QP Time Derivative Variable Name", "Pressure_dot");

    p->set<bool>("Have Source", false);
    p->set<string>("Source Name", "Source");

    p->set<string>("Thermal Conductivity Name", "Thermal Conductivity");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    p->set<string>("Gradient QP Variable Name", "Pressure Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);
  
    //Output
    p->set<string>("Residual Name", "Pressure Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", node_scalar);

    evaluators_to_build["Continuity Resid"] = p;
  }

  { // Velocity x Resid
    RCP<ParameterList> p = rcp(new ParameterList("Velocity x Resid"));

    int type = FactoryTraits<AlbanyTraits>::id_heateqresid;
    p->set<int>("Type", type);

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);
    p->set<string>("QP Variable Name", "Velocity x");

    p->set<string>("QP Time Derivative Variable Name", "Velocity_dot x");

    p->set<bool>("Have Source", false);
    p->set<string>("Source Name", "Source");

    p->set<string>("Thermal Conductivity Name", "Thermal Conductivity");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    p->set<string>("Gradient QP Variable Name", "Velocity x Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);
  
    //Output
    p->set<string>("Residual Name", "Velocity x Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", node_scalar);

    evaluators_to_build["Momentum x Resid"] = p;
  }

  if (numDim>=2){ // Velocity y Resid
    RCP<ParameterList> p = rcp(new ParameterList("Velocity y Resid"));

    int type = FactoryTraits<AlbanyTraits>::id_heateqresid;
    p->set<int>("Type", type);

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);
    p->set<string>("QP Variable Name", "Velocity y");

    p->set<string>("QP Time Derivative Variable Name", "Velocity_dot y");

    p->set<bool>("Have Source", false);
    p->set<string>("Source Name", "Source");

    p->set<string>("Thermal Conductivity Name", "Thermal Conductivity");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    p->set<string>("Gradient QP Variable Name", "Velocity y Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);
  
    //Output
    p->set<string>("Residual Name", "Velocity y Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", node_scalar);

    evaluators_to_build["Momentum y Resid"] = p;
  }

  if (numDim==3){ // Velocity z Resid
    RCP<ParameterList> p = rcp(new ParameterList("Velocity z Resid"));

    int type = FactoryTraits<AlbanyTraits>::id_heateqresid;
    p->set<int>("Type", type);

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", node_qp_scalar);
    p->set<string>("QP Variable Name", "Velocity z");

    p->set<string>("QP Time Derivative Variable Name", "Velocity_dot z");

    p->set<bool>("Have Source", false);
    p->set<string>("Source Name", "Source");

    p->set<string>("Thermal Conductivity Name", "Thermal Conductivity");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", qp_scalar);

    p->set<string>("Gradient QP Variable Name", "Velocity z Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", qp_vector);

    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", node_qp_vector);
  
    //Output
    p->set<string>("Residual Name", "Velocity z Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", node_scalar);

    evaluators_to_build["Momentum z Resid"] = p;
  }

  { // Scatter Residual
   RCP< vector<string> > resid_names = rcp(new vector<string>(neq));
     (*resid_names)[0] = "Temperature Residual";
     (*resid_names)[1] = "Pressure Residual";
     (*resid_names)[2] = "Velocity x Residual";
     if (numDim>=2)
       (*resid_names)[3] = "Velocity y Residual";
     if (numDim==3)
       (*resid_names)[4] = "Velocity z Residual";

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
}

Teuchos::RCP<const Teuchos::ParameterList>
Albany::NavierStokes::getValidProblemParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
    this->getGenericProblemParams("ValidNavierStokesParams");

  if (numDim==1)
    validPL->set<bool>("Periodic BC", false, "Flag to indicate periodic BC for 1D problems");
  validPL->sublist("Thermal Conductivity", false, "");
  validPL->set("Convection Velocity", "{0,0,0}", "");
  validPL->set<bool>("Have Rho Cp", false, "Flag to indicate if rhoCp is used");

  return validPL;
}

