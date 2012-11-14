//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include "LameUtils.hpp"
#include "Teuchos_TestForException.hpp"
#include <string>
#include <algorithm>

// LAME material models
#ifdef ALBANY_LAME
#include "models/Elastic.h"
#include "models/ElasticNew.h"
#include "models/NeoHookean.h"
#include "models/ElasticFracture.h"
#include "models/ElasticPlastic.h"
#include "models/EPPowerLaw.h"
#include "models/DuctileFracture.h"
#include "models/JohnsonCook.h"
#include "models/PowerLawCreep.h"
#include "models/FoamPlasticity.h"
#include "models/LowDensityFoam.h"
#include "models/StiffElastic.h"
#include "models/FeFp.h"
#include "models/CrystalPlasticity.h"
#endif

namespace LameUtils {

Teuchos::RCP<LameMaterial> constructLameMaterialModel(const std::string& lameMaterialModelName,
						      const Teuchos::ParameterList& lameMaterialParameters){

  // Strings should be all upper case with spaces replaced with underscores
  std::string materialModelName = lameMaterialModelName;
  std::transform(materialModelName.begin(), materialModelName.end(), materialModelName.begin(), 
    (int (*)(int))std::toupper);
  std::replace(materialModelName.begin(), materialModelName.end(), ' ', '_');

  LameMatProps props;
  parameterListToMatProps(lameMaterialParameters, props);

  Teuchos::RCP<LameMaterial> materialModel;

#ifdef ALBANY_LAME
  if(materialModelName == "ELASTIC")
    materialModel = Teuchos::rcp(new lame::Elastic(props));
  else if(materialModelName == "ELASTIC_NEW")
    materialModel = Teuchos::rcp(new lame::ElasticNew(props));
  else if(materialModelName == "NEO_HOOKEAN")
    materialModel = Teuchos::rcp(new lame::NeoHookean(props));
  else if(materialModelName == "ELASTIC_FRACTURE")
    materialModel = Teuchos::rcp(new lame::ElasticFracture(props));
  else if(materialModelName == "ELASTIC_PLASTIC")
    materialModel = Teuchos::rcp(new lame::ElasticPlastic(props));
  else if(materialModelName == "EP_POWER_HARD")
    materialModel = Teuchos::rcp(new lame::EPPowerLaw(props));
  else if(materialModelName == "DUCTILE_FRACTURE")
    materialModel = Teuchos::rcp(new lame::DuctileFracture(props));
  else if(materialModelName == "JOHNSON_COOK")
    materialModel = Teuchos::rcp(new lame::JohnsonCook(props));
  else if(materialModelName == "POWER_LAW_CREEP")
    materialModel = Teuchos::rcp(new lame::PowerLawCreep(props));
  else if(materialModelName == "FOAM_PLASTICITY")
    materialModel = Teuchos::rcp(new lame::FoamPlasticity(props));
  else if(materialModelName == "LOW_DENSITY_FOAM")
    materialModel = Teuchos::rcp(new lame::LowDensityFoam(props));
  else if(materialModelName == "STIFF_ELASTIC")
    materialModel = Teuchos::rcp(new lame::StiffElastic(props));
  else if(materialModelName == "FEFP")
    materialModel = Teuchos::rcp(new lame::FeFp(props));
  else if(materialModelName == "CRYSTAL_PLASTICITY")
    materialModel = Teuchos::rcp(new lame::CrystalPlasticity(props));
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter, " unsupported LAME material model: " + lameMaterialModelName + " (" + materialModelName + ")\n");
#endif

#ifdef ALBANY_LAMENT
  if(materialModelName == "ELASTIC_NEW")
    materialModel = Teuchos::rcp(new lament::ElasticNew<double>(props));
  else if(materialModelName == "NEOHOOKEAN")
    materialModel = Teuchos::rcp(new lament::Neohookean<double>(props));
  else{
    if(materialModel.is_null())
      TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter, " unsupported LAMENT material model: " + lameMaterialModelName + " (" + materialModelName + ")\n");
  }
#endif

  return materialModel;
}

std::vector<std::string> getStateVariableNames(const std::string& lameMaterialModelName,
                                               const Teuchos::ParameterList& lameMaterialParameters){

  Teuchos::RCP<LameMaterial> materialModel = constructLameMaterialModel(lameMaterialModelName, lameMaterialParameters);

  // Get a list of the state variables, in alphabetical order
  std::vector<std::string> lameMaterialModelStateVariables;
  std::string tempLameMaterialModelName;
  materialModel->getStateVarListAndName(lameMaterialModelStateVariables, tempLameMaterialModelName);

  // Reorder the list to match the order of the variables in the actual state array passed to/from LAME
  std::map<int, std::string> variableNamesByIndex;
  for(unsigned int i=0 ; i<lameMaterialModelStateVariables.size() ; ++i){
    std::string variableName = lameMaterialModelStateVariables[i];
    int index = materialModel->getStateVariableIndex(variableName);
    variableNamesByIndex[index] = variableName;
  }

  std::vector<std::string> sortedVariableNames;
  for(unsigned int i=0 ; i<lameMaterialModelStateVariables.size() ; ++i){
    sortedVariableNames.push_back(variableNamesByIndex[(int)i]);
  }

  return sortedVariableNames;
}


std::vector<double> getStateVariableInitialValues(const std::string& lameMaterialModelName,
                                                  const Teuchos::ParameterList& lameMaterialParameters){

  Teuchos::RCP<LameMaterial> materialModel = constructLameMaterialModel(lameMaterialModelName, lameMaterialParameters);

  int numStateVariables = materialModel->getNumStateVars();

  // Allocate workset space
  std::vector<double> strainRate(6);   // symmetric tensor5
  std::vector<double> spin(3);         // skew-symmetric tensor
  std::vector<double> leftStretch(6);  // symmetric tensor
  std::vector<double> rotation(9);     // full tensor
  std::vector<double> stressOld(6);    // symmetric tensor
  std::vector<double> stressNew(6);    // symmetric tensor
  std::vector<double> stateOld(numStateVariables);  // a single double for each state variable
  std::vector<double> stateNew(numStateVariables);  // a single double for each state variable

  // \todo Set up scratch space for material models using getNumScratchVars() and setScratchPtr().

  // Create the matParams structure, which is passed to LAME
  Teuchos::RCP<LameMatParams> matp = Teuchos::rcp(new LameMatParams());
  matp->nelements = 1;
  matp->dt = 0.0;
  matp->time = 0.0;
  matp->strain_rate = &strainRate[0];
  matp->spin = &spin[0];
  matp->left_stretch = &leftStretch[0];
  matp->rotation = &rotation[0];
  matp->state_old = &stateOld[0];
  matp->state_new = &stateNew[0];
  matp->stress_old = &stressOld[0];
  matp->stress_new = &stressNew[0];

  LameMatProps props;
  parameterListToMatProps(lameMaterialParameters, props);

  // todo Check with Bill to see if props can be a null pointer (meaning no changes to the material properties).

  materialModel->initialize(matp.get(), &props);

  return stateOld;
}

}
