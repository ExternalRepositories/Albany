//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include "MechanicsProblem.hpp"
#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "PHAL_AlbanyTraits.hpp"

void
Albany::MechanicsProblem::
getVariableType(Teuchos::ParameterList& param_list,
                const std::string& default_type,
                Albany::MechanicsProblem::MECH_VAR_TYPE& variable_type,
                bool& have_variable,
                bool& have_equation)
{
  std::string type = param_list.get("Variable Type", default_type);
  if (type == "None")
    variable_type = MECH_VAR_TYPE_NONE;
  else if (type == "Constant")
    variable_type = MECH_VAR_TYPE_CONSTANT;
  else if (type == "DOF")
    variable_type = MECH_VAR_TYPE_DOF;
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                               "Unknown variable type " << type << std::endl);
  have_variable = (variable_type != MECH_VAR_TYPE_NONE);
  have_equation = (variable_type == MECH_VAR_TYPE_DOF);
}
//------------------------------------------------------------------------------
std::string
Albany::MechanicsProblem::
variableTypeToString(Albany::MechanicsProblem::MECH_VAR_TYPE variable_type)
{
  if (variable_type == MECH_VAR_TYPE_NONE)
    return "None";
  else if (variable_type == MECH_VAR_TYPE_CONSTANT)
    return "Constant";
  return "DOF";
}

//------------------------------------------------------------------------------
Albany::MechanicsProblem::
MechanicsProblem(const Teuchos::RCP<Teuchos::ParameterList>& params,
                 const Teuchos::RCP<ParamLib>& param_lib,
                 const int num_dims,
                 const Teuchos::RCP<const Epetra_Comm>& comm) :
  Albany::AbstractProblem(params, param_lib),
  have_source_(false),
  num_dims_(num_dims),
  have_mech_eq_(false),
  have_heat_eq_(false),
  have_pressure_eq_(false),
  have_transport_eq_(false),
  have_hydrostress_eq_(false)
{

  std::string& method = params->get("Name", "Mechanics ");
  *out << "Problem Name = " << method << std::endl;

  have_source_ =  params->isSublist("Source Functions");

  getVariableType(params->sublist("Displacement"), "DOF", mech_type_,
                  have_mech_, have_mech_eq_);
  getVariableType(params->sublist("Heat"), "None", heat_type_,
                  have_heat_, have_heat_eq_);
  getVariableType(params->sublist("Pore Pressure"), "None", pressure_type_,
                  have_pressure_, have_pressure_eq_);
  getVariableType(params->sublist("Transport"), "None", transport_type_,
                  have_transport_, have_transport_eq_);
  getVariableType(params->sublist("HydroStress"), "None", hydrostress_type_,
                  have_hydrostress_, have_hydrostress_eq_);

  if (have_heat_eq_)
    have_source_ =  params->isSublist("Source Functions");

  // Compute number of equations
  int num_eq = 0;
  if (have_mech_eq_) num_eq += num_dims_;
  if (have_heat_eq_) num_eq += 1;
  if (have_pressure_eq_) num_eq += 1;
  if (have_transport_eq_) num_eq += 1;
  if (have_hydrostress_eq_) num_eq +=1;
  this->setNumEquations(num_eq);

  // Print out a summary of the problem
  *out << "Mechanics problem:" << std::endl
       << "\tSpatial dimension:       " << num_dims_ << std::endl
       << "\tMechanics variables:     " << variableTypeToString(mech_type_)
       << std::endl
       << "\tHeat variables:          " << variableTypeToString(heat_type_)
       << std::endl
       << "\tPore Pressure variables: " << variableTypeToString(pressure_type_)
       << std::endl
       << "\tTransport variables:     " << variableTypeToString(transport_type_)
       << std::endl
       << "\tHydroStress variables:   " << variableTypeToString(hydrostress_type_)
       << std::endl;

  bool I_Do_Not_Have_A_Valid_Material_DB(true);
  if(params->isType<string>("MaterialDB Filename")){
    I_Do_Not_Have_A_Valid_Material_DB = false;
    std::string filename = params->get<string>("MaterialDB Filename");
    material_DB_ = Teuchos::rcp(new QCAD::MaterialDatabase(filename, comm));
  }
  TEUCHOS_TEST_FOR_EXCEPTION(I_Do_Not_Have_A_Valid_Material_DB, 
                             std::logic_error,
                             "Mechanics Problem Requires a Material Database");

}
//------------------------------------------------------------------------------
Albany::MechanicsProblem::
~MechanicsProblem()
{
}
//------------------------------------------------------------------------------
//the following function returns the problem information required for
//setting the rigid body modes (RBMs) for elasticity problems (in
//src/Albany_SolverFactory.cpp) written by IK, Feb. 2012
void
Albany::MechanicsProblem::
getRBMInfoForML(int& num_PDEs, int& num_elasticity_dim, 
                int& num_scalar, int& null_space_dim)
{
  // Need numPDEs should be num_dims_ + nDOF for other governing equations  -SS

  num_PDEs = neq;
  num_elasticity_dim = 0;
  if (have_mech_eq_) num_elasticity_dim = num_dims_;
  num_scalar = neq - num_elasticity_dim;
  if (have_mech_eq_) {
    if (num_dims_ == 1) {null_space_dim = 0; }
    if (num_dims_ == 2) {null_space_dim = 3; }
    if (num_dims_ == 3) {null_space_dim = 6; }
  }
}
//------------------------------------------------------------------------------
void
Albany::MechanicsProblem::
buildProblem(
             Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
             Albany::StateManager& stateMgr)
{
  /* Construct All Phalanx Evaluators */
  int physSets = meshSpecs.size();
  cout << "Num MeshSpecs: " << physSets << endl;
  fm.resize(physSets);

  cout << "Calling MechanicsProblem::buildEvaluators" << endl;
  for (int ps=0; ps<physSets; ps++) {
    fm[ps]  = Teuchos::rcp(new PHX::FieldManager<PHAL::AlbanyTraits>);
    buildEvaluators(*fm[ps], *meshSpecs[ps], stateMgr, BUILD_RESID_FM,
                    Teuchos::null);
  }
  constructDirichletEvaluators(*meshSpecs[0]);
}
//------------------------------------------------------------------------------
Teuchos::Array<Teuchos::RCP<const PHX::FieldTag> >
Albany::MechanicsProblem::
buildEvaluators(
                PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
                const Albany::MeshSpecsStruct& meshSpecs,
                Albany::StateManager& stateMgr,
                Albany::FieldManagerChoice fmchoice,
                const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
  // Call constructeEvaluators<EvalT>(*rfm[0], *meshSpecs[0], stateMgr);
  // for each EvalT in PHAL::AlbanyTraits::BEvalTypes
  ConstructEvaluatorsOp<MechanicsProblem> op(
                                             *this, fm0, meshSpecs, stateMgr, fmchoice, responseList);
  boost::mpl::for_each<PHAL::AlbanyTraits::BEvalTypes>(op);
  return *op.tags;
}
//------------------------------------------------------------------------------
void
Albany::MechanicsProblem::constructDirichletEvaluators(const Albany::MeshSpecsStruct& meshSpecs)
{

  // Construct Dirichlet evaluators for all nodesets and names
  std::vector<string> dirichletNames(neq);
  int index = 0;
  if (have_mech_eq_) {
    dirichletNames[index++] = "X";
    if (neq>1) dirichletNames[index++] = "Y";
    if (neq>2) dirichletNames[index++] = "Z";
  }

  if (have_heat_eq_) dirichletNames[index++] = "T";
  if (have_pressure_eq_) dirichletNames[index++] = "P";
  // Note: for hydrogen transport problem, L2 projection is need to derive the
  // source term/flux induced by volumetric deformation
  if (have_transport_eq_) dirichletNames[index++] = "C"; // Lattice Concentration
  if (have_hydrostress_eq_) dirichletNames[index++] = "TAU"; // Projected Hydrostatic Stress

  Albany::BCUtils<Albany::DirichletTraits> dirUtils;
  dfm = dirUtils.constructBCEvaluators(meshSpecs.nsNames, dirichletNames,
                                       this->params, this->paramLib);
}
//------------------------------------------------------------------------------
Teuchos::RCP<const Teuchos::ParameterList>
Albany::MechanicsProblem::getValidProblemParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
    this->getGenericProblemParams("ValidMechanicsProblemParams");

  validPL->set<string>("MaterialDB Filename",
                       "materials.xml",
                       "Filename of material database xml file");
  validPL->sublist("Displacement", false, "");
  validPL->sublist("Heat", false, "");
  validPL->sublist("Pore Pressure", false, "");
  validPL->sublist("Transport", false, "");
  validPL->sublist("HydroStress", false, "");


  return validPL;
}

void
Albany::MechanicsProblem::
getAllocatedStates(
   Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > old_state,
   Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > new_state
                   ) const
{
  old_state = old_state_;
  new_state = new_state_;
}
//------------------------------------------------------------------------------
// std::string
// Albany::MechanicsProblem::stateString(std::string name, bool surfaceFlag)
// {
//   std::string outputName(name);
//   if (surfaceFlag) outputName = "surf_"+name;
//   return outputName;
// }
// //------------------------------------------------------------------------------
// Teuchos::RCP<std::map<std::string, std::string> >
// Albany::MechanicsProblem::
// constructFieldNameMap(bool surface_flag)
// {
//   Teuchos::RCP<std::map<std::string, std::string> > name_map =
//     Teuchos::rcp( new std::map<std::string, std::string> );

//   name_map->insert( std::make_pair("Cauchy_Stress","Cauchy_Stress") );  
//   name_map->insert( std::make_pair("Fp","Fp") );
//   name_map->insert( std::make_pair("eqps","eqps") );
//   name_map->insert( std::make_pair("Total_Stress","Total_Stress") );
//   name_map->insert( std::make_pair("KCPermeability","KCPermeability") );
//   name_map->insert( std::make_pair("Biot_Modulus","Biot_Modulus") );
//   name_map->insert( std::make_pair("Biot_Coefficient","Biot_Coefficient") );
//   name_map->insert( std::make_pair("Porosity","Porosity") );
//   name_map->insert( std::make_pair("Pore_Pressure","Pore_Pressure") );
//   name_map->insert( std::make_pair("Matrix_Energy","Matrix_Energy") );
//   name_map->insert( std::make_pair("F1_Energy","F1_Energy") );
//   name_map->insert( std::make_pair("F2_Energy","F2_Energy") );
//   name_map->insert( std::make_pair("Matrix_Damage","Matrix_Damage") );
//   name_map->insert( std::make_pair("F1_Damage","F1_Damage") );
//   name_map->insert( std::make_pair("F2_Damage","F2_Damage") );
//   name_map->insert( std::make_pair("Void_Volume","Void_Volume") );

//   if ( surface_flag ) {
//     std::map<std::string, std::string>::iterator it;
//     for (it = name_map->begin(); it != name_map->end(); ++it) {
//       it->second = stateString(it->second, true);
//     }
//   }
//   return name_map;
// }
