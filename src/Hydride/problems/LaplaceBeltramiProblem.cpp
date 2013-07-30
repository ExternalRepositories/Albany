//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "LaplaceBeltramiProblem.hpp"
#include "Albany_InitialCondition.hpp"

#include "Intrepid_FieldContainer.hpp"
#include "Intrepid_DefaultCubatureFactory.hpp"
#include "Shards_CellTopology.hpp"
#include "PHAL_FactoryTraits.hpp"
#include "Albany_Utils.hpp"


Albany::LaplaceBeltramiProblem::
LaplaceBeltramiProblem(const Teuchos::RCP<Teuchos::ParameterList>& params_,
                       const Teuchos::RCP<ParamLib>& paramLib_,
                       const int numDim_,
                       const Teuchos::RCP<const Epetra_Comm>& comm_) :
  Albany::AbstractProblem(params_, paramLib_, numDim_),
  numDim(numDim_),
  comm(comm_) {

  // Ask the discretization to initialize the problem by copying the mesh coordinates into the initial guess
  //this->requirements.push_back("Initial Guess Coords");

}

Albany::LaplaceBeltramiProblem::
~LaplaceBeltramiProblem() {
}

void
Albany::LaplaceBeltramiProblem::
buildProblem(
  Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
  Albany::StateManager& stateMgr) {

  /* Construct All Phalanx Evaluators */
  int physSets = meshSpecs.size();
  cout << "LaplaceBeltrami Problem Num MeshSpecs: " << physSets << endl;
  fm.resize(physSets);

  for(int ps = 0; ps < physSets; ps++) {
    fm[ps]  = Teuchos::rcp(new PHX::FieldManager<PHAL::AlbanyTraits>);
    buildEvaluators(*fm[ps], *meshSpecs[ps], stateMgr, BUILD_RESID_FM,
                    Teuchos::null);
  }



  if(meshSpecs[0]->nsNames.size() > 0) // Build a nodeset evaluator if nodesets are present

    constructDirichletEvaluators(meshSpecs[0]->nsNames);


}

Teuchos::Array<Teuchos::RCP<const PHX::FieldTag> >
Albany::LaplaceBeltramiProblem::
buildEvaluators(
  PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
  const Albany::MeshSpecsStruct& meshSpecs,
  Albany::StateManager& stateMgr,
  Albany::FieldManagerChoice fmchoice,
  const Teuchos::RCP<Teuchos::ParameterList>& responseList) {
  // Call constructEvaluators<EvalT>(*rfm[0], *meshSpecs[0], stateMgr);
  // for each EvalT in PHAL::AlbanyTraits::BEvalTypes
  ConstructEvaluatorsOp<LaplaceBeltramiProblem> op(
    *this, fm0, meshSpecs, stateMgr, fmchoice, responseList);
  boost::mpl::for_each<PHAL::AlbanyTraits::BEvalTypes>(op);
  return *op.tags;
}

// Dirichlet BCs
void
Albany::LaplaceBeltramiProblem::constructDirichletEvaluators(const std::vector<std::string>& nodeSetIDs) {
  // Construct BC evaluators for all node sets and names
  std::vector<string> bcNames(1);
  bcNames[0] = "Identity";

  Albany::BCUtils<Albany::DirichletTraits> bcUtils;
  dfm = bcUtils.constructBCEvaluators(nodeSetIDs, bcNames,
                                      this->params, this->paramLib, numDim);
}


Teuchos::RCP<const Teuchos::ParameterList>
Albany::LaplaceBeltramiProblem::getValidProblemParameters() const {
  Teuchos::RCP<Teuchos::ParameterList> validPL =
    this->getGenericProblemParams("ValidLaplaceBeltramiProblemParams");

  /*
    Teuchos::Array<int> defaultPeriod;
    validPL->sublist("Thermal Conductivity", false, "");
    validPL->sublist("Hydrogen Conductivity", false, "");
    validPL->set<bool>("Have Rho Cp", false, "Flag to indicate if rhoCp is used");
    validPL->set<string>("MaterialDB Filename","materials.xml","Filename of material database xml file");
  */

  validPL->set<string>("Method", "", "Smoothing method to use");
  //  validPL->sublist("Constrained BCs", false, "");

  return validPL;
}

