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
#include "Albany_SolutionAverageResponseFunction.hpp"
#include "Albany_SolutionTwoNormResponseFunction.hpp"
#include "Albany_SolutionMaxValueResponseFunction.hpp"
#include "Albany_InitialCondition.hpp"

#include "Intrepid_FieldContainer.hpp"
#include "Intrepid_DefaultCubatureFactory.hpp"
#include "Shards_CellTopology.hpp"
#include "PHAL_FactoryTraits.hpp"
#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"

void
Albany::NavierStokes::
getVariableType(Teuchos::ParameterList& paramList,
		const std::string& defaultType,
		Albany::NavierStokes::NS_VAR_TYPE& variableType,
		bool& haveVariable,
		bool& haveEquation)
{
  std::string type = paramList.get("Variable Type", defaultType);
  if (type == "None")
    variableType = NS_VAR_TYPE_NONE;
  else if (type == "Constant")
    variableType = NS_VAR_TYPE_CONSTANT;
  else if (type == "DOF")
    variableType = NS_VAR_TYPE_DOF;
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
		       "Unknown variable type " << type << std::endl);
  haveVariable = (variableType != NS_VAR_TYPE_NONE);
  haveEquation = (variableType == NS_VAR_TYPE_DOF);
}

std::string
Albany::NavierStokes::
variableTypeToString(Albany::NavierStokes::NS_VAR_TYPE variableType)
{
  if (variableType == NS_VAR_TYPE_NONE)
    return "None";
  else if (variableType == NS_VAR_TYPE_CONSTANT)
    return "Constant";
  return "DOF";
}

Albany::NavierStokes::
NavierStokes( const Teuchos::RCP<Teuchos::ParameterList>& params_,
             const Teuchos::RCP<ParamLib>& paramLib_,
             const int numDim_) :
  Albany::AbstractProblem(params_, paramLib_),
  haveFlow(false),
  haveHeat(false),
  haveNeut(false),
  haveFlowEq(false),
  haveHeatEq(false),
  haveNeutEq(false),
  haveSource(false),
  haveNeutSource(false),
  havePSPG(false),
  haveSUPG(false),
  porousMedia(false),
  numDim(numDim_)
{
  if (numDim==1) periodic = params->get("Periodic BC", false);
  else           periodic = false;
  if (periodic) *out <<" Periodic Boundary Conditions being used." <<std::endl;

  getVariableType(params->sublist("Flow"), "DOF", flowType, 
		  haveFlow, haveFlowEq);
  getVariableType(params->sublist("Heat"), "None", heatType, 
		  haveHeat, haveHeatEq);
  getVariableType(params->sublist("Neutronics"), "None", neutType, 
		  haveNeut, haveNeutEq);

  if (haveFlowEq) {
    havePSPG = params->get("Have Pressure Stabilization", true);
    porousMedia = params->get("Porous Media",false);
  }

  if (haveFlow && (haveFlowEq || haveHeatEq))
    haveSUPG = params->get("Have SUPG Stabilization", true);

  if (haveHeatEq)
    haveSource =  params->isSublist("Source Functions");

  if (haveNeutEq)
    haveNeutSource =  params->isSublist("Neutron Source");

  // Compute number of equations
  int num_eq = 0;
  if (haveFlowEq) num_eq += numDim+1;
  if (haveHeatEq) num_eq += 1;
  if (haveNeutEq) num_eq += 1;
  this->setNumEquations(num_eq);

  // Print out a summary of the problem
  *out << "Navier-Stokes problem:" << std::endl
       << "\tSpatial dimension:      " << numDim << std::endl
       << "\tFlow variables:         " << variableTypeToString(flowType) 
       << std::endl
       << "\tHeat variables:         " << variableTypeToString(heatType) 
       << std::endl
       << "\tNeutronics variables:   " << variableTypeToString(neutType) 
       << std::endl
       << "\tPressure stabilization: " << havePSPG << std::endl
       << "\tUpwind stabilization:   " << haveSUPG << std::endl
       << "\tPorous media:           " << porousMedia << std::endl;
}

Albany::NavierStokes::
~NavierStokes()
{
}

void
Albany::NavierStokes::
buildProblem(
    Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
    Albany::StateManager& stateMgr,
    Teuchos::ArrayRCP< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses)
{
 /* Construct All Phalanx Evaluators */
  TEUCHOS_TEST_FOR_EXCEPTION(meshSpecs.size()!=1,std::logic_error,"Problem supports one Material Block");
  fm.resize(1); rfm.resize(1);
  fm[0]  = Teuchos::rcp(new PHX::FieldManager<PHAL::AlbanyTraits>);
  rfm[0] = Teuchos::rcp(new PHX::FieldManager<PHAL::AlbanyTraits>);

  constructResidEvaluators<PHAL::AlbanyTraits::Residual  >(*fm[0], *meshSpecs[0], stateMgr);
  constructResidEvaluators<PHAL::AlbanyTraits::Jacobian  >(*fm[0], *meshSpecs[0], stateMgr);
  constructResidEvaluators<PHAL::AlbanyTraits::Tangent   >(*fm[0], *meshSpecs[0], stateMgr);
  constructResidEvaluators<PHAL::AlbanyTraits::SGResidual>(*fm[0], *meshSpecs[0], stateMgr);
  constructResidEvaluators<PHAL::AlbanyTraits::SGJacobian>(*fm[0], *meshSpecs[0], stateMgr);
  constructResidEvaluators<PHAL::AlbanyTraits::SGTangent >(*fm[0], *meshSpecs[0], stateMgr);
  constructResidEvaluators<PHAL::AlbanyTraits::MPResidual>(*fm[0], *meshSpecs[0], stateMgr);
  constructResidEvaluators<PHAL::AlbanyTraits::MPJacobian>(*fm[0], *meshSpecs[0], stateMgr);
  constructResidEvaluators<PHAL::AlbanyTraits::MPTangent >(*fm[0], *meshSpecs[0], stateMgr);
  constructResponseEvaluators<PHAL::AlbanyTraits::Residual  >(*rfm[0], *meshSpecs[0], stateMgr, responses);
  constructResponseEvaluators<PHAL::AlbanyTraits::Jacobian  >(*rfm[0], *meshSpecs[0], stateMgr);
  constructResponseEvaluators<PHAL::AlbanyTraits::Tangent   >(*rfm[0], *meshSpecs[0], stateMgr);
  constructResponseEvaluators<PHAL::AlbanyTraits::SGResidual>(*rfm[0], *meshSpecs[0], stateMgr);
  constructResponseEvaluators<PHAL::AlbanyTraits::SGJacobian>(*rfm[0], *meshSpecs[0], stateMgr);
  constructResponseEvaluators<PHAL::AlbanyTraits::SGTangent >(*rfm[0], *meshSpecs[0], stateMgr);
  constructResponseEvaluators<PHAL::AlbanyTraits::MPResidual>(*rfm[0], *meshSpecs[0], stateMgr);
  constructResponseEvaluators<PHAL::AlbanyTraits::MPJacobian>(*rfm[0], *meshSpecs[0], stateMgr);
  constructResponseEvaluators<PHAL::AlbanyTraits::MPTangent >(*rfm[0], *meshSpecs[0], stateMgr);

  constructDirichletEvaluators(*meshSpecs[0]);
}

void
Albany::NavierStokes::constructDirichletEvaluators(
        const Albany::MeshSpecsStruct& meshSpecs)
{
   // Construct Dirichlet evaluators for all nodesets and names
   vector<string> dirichletNames(neq);
   int index = 0;
   if (haveFlowEq) {
     dirichletNames[index++] = "ux";
     if (numDim>=2) dirichletNames[index++] = "uy";
     if (numDim==3) dirichletNames[index++] = "uz";
     dirichletNames[index++] = "p";
   }
   if (haveHeatEq) dirichletNames[index++] = "T";
   if (haveNeutEq) dirichletNames[index++] = "phi";
   Albany::BCUtils<Albany::DirichletTraits> dirUtils;
   dfm = dirUtils.constructBCEvaluators(meshSpecs.nsNames, dirichletNames,
                                          this->params, this->paramLib);
}

Teuchos::RCP<const Teuchos::ParameterList>
Albany::NavierStokes::getValidProblemParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
    this->getGenericProblemParams("ValidNavierStokesParams");

  if (numDim==1)
    validPL->set<bool>("Periodic BC", false, "Flag to indicate periodic BC for 1D problems");
  validPL->set<bool>("Have Pressure Stabilization", true);
  validPL->set<bool>("Have SUPG Stabilization", true);
  validPL->set<bool>("Porous Media", false, "Flag to use porous media equations");
  validPL->sublist("Flow", false, "");
  validPL->sublist("Heat", false, "");
  validPL->sublist("Neutronics", false, "");
  validPL->sublist("Thermal Conductivity", false, "");
  validPL->sublist("Density", false, "");
  validPL->sublist("Viscosity", false, "");
  validPL->sublist("Volumetric Expansion Coefficient", false, "");
  validPL->sublist("Specific Heat", false, "");
  validPL->sublist("Body Force", false, "");
  validPL->sublist("Porosity", false, "");
  validPL->sublist("Permeability", false, "");
  validPL->sublist("Forchheimer", false, "");
  
  validPL->sublist("Neutron Source", false, "");
  validPL->sublist("Neutron Diffusion Coefficient", false, "");
  validPL->sublist("Absorption Cross Section", false, "");
  validPL->sublist("Fission Cross Section", false, "");
  validPL->sublist("Neutrons per Fission", false, "");
  validPL->sublist("Scattering Cross Section", false, "");
  validPL->sublist("Average Scattering Angle", false, "");
  validPL->sublist("Energy Released per Fission", false, "");

  return validPL;
}

