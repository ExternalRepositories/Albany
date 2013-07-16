//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include "Albany_ProblemUtils.hpp"
#include "Albany_Utils.hpp"
#include "Intrepid_DefaultCubatureFactory.hpp"
#include "Intrepid_FieldContainer.hpp"
#include "ProjectionProblem.hpp"
#include "Shards_CellTopology.hpp"

Albany::ProjectionProblem::ProjectionProblem(
    RCP<ParameterList> const & parameter_list,
    RCP<ParamLib> const & parameter_library,
    int const number_dimensions) :
    Albany::AbstractProblem(
        parameter_list,
        parameter_library,
        number_dimensions + 9), // additional DOF for pore pressure
    have_boundary_source_(false), number_dimensions_(number_dimensions),
    projection_(
        params->sublist("Projection").get("Projection Variable", ""),
        params->sublist("Projection").get("Projection Rank", 0),
        params->sublist("Projection").get("Projection Comp", 0), number_dimensions_)
{

  std::string& method = params->get("Name",
      "Total Lagrangian Plasticity with Projection ");
  *out << "Problem Name = " << method << std::endl;

  have_boundary_source_ = params->isSublist("Source Functions");

  material_model_ = params->sublist("Material Model").get("Model Name", "Neohookean");
  projection_field_ = params->sublist("Projection").get("Projection Variable",
      "");
  projection_rank_ = params->sublist("Projection").get("Projection Rank", 0);
  *out << "Projection Variable: " << projection_field_ << std::endl;
  *out << "Projection Variable Rank: " << projection_rank_ << std::endl;

  insertion_criterion_ = params->sublist("Insertion Criteria").get(
      "Insertion Criteria", "");

  // Only run if there is a projection variable defined
  if (projection_.isProjected()) {
    // For debug purposes
    *out << "Will variable be projected? " << projection_.isProjected()
        << std::endl;
    *out << "Number of components: " << projection_.getProjectedComponents()
        << std::endl;
    *out << "Rank of variable: " << projection_.getProjectedRank() << std::endl;

    /* the evaluator constructor requires information on the size of the
     * projected variable as boolean flags in the argument list. Allowed
     * variable types are vector, (rank 2) tensor, or scalar (default).
     */
    switch (projection_.getProjectedRank()) {
    // Currently doesn't really do anything. Have to change when I decide how to store the variable
    case 1:
      is_field_vector_ = true;
      is_field_tensor_ = false;
      break;

    case 2:
      //is_field_vector_ = false;
      //is_field_tensor_ = true;
      is_field_vector_ = true;
      is_field_tensor_ = false;
      break;

    default:
      is_field_vector_ = false;
      is_field_tensor_ = false;
      break;
    }
  }

// Changing this ifdef changes ordering from  (X,Y,T) to (T,X,Y)
//#define NUMBER_T_FIRST
#ifdef NUMBER_T_FIRST
  temperature_offset_=0;
  position_offset_=projection_.getProjectedComponents();
#else
  position_offset_ = 0;
  temperature_offset_ = number_dimensions_;
#endif
}

//
// Simple destructor
//
Albany::ProjectionProblem::~ProjectionProblem()
{
}

// returns the problem information required for setting the rigid body modes
// (RBMs) for elasticity problems (in src/Albany_SolverFactory.cpp)
// IK, 2012-02
void Albany::ProjectionProblem::getRBMInfoForML(
    int & number_PDEs,
    int & number_elasticity_dimensions,
    int & number_scalar_dimensions,
    int & null_space_dimensions)
{
  number_PDEs = number_dimensions_ + projection_.getProjectedComponents();
  number_elasticity_dimensions = number_dimensions_;
  number_scalar_dimensions = projection_.getProjectedComponents();

  switch (number_dimensions_) {
  default:
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "Invalid number of dimensions");
    break;
  case 1:
    null_space_dimensions = 0;
    break;
  case 2:
    null_space_dimensions = 3;
    break;
  case 3:
    null_space_dimensions = 6;
    break;
  }

}

void Albany::ProjectionProblem::buildProblem(
    ArrayRCP<RCP<Albany::MeshSpecsStruct> > mesh_specs,
    Albany::StateManager& state_manager)
{
  /* Construct All Phalanx Evaluators */
  TEUCHOS_TEST_FOR_EXCEPTION(mesh_specs.size() != 1, std::logic_error,
      "Problem supports one Material Block");
  fm.resize(1);
  fm[0] = rcp(new PHX::FieldManager<AlbanyTraits>);
  buildEvaluators(*fm[0], *mesh_specs[0], state_manager, BUILD_RESID_FM,
      Teuchos::null);
  constructDirichletEvaluators(*mesh_specs[0]);
}

Teuchos::Array<RCP<const PHX::FieldTag> > Albany::ProjectionProblem::buildEvaluators(
    PHX::FieldManager<AlbanyTraits>& field_manager,
    const Albany::MeshSpecsStruct& mesh_specs, Albany::StateManager& state_manager,
    Albany::FieldManagerChoice field_manager_choice,
    const RCP<ParameterList>& response_list)
{
  // Call constructeEvaluators<Evaluator>(*rfm[0], *mesh_specs[0], state_manager);
  // for each Evaluator in AlbanyTraits::BEvalTypes
  ConstructEvaluatorsOp<ProjectionProblem> op(*this, field_manager, mesh_specs, state_manager,
      field_manager_choice, response_list);
  boost::mpl::for_each<AlbanyTraits::BEvalTypes>(op);
  return *op.tags;
}

void Albany::ProjectionProblem::constructDirichletEvaluators(
    const Albany::MeshSpecsStruct& mesh_specs)
{
  // Construct Dirichlet evaluators for all nodesets and names
  std::vector<std::string> dirichletNames(neq);
  dirichletNames[position_offset_] = "X";
  if (number_dimensions_ > 1) dirichletNames[position_offset_ + 1] = "Y";
  if (number_dimensions_ > 2) dirichletNames[position_offset_ + 2] = "Z";
  dirichletNames[temperature_offset_] = "T";
  Albany::BCUtils<Albany::DirichletTraits> dirUtils;
  dfm = dirUtils.constructBCEvaluators(mesh_specs.nsNames, dirichletNames,
      this->params, this->paramLib);
}

RCP<const ParameterList> Albany::ProjectionProblem::getValidProblemParameters() const
{
  RCP<ParameterList> validPL = this->getGenericProblemParams(
      "ValidProjectionProblemParams");

  validPL->sublist("Material Model", false, "");
  validPL->set<bool>("avgJ", false,
      "Flag to indicate the J should be averaged");
  validPL->set<bool>("volavgJ", false,
      "Flag to indicate the J should be volume averaged");
  validPL->set<bool>("weighted_Volume_Averaged_J", false,
      "Flag to indicate the J should be volume averaged with stabilization");
  validPL->sublist("Elastic Modulus", false, "");
  validPL->sublist("Shear Modulus", false, "");
  validPL->sublist("Poissons Ratio", false, "");
  validPL->sublist("Projection", false, "");
  validPL->sublist("Insertion Criteria", false, "");

  if (material_model_ == "J2" || material_model_ == "J2Fiber") {
    validPL->set<bool>("Compute Dislocation Density Tensor", false,
        "Flag to compute the dislocaiton density tensor (only for 3D)");
    validPL->sublist("Hardening Modulus", false, "");
    validPL->sublist("Saturation Modulus", false, "");
    validPL->sublist("Saturation Exponent", false, "");
    validPL->sublist("Yield Strength", false, "");

    if (material_model_ == "J2Fiber") {
      validPL->set<RealType>("xiinf_J2", false, "");
      validPL->set<RealType>("tau_J2", false, "");
      validPL->set<RealType>("k_f1", false, "");
      validPL->set<RealType>("q_f1", false, "");
      validPL->set<RealType>("vol_f1", false, "");
      validPL->set<RealType>("xiinf_f1", false, "");
      validPL->set<RealType>("tau_f1", false, "");
      validPL->set<RealType>("Mx_f1", false, "");
      validPL->set<RealType>("My_f1", false, "");
      validPL->set<RealType>("Mz_f1", false, "");
      validPL->set<RealType>("k_f2", false, "");
      validPL->set<RealType>("q_f2", false, "");
      validPL->set<RealType>("vol_f2", false, "");
      validPL->set<RealType>("xiinf_f2", false, "");
      validPL->set<RealType>("tau_f2", false, "");
      validPL->set<RealType>("Mx_f2", false, "");
      validPL->set<RealType>("My_f2", false, "");
      validPL->set<RealType>("Mz_f2", false, "");
    }
  }

  return validPL;
}

void Albany::ProjectionProblem::getAllocatedStates(
    ArrayRCP<
        ArrayRCP<RCP<FieldContainer<RealType> > > > old_state,
    ArrayRCP<
        ArrayRCP<RCP<FieldContainer<RealType> > > > new_state) const
{
  old_state = old_state_;
  new_state = new_state_;
}
