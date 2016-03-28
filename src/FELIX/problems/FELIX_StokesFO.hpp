//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef FELIX_STOKES_FO_PROBLEM_HPP
#define FELIX_STOKES_FO_PROBLEM_HPP 1

#include <type_traits>

#include "Intrepid2_FieldContainer.hpp"
#include "Intrepid2_DefaultCubatureFactory.hpp"
#include "Phalanx.hpp"
#include "Shards_CellTopology.hpp"
#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"
#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_EvaluatorUtils.hpp"
#include "Albany_ResponseUtilities.hpp"

#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"
#include "PHAL_AlbanyTraits.hpp"

#include "PHAL_LoadStateField.hpp"
#include "PHAL_DOFCellToSide.hpp"
#include "PHAL_DOFVecCellToSide.hpp"
#include "PHAL_DOFVecInterpolationSide.hpp"
#include "PHAL_LoadSideSetStateField.hpp"
#include "PHAL_SaveSideSetStateField.hpp"
#include "FELIX_SharedParameter.hpp"
#include "FELIX_StokesParamEnum.hpp"

#include "FELIX_EffectivePressure.hpp"
#include "FELIX_StokesFOResid.hpp"
#include "FELIX_StokesFOBasalResid.hpp"
#ifdef CISM_HAS_FELIX
#include "FELIX_CismSurfaceGradFO.hpp"
#endif
#include "FELIX_StokesFOBodyForce.hpp"
#include "FELIX_ViscosityFO.hpp"
#include "FELIX_FieldNorm.hpp"
#include "FELIX_FluxDiv.hpp"
#include "FELIX_BasalFrictionCoefficient.hpp"
#include "FELIX_BasalFrictionCoefficientGradient.hpp"
#include "FELIX_UpdateZCoordinate.hpp"
#include "FELIX_GatherVerticallyAveragedVelocity.hpp"

//uncomment the following line if you want debug output to be printed to screen
//#define OUTPUT_TO_SCREEN

namespace FELIX
{

/*!
 * \brief Abstract interface for representing a 1-D finite element
 * problem.
 */
class StokesFO : public Albany::AbstractProblem
{
public:

  //! Default constructor
  StokesFO (const Teuchos::RCP<Teuchos::ParameterList>& params,
            const Teuchos::RCP<ParamLib>& paramLib,
            const int numDim_);

  //! Destructor
  ~StokesFO();

  //! Return number of spatial dimensions
  virtual int spatialDimension() const { return numDim; }

  //! Build the PDE instantiations, boundary conditions, and initial solution
  virtual void buildProblem (Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
                             Albany::StateManager& stateMgr);

  // Build evaluators
  virtual Teuchos::Array< Teuchos::RCP<const PHX::FieldTag> >
  buildEvaluators (PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
                   const Albany::MeshSpecsStruct& meshSpecs,
                   Albany::StateManager& stateMgr,
                   Albany::FieldManagerChoice fmchoice,
                   const Teuchos::RCP<Teuchos::ParameterList>& responseList);

  //! Each problem must generate it's list of valide parameters
  Teuchos::RCP<const Teuchos::ParameterList> getValidProblemParameters() const;

private:

  //! Private to prohibit copying
  StokesFO(const StokesFO&);

  //! Private to prohibit copying
  StokesFO& operator=(const StokesFO&);

public:

  //! Main problem setup routine. Not directly called, but indirectly by following functions
  template <typename EvalT> Teuchos::RCP<const PHX::FieldTag>
  constructEvaluators (PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
                       const Albany::MeshSpecsStruct& meshSpecs,
                       Albany::StateManager& stateMgr,
                       Albany::FieldManagerChoice fmchoice,
                       const Teuchos::RCP<Teuchos::ParameterList>& responseList);

  void constructDirichletEvaluators(const Albany::MeshSpecsStruct& meshSpecs);
  void constructNeumannEvaluators(const Teuchos::RCP<Albany::MeshSpecsStruct>& meshSpecs);

protected:

  Teuchos::RCP<shards::CellTopology> cellType;
  Teuchos::RCP<shards::CellTopology> basalSideType;
  Teuchos::RCP<shards::CellTopology> surfaceSideType;

  Teuchos::RCP<Intrepid2::Cubature<RealType, Intrepid2::FieldContainer_Kokkos<RealType, PHX::Layout,PHX::Device> > >  cellCubature;
  Teuchos::RCP<Intrepid2::Cubature<RealType, Intrepid2::FieldContainer_Kokkos<RealType, PHX::Layout,PHX::Device> > >  basalCubature;
  Teuchos::RCP<Intrepid2::Cubature<RealType, Intrepid2::FieldContainer_Kokkos<RealType, PHX::Layout,PHX::Device> > >  surfaceCubature;

  Teuchos::RCP<Intrepid2::Basis<RealType, Intrepid2::FieldContainer_Kokkos<RealType, PHX::Layout, PHX::Device> > > cellBasis;
  Teuchos::RCP<Intrepid2::Basis<RealType, Intrepid2::FieldContainer_Kokkos<RealType, PHX::Layout, PHX::Device> > > basalSideBasis;
  Teuchos::RCP<Intrepid2::Basis<RealType, Intrepid2::FieldContainer_Kokkos<RealType, PHX::Layout, PHX::Device> > > surfaceSideBasis;

  int numDim;
  Teuchos::RCP<Albany::Layouts> dl,dl_basal,dl_surface;
  std::map<std::string,Teuchos::RCP<Albany::Layouts>> dls;

  bool  sliding;
  std::string basalSideName;
  std::string surfaceSideName;

  std::string elementBlockName;
  std::string basalEBName;
  std::string surfaceEBName;
};

} // Namespace FELIX

// ================================ IMPLEMENTATION ============================ //

template <typename EvalT>
Teuchos::RCP<const PHX::FieldTag>
FELIX::StokesFO::constructEvaluators (PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
                                      const Albany::MeshSpecsStruct& meshSpecs,
                                      Albany::StateManager& stateMgr,
                                      Albany::FieldManagerChoice fieldManagerChoice,
                                      const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
  Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils(dl);
  Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtilsBasal(dl_basal);
  Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtilsSurface(dl_surface);

  int offset=0;

  Albany::StateStruct::MeshFieldEntity entity;
  Teuchos::RCP<PHX::Evaluator<PHAL::AlbanyTraits> > ev;
  Teuchos::RCP<Teuchos::ParameterList> p;

  // ---------------------------- Registering state variables ------------------------- //

  std::string stateName, fieldName, param_name;

  // Temperature
  entity = Albany::StateStruct::NodalDataToElemNode;
  stateName = fieldName = "temperature";
  p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName, true, &entity);
  ev = Teuchos::rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);
  if (ss_requirements.find(basalSideName)!=ss_requirements.end())
  {
    const Albany::AbstractFieldContainer::FieldContainerRequirements& req = ss_requirements.at(basalSideName);
    auto it = std::find(req.begin(), req.end(), stateName);
    if (it!=req.end())
    {
      entity = Albany::StateStruct::NodalDataToElemNode;
      // Note: temperature on side registered as node vector, since we store all layers values in the nodes on the basal mesh.
      //       However, we need to create the layout, since the vector length is the number of layers.
      //       I don't have a clean solution now, so I just ask the user to pass me the number of layers in the temperature file.
      int numLayers = params->get<int>("Layered Data Length",11); // Default 11 layers: 0, 0.1, ...,1.0
      Teuchos::RCP<PHX::DataLayout> dl_temp;
      Teuchos::RCP<PHX::DataLayout> sns = dl_basal->side_node_scalar;
      dl_temp = Teuchos::rcp(new PHX::MDALayout<Cell,Side,Node,LayerDim>(sns->dimension(0),sns->dimension(1),sns->dimension(2),numLayers));
      p = stateMgr.registerSideSetStateVariable(basalSideName, stateName, fieldName, dl_temp, basalEBName, true, &entity);
    }
  }

  // Flow factor
  entity = Albany::StateStruct::ElemData;
  stateName = fieldName = "flow_factor";
  p = stateMgr.registerStateVariable(stateName, dl->cell_scalar2, elementBlockName, true, &entity);
  p->set<std::string>("Field Name", fieldName);
  ev = Teuchos::rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);

  // Surface height
  entity = Albany::StateStruct::NodalDataToElemNode;
  stateName = "surface_height";
  fieldName = "Surface Height";
  p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName, true, &entity);
  p->set<std::string>("Field Name", fieldName);
  ev = Teuchos::rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);
  if (ss_requirements.find(basalSideName)!=ss_requirements.end())
  {
    // If requested, we also add it as side set state (e.g., needed if we load from file on the side mesh)
    const Albany::AbstractFieldContainer::FieldContainerRequirements& req = ss_requirements.at(basalSideName);
    auto it = std::find(req.begin(), req.end(), stateName);
    if (it!=req.end())
      p = stateMgr.registerSideSetStateVariable(basalSideName, stateName, fieldName, dl_basal->side_node_scalar, basalEBName, true, &entity);
  }

#ifdef CISM_HAS_FELIX
  // Surface Gradient-x
  entity = Albany::StateStruct::NodalDataToElemNode;
  stateName = "xgrad_surface_height"; //ds/dx which can be passed from CISM (definened at nodes)
  fieldName = "CISM Surface Height Gradient X";
  p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName, true, &entity);
  p->set<std::string>("Field Name", fieldName);
  ev = Teuchos::rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);

  // Surface Gradient-y
  entity = Albany::StateStruct::NodalDataToElemNode;
  stateName = "ygrad_surface_height"; //ds/dy which can be passed from CISM (defined at nodes)
  fieldName = "CISM Surface Height Gradient Y";
  p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName, true, &entity);
  p->set<std::string>("Field Name", fieldName);
  ev = Teuchos::rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);
#endif

  // Ice thickness
  bool isThicknessAParameter = false; // Determining whether thickness is a distributed parameter
  stateName = "thickness";
  const std::string* meshPart;
  const std::string emptyString("");
  if (this->params->isSublist("Distributed Parameters"))
  {
    Teuchos::ParameterList& dist_params_list =  this->params->sublist("Distributed Parameters");
    Teuchos::ParameterList* param_list;
    int numParams = dist_params_list.get<int>("Number of Parameter Vectors",0);
    for (int p_index=0; p_index< numParams; ++p_index)
    {
      std::string parameter_sublist_name = Albany::strint("Distributed Parameter", p_index);
      if (dist_params_list.isSublist(parameter_sublist_name))
      {
        param_list = &dist_params_list.sublist(parameter_sublist_name);
        if (param_list->get<std::string>("Name", emptyString) == stateName)
        {
          meshPart = &param_list->get<std::string>("Mesh Part",emptyString);
          isThicknessAParameter = true;
          break;
        }
      }
      else
      {
        if (stateName == dist_params_list.get(Albany::strint("Parameter", p_index), emptyString))
        {
          isThicknessAParameter = true;
          meshPart = &emptyString;
          break;
        }
      }
    }
  }

  if(isThicknessAParameter)
  {
    // Thickness is a distributed parameter
    TEUCHOS_TEST_FOR_EXCEPTION (ss_requirements.find(basalSideName)==ss_requirements.end(), std::logic_error,
                                "Error! 'thickness' is a parameter, but there are no basal requirements.\n");
    const Albany::AbstractFieldContainer::FieldContainerRequirements& req = ss_requirements.at(basalSideName);

    TEUCHOS_TEST_FOR_EXCEPTION (std::find(req.begin(), req.end(), stateName)==req.end(), std::logic_error,
                                "Error! 'thickness' is a parameter, but is not listed as basal requirements.\n");

    // thickness is a distributed 3D parameter
    entity = Albany::StateStruct::NodalDistParameter;
    fieldName = "Ice Thickness Param";
    p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName, true, &entity, *meshPart);
    ev = evalUtils.constructGatherScalarExtruded2DNodalParameter(stateName,fieldName);
    fm0.template registerEvaluator<EvalT>(ev);

    std::stringstream key;
    key << stateName <<  "Is Distributed Parameter";
    this->params->set<int>(key.str(), 1);

    if (basalSideName!="INVALID")
    {
      // Interpolate the 3D state on the side (some evaluators need thickness as a side field)
      ev = evalUtilsBasal.constructDOFCellToSideEvaluator(fieldName,basalSideName,cellType);
      fm0.template registerEvaluator<EvalT> (ev);

      stateName = "thickness_side_avg";
      if (std::find(req.begin(),req.end(),stateName)!=req.end())
      {
        // We interpolate the thickness from quad point to cell
        ev = evalUtilsBasal.constructSideQuadPointsToSideInterpolationEvaluator (fieldName, basalSideName, false);
        fm0.template registerEvaluator<EvalT>(ev);

        // We save it on the basal mesh
        p = stateMgr.registerSideSetStateVariable(basalSideName, stateName, fieldName, dl_basal->side_scalar, basalEBName, true);
        p->set<bool>("Is Vector Field", false);
        ev = Teuchos::rcp(new PHAL::SaveSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p,dl_basal));
        fm0.template registerEvaluator<EvalT>(ev);
        if (fieldManagerChoice == Albany::BUILD_RESID_FM)
        {
          // Only PHAL::AlbanyTraits::Residual evaluates something
          if (ev->evaluatedFields().size()>0)
          {
            // Require save thickness
            fm0.template requireField<EvalT>(*ev->evaluatedFields()[0]);
          }
        }
      }
    }
  }
  else
  {
    // thickness is just an input field
    fieldName = "Ice Thickness";
    p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName, true, &entity);
    p->set<std::string>("Field Name", fieldName);
    ev = Teuchos::rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (ss_requirements.find(basalSideName)!=ss_requirements.end())
  {
    const Albany::AbstractFieldContainer::FieldContainerRequirements& req = ss_requirements.at(basalSideName);
    if (std::find(req.begin(), req.end(), stateName)!=req.end())
    {
      // ...and thickness is one of them.
      fieldName = "Ice Thickness";
      entity = Albany::StateStruct::NodalDataToElemNode;
      p = stateMgr.registerSideSetStateVariable(basalSideName, stateName, fieldName, dl_basal->side_node_scalar, basalEBName, true, &entity);
      ev = Teuchos::rcp(new PHAL::LoadSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
    }
  }

  // Basal friction
  stateName = "basal_friction";
  bool isBetaAParameter = false; // Determining whether basal friction is a distributed parameter
  if (this->params->isSublist("Distributed Parameters"))
  {
    Teuchos::ParameterList& dist_params_list =  this->params->sublist("Distributed Parameters");
    Teuchos::ParameterList* param_list;
    int numParams = dist_params_list.get<int>("Number of Parameter Vectors",0);
    for (int p_index=0; p_index< numParams; ++p_index)
    {
      std::string parameter_sublist_name = Albany::strint("Distributed Parameter", p_index);
      if (dist_params_list.isSublist(parameter_sublist_name))
      {
        param_list = &dist_params_list.sublist(parameter_sublist_name);
        if (param_list->get<std::string>("Name", emptyString) == stateName)
        {
          meshPart = &param_list->get<std::string>("Mesh Part",emptyString);
          isBetaAParameter = true;
          break;
        }
      }
      else
      {
        if (stateName == dist_params_list.get(Albany::strint("Parameter", p_index), emptyString))
        {
          isBetaAParameter = true;
          meshPart = &emptyString;
          break;
        }
      }
    }
  }

  if(isBetaAParameter)
  {
    //basal friction is a distributed parameter
    TEUCHOS_TEST_FOR_EXCEPTION (ss_requirements.find(basalSideName)==ss_requirements.end(), std::logic_error,
                                "Error! 'basal_friction' is a parameter, but there are no basal requirements.\n");
    const Albany::AbstractFieldContainer::FieldContainerRequirements& req = ss_requirements.at(basalSideName);

    TEUCHOS_TEST_FOR_EXCEPTION (std::find(req.begin(), req.end(), stateName)==req.end(), std::logic_error,
                                "Error! 'basal_friction' is a parameter, but is not listed as basal requirements.\n");

    //basal friction is a distributed 3D parameter
    entity = Albany::StateStruct::NodalDistParameter;
    fieldName = "Beta Given";
    p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName, true, &entity, *meshPart);
    ev = evalUtils.constructGatherScalarNodalParameter(stateName,fieldName);
    fm0.template registerEvaluator<EvalT>(ev);

    std::stringstream key;
    key << stateName <<  "Is Distributed Parameter";
    this->params->set<int>(key.str(), 1);

    if (basalSideName!="INVALID")
    {
      // Interpolate the 3D state on the side (the BasalFrictionCoefficient evaluator needs a side field)
      ev = evalUtilsBasal.constructDOFCellToSideEvaluator(fieldName,basalSideName,cellType);
      fm0.template registerEvaluator<EvalT> (ev);
    }
  }

  if (ss_requirements.find(basalSideName)!=ss_requirements.end())
  {
    const Albany::AbstractFieldContainer::FieldContainerRequirements& req = ss_requirements.at(basalSideName);
    if (std::find(req.begin(), req.end(), stateName)!=req.end())
    {
      // ...and basal_friction is one of them.
      entity = Albany::StateStruct::NodalDataToElemNode;
      p = stateMgr.registerSideSetStateVariable(basalSideName, stateName, fieldName, dl_basal->side_node_scalar, basalEBName, true, &entity);
      if (isBetaAParameter)
      {
        //basal friction is a distributed 3D parameter. We already took care of this case
      }
      else if (std::find(requirements.begin(),requirements.end(),stateName)==requirements.end())
      {
        //---- Load the side state
        ev = Teuchos::rcp(new PHAL::LoadSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);

        //---- Interpolate Beta Given on QP on side (may be used by a response)
        ev = evalUtilsBasal.getPSUtils().constructDOFInterpolationSideEvaluator(fieldName, basalSideName);
        fm0.template registerEvaluator<EvalT>(ev);
      }
    }

    // Check if the user also wants to save a side-averaged basal_friction
    stateName = "basal_friction_side_avg";
    fieldName = "Beta Given";
    if (std::find(req.begin(), req.end(), stateName)!=req.end())
    {
      // We interpolate the given beta from quad point to side
      ev = evalUtilsBasal.constructSideQuadPointsToSideInterpolationEvaluator (fieldName, basalSideName, false);
      fm0.template registerEvaluator<EvalT>(ev);

      // We save it on the basal mesh
      p = stateMgr.registerSideSetStateVariable(basalSideName, stateName, fieldName, dl_basal->side_scalar, basalEBName, true);
      p->set<bool>("Is Vector Field", false);
      ev = Teuchos::rcp(new PHAL::SaveSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p,dl_basal));
      fm0.template registerEvaluator<EvalT>(ev);
      if (fieldManagerChoice == Albany::BUILD_RESID_FM)
      {
        // Only PHAL::AlbanyTraits::Residual evaluates something
        if (ev->evaluatedFields().size()>0)
        {
          // Require save beta
          fm0.template requireField<EvalT>(*ev->evaluatedFields()[0]);
        }
      }
    }

    // Check if the user also wants to save a side-averaged beta
    stateName = "beta_side_avg";
    fieldName = "Beta";
    if (std::find(req.begin(), req.end(), stateName)!=req.end())
    {
      // We interpolate beta from quad point to cell
      ev = evalUtilsBasal.constructSideQuadPointsToSideInterpolationEvaluator (fieldName, basalSideName, false);
      fm0.template registerEvaluator<EvalT>(ev);

      // We save it on the basal mesh
      p = stateMgr.registerSideSetStateVariable(basalSideName, stateName, fieldName, dl_basal->side_scalar, basalEBName, true);
      p->set<bool>("Is Vector Field", false);
      ev = Teuchos::rcp(new PHAL::SaveSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p,dl_basal));
      fm0.template registerEvaluator<EvalT>(ev);
      if (fieldManagerChoice == Albany::BUILD_RESID_FM)
      {
        // Only PHAL::AlbanyTraits::Residual evaluates something
        if (ev->evaluatedFields().size()>0)
        {
          // Require save beta
          fm0.template requireField<EvalT>(*ev->evaluatedFields()[0]);
        }
      }
    }
  }

  stateName = "basal_friction";
  if (!isBetaAParameter && std::find(requirements.begin(),requirements.end(),stateName)!=requirements.end())
  {
    entity = Albany::StateStruct::NodalDataToElemNode;
    fieldName = "Beta";
    p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName, true, &entity);
    p->set<std::string>("Field Name", fieldName);

    // We are (for some mystic reason) extruding beta to the whole 3D mesh, even if it is not a parameter
    ev = Teuchos::rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    // We restrict it back to the 2D mesh. Clearly, this is not optimal. Just add 'basal_friction' to the Basal Requirements!
    if(basalSideName!="INVALID") {
      ev = evalUtilsBasal.constructDOFCellToSideEvaluator("Beta",basalSideName,cellType);
      fm0.template registerEvaluator<EvalT> (ev);
    }
  }

  // Effective pressure
  if (ss_requirements.find(basalSideName)!=ss_requirements.end())
  {
    stateName = "effective_pressure";
    fieldName = "Effective Pressure";
    const Albany::AbstractFieldContainer::FieldContainerRequirements& req = ss_requirements.at(basalSideName);

    auto it = std::find(req.begin(), req.end(), stateName);
    if (it!=req.end())
    {
      // We interpolate the effective pressure from quad point to cell (to then save it)
      ev = evalUtilsBasal.constructSideQuadPointsToSideInterpolationEvaluator (fieldName, basalSideName, false);
      fm0.template registerEvaluator<EvalT>(ev);

      // We register the state and build the loader
      p = stateMgr.registerSideSetStateVariable(basalSideName,stateName,fieldName, dl_basal->side_scalar, basalEBName, true);
      p->set<bool>("Is Vector Field", false);
      ev = Teuchos::rcp(new PHAL::SaveSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p,dl_basal));
      fm0.template registerEvaluator<EvalT>(ev);
      if (fieldManagerChoice == Albany::BUILD_RESID_FM)
      {
        // Only PHAL::AlbanyTraits::Residual evaluates something
        if (ev->evaluatedFields().size()>0)
        {
          // Require save beta
          fm0.template requireField<EvalT>(*ev->evaluatedFields()[0]);
        }
      }
    }
  }

  // Bed topography
  stateName = "bed_topography";
  entity= Albany::StateStruct::NodalDataToElemNode;
  p = stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName,true, &entity);
  ev = Teuchos::rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);

#if defined(CISM_HAS_FELIX) || defined(MPAS_HAS_FELIX)
  // Dirichelt field
  entity = Albany::StateStruct::NodalDistParameter;
  // Here is how to register the field for dirichlet condition.
  stateName = "dirichlet_field";
  p = stateMgr.registerStateVariable(stateName, dl->node_vector, elementBlockName, true, &entity, "");
  ev = Teuchos::rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);
#endif

  // Basal friction sensitivity
  stateName = "basal_friction_sensitivity";
  entity = Albany::StateStruct::NodalDistParameter;
  stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName,true, &entity,"bottom");

  // Thickness sensitivity
  stateName = "thickness_sensitivity";
  entity = Albany::StateStruct::NodalDistParameter;
  stateMgr.registerStateVariable(stateName, dl->node_scalar, elementBlockName,true, &entity,"bottom");

  // ----------  Define Field Names ----------- //
  Teuchos::ArrayRCP<std::string> dof_names(1);
  Teuchos::ArrayRCP<std::string> resid_names(1);
  Teuchos::RCP<std::map<std::string, int> > extruded_params_levels = Teuchos::rcp(new std::map<std::string, int> ());
  dof_names[0] = "Velocity";
  resid_names[0] = "Stokes Residual";
  if(isThicknessAParameter)
  {
    std::string extruded_param_name = "thickness";
    int extruded_param_level = 0;
    extruded_params_levels->insert(std::make_pair(extruded_param_name, extruded_param_level));
  }

  // ------------------- Interpolations and utilities ------------------ //

  // Gather solution field
  ev = evalUtils.constructGatherSolutionEvaluator_noTransient(true, dof_names, offset);
  fm0.template registerEvaluator<EvalT> (ev);

  // Interpolate solution field
  ev = evalUtils.constructDOFVecInterpolationEvaluator(dof_names[0]);
  fm0.template registerEvaluator<EvalT> (ev);

  // Interpolate solution gradient
  ev = evalUtils.constructDOFVecGradInterpolationEvaluator(dof_names[0]);
  fm0.template registerEvaluator<EvalT> (ev);

  // Scatter residual
  ev = evalUtils.constructScatterResidualEvaluatorWithExtrudedParams(true, resid_names, extruded_params_levels, offset, "Scatter Stokes");
  fm0.template registerEvaluator<EvalT> (ev);

  // Interpolate temperature from nodes to cell
  ev = evalUtils.getPSUtils().constructNodesToCellInterpolationEvaluator ("temperature",false);
  fm0.template registerEvaluator<EvalT> (ev);

  if(!isThicknessAParameter)
  {
    //----- Gather Coordinate Vector (general parameters)
    ev = evalUtils.constructGatherCoordinateVectorEvaluator();
    fm0.template registerEvaluator<EvalT> (ev);
  }
  else
  {
#ifndef ALBANY_MESH_DEPENDS_ON_PARAMETERS
    //----- Gather Coordinate Vector (general parameters)
    ev = evalUtils.constructGatherCoordinateVectorEvaluator();
    fm0.template registerEvaluator<EvalT> (ev);
#else

    //----- Gather Coordinate Vector (ad hoc parameters)
    p = Teuchos::rcp(new Teuchos::ParameterList("Gather Coordinate Vector"));

    // Input: Periodic BC flag
    p->set<bool>("Periodic BC", false);

    // Output:: Coordindate Vector at vertices
    p->set<std::string>("Coordinate Vector Name", "Coord Vec Old");

    ev = Teuchos::rcp(new PHAL::GatherCoordinateVector<EvalT,PHAL::AlbanyTraits>(*p,dl));
    fm0.template registerEvaluator<EvalT>(ev);

    //------ Update Z Coordinate
    p = Teuchos::rcp(new Teuchos::ParameterList("Update Z Coordinate"));

    p->set<std::string>("Old Coords Name", "Coord Vec Old");
    p->set<std::string>("New Coords Name", "Coord Vec");
    p->set<std::string>("Past Thickness Name", "Ice Thickness Param");
    p->set<std::string>("Top Surface Name", "Surface Height");

    ev = Teuchos::rcp(new FELIX::UpdateZCoordinateMovingBed<EvalT,PHAL::AlbanyTraits>(*p, dl));
    fm0.template registerEvaluator<EvalT>(ev);
#endif
  }

  // Map to physical frame
  ev = evalUtils.constructMapToPhysicalFrameEvaluator(cellType, cellCubature);
  fm0.template registerEvaluator<EvalT> (ev);

  // Compute basis funcitons
  ev = evalUtils.constructComputeBasisFunctionsEvaluator(cellType, cellBasis, cellCubature);
  fm0.template registerEvaluator<EvalT> (ev);

  // Intepolate surface height
  ev = evalUtils.constructDOFInterpolationEvaluator_noDeriv("Surface Height");
  fm0.template registerEvaluator<EvalT> (ev);

  // Intepolate surface height gradient
  ev = evalUtils.constructDOFGradInterpolationEvaluator_noDeriv("Surface Height");
  fm0.template registerEvaluator<EvalT> (ev);

  if (basalSideName!="INVALID")
  {
    // -------------------- Special evaluators for side handling ----------------- //

    //---- Compute side basis functions
    ev = evalUtilsBasal.constructComputeBasisFunctionsSideEvaluator(cellType, basalSideBasis, basalCubature, basalSideName);
    fm0.template registerEvaluator<EvalT> (ev);

    //---- Compute Quad Points coordinates on the side set
    ev = evalUtilsBasal.constructMapToPhysicalFrameSideEvaluator(cellType,basalCubature,basalSideName);
    fm0.template registerEvaluator<EvalT> (ev);

    //---- Restrict velocity from cell-based to cell-side-based
    ev = evalUtilsBasal.constructDOFVecCellToSideEvaluator("Velocity",basalSideName,cellType,"Basal Velocity");
    fm0.template registerEvaluator<EvalT> (ev);

    //---- Interpolate velocity on QP on side
    ev = evalUtilsBasal.constructDOFVecInterpolationSideEvaluator("Basal Velocity", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Interpolate velocity gradient on QP on side
    ev = evalUtilsBasal.constructDOFVecGradInterpolationSideEvaluator("Basal Velocity", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Restrict ice thickness from cell-based to cell-side-based
    ev = evalUtilsBasal.getPSUtils().constructDOFCellToSideEvaluator("Ice Thickness",basalSideName,cellType);
    fm0.template registerEvaluator<EvalT> (ev);

    //---- Interpolate thickness gradient on QP on side
    ev = evalUtilsBasal.getPSUtils().constructDOFGradInterpolationSideEvaluator("Ice Thickness", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Interpolate thickness on QP on side
    ev = evalUtilsBasal.getPSUtils().constructDOFInterpolationSideEvaluator("Ice Thickness Param", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Restrict ice thickness (param) from cell-based to cell-side-based
    ev = evalUtilsBasal.getPSUtils().constructDOFCellToSideEvaluator("Ice Thickness Param",basalSideName,cellType);
    fm0.template registerEvaluator<EvalT> (ev);

    //---- Interpolate thickness (param) gradient on QP on side
    ev = evalUtilsBasal.constructDOFGradInterpolationSideEvaluator("Ice Thickness Param", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Interpolate observed thickness on QP on side
    ev = evalUtilsBasal.getPSUtils().constructDOFInterpolationSideEvaluator("Observed Ice Thickness", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Interpolate thickness on QP on side
    ev = evalUtilsBasal.getPSUtils().constructDOFInterpolationSideEvaluator("Ice Thickness", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Interpolate thickness RMS on QP on side
    ev = evalUtilsBasal.getPSUtils().constructDOFInterpolationSideEvaluator("Ice Thickness RMS", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Interpolate effective pressure on QP on side
    ev = evalUtilsBasal.constructDOFInterpolationSideEvaluator("Effective Pressure", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Restrict surface height from cell-based to cell-side-based
    ev = evalUtilsBasal.getPSUtils().constructDOFCellToSideEvaluator("Surface Height",basalSideName,cellType);
    fm0.template registerEvaluator<EvalT> (ev);

    //---- Interpolate surface height on QP on side
    ev = evalUtilsBasal.getPSUtils().constructDOFInterpolationSideEvaluator("Surface Height", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    // Interpolate the 3D state on the side (the BasalFrictionCoefficient evaluator needs a side field)
    ev = evalUtilsBasal.constructDOFVecCellToSideEvaluator("Averaged Velocity",basalSideName,cellType);
    fm0.template registerEvaluator<EvalT> (ev);

    //---- Interpolate surface height on QP on side
    ev = evalUtilsBasal.constructDOFDivInterpolationSideEvaluator("Averaged Velocity", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Interpolate velocity on QP on side
    ev = evalUtilsBasal.constructDOFVecInterpolationSideEvaluator("Averaged Velocity", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Interpolate surface velocity on QP on side
    ev = evalUtilsBasal.getPSUtils().constructDOFInterpolationSideEvaluator("Surface Mass Balance", basalSideName);
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (surfaceSideName!="INVALID")
  {
    //---- Compute side basis functions
    ev = evalUtilsSurface.constructComputeBasisFunctionsSideEvaluator(cellType, surfaceSideBasis, surfaceCubature, surfaceSideName);
    fm0.template registerEvaluator<EvalT> (ev);

    //---- Interpolate surface velocity on QP on side
    ev = evalUtilsSurface.getPSUtils().constructDOFVecInterpolationSideEvaluator("Observed Surface Velocity", surfaceSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Interpolate surface velocity rms on QP on side
    ev = evalUtilsSurface.getPSUtils().constructDOFVecInterpolationSideEvaluator("Observed Surface Velocity RMS", surfaceSideName);
    fm0.template registerEvaluator<EvalT>(ev);

    //---- Restrict velocity (the solution) from cell-based to cell-side-based on upper side
    ev = evalUtilsSurface.constructDOFVecCellToSideEvaluator("Velocity",surfaceSideName,cellType,"Surface Velocity");
    fm0.template registerEvaluator<EvalT> (ev);

    //---- Interpolate velocity (the solution) on QP on side
    ev = evalUtilsSurface.constructDOFVecInterpolationSideEvaluator("Surface Velocity", surfaceSideName);
    fm0.template registerEvaluator<EvalT>(ev);
  }

  // -------------------------------- FELIX evaluators ------------------------- //

  // --- FO Stokes Resid --- //
  p = Teuchos::rcp(new Teuchos::ParameterList("Stokes Resid"));

  //Input
  p->set<std::string>("Weighted BF Variable Name", "wBF");
  p->set<std::string>("Weighted Gradient BF Variable Name", "wGrad BF");
  p->set<std::string>("Velocity QP Variable Name", "Velocity");
  p->set<std::string>("Velocity Gradient QP Variable Name", "Velocity Gradient");
  p->set<std::string>("Body Force Variable Name", "Body Force");
  p->set<std::string>("Viscosity QP Variable Name", "FELIX Viscosity");
  p->set<std::string>("Coordinate Vector Name", "Coord Vec");
  p->set<Teuchos::ParameterList*>("Stereographic Map", &params->sublist("Stereographic Map"));
  p->set<Teuchos::ParameterList*>("Parameter List", &params->sublist("Equation Set"));
  p->set<std::string>("Basal Residual Variable Name", "Basal Residual");
  p->set<bool>("Needs Basal Residual", sliding);

  //Output
  p->set<std::string>("Residual Variable Name", "Stokes Residual");

  ev = Teuchos::rcp(new FELIX::StokesFOResid<EvalT,PHAL::AlbanyTraits>(*p,dl));
  fm0.template registerEvaluator<EvalT>(ev);

  if (sliding)
  {
    // --- Basal Residual --- //
    p = Teuchos::rcp(new Teuchos::ParameterList("Stokes Basal Residual"));

    //Input
    p->set<std::string>("BF Side Name", "BF "+basalSideName);
    p->set<std::string>("Weighted Measure Name", "Weighted Measure "+basalSideName);
    p->set<std::string>("Basal Friction Coefficient Side QP Variable Name", "Beta");
    p->set<std::string>("Velocity Side QP Variable Name", "Basal Velocity");
    p->set<std::string>("Side Set Name", basalSideName);
    p->set<Teuchos::RCP<shards::CellTopology> >("Cell Type", cellType);
    p->set<Teuchos::ParameterList*>("Parameter List", &params->sublist("FELIX Basal Friction Coefficient"));

    //Output
    p->set<std::string>("Basal Residual Variable Name", "Basal Residual");

    ev = Teuchos::rcp(new FELIX::StokesFOBasalResid<EvalT,PHAL::AlbanyTraits>(*p,dl_basal));
    fm0.template registerEvaluator<EvalT>(ev);

    //--- Sliding velocity calculation ---//
    p = Teuchos::rcp(new Teuchos::ParameterList("FELIX Velocity Norm"));

    // Input
    p->set<std::string>("Field Name","Basal Velocity");
    p->set<std::string>("Field Layout","Cell Side QuadPoint");
    p->set<std::string>("Side Set Name", basalSideName);

    // Output
    p->set<std::string>("Field Norm Name","Sliding Velocity");

    ev = Teuchos::rcp(new FELIX::FieldNorm<EvalT,PHAL::AlbanyTraits>(*p,dl_basal));
    fm0.template registerEvaluator<EvalT>(ev);

    //--- Effective pressure (surrogate) calculation ---//
    p = Teuchos::rcp(new Teuchos::ParameterList("FELIX Effective Pressure Surrogate"));

    // Input
    p->set<std::string>("Hydrostatic Potential Variable Name","Subglacial Hydrostatic Potential");
    p->set<std::string>("Ice Thickness Variable Name", "Ice Thickness");
    p->set<std::string>("Side Set Name", basalSideName);
    p->set<bool>("Surrogate", true);
    p->set<bool>("Stokes", true);
    p->set<Teuchos::ParameterList*>("FELIX Physical Parameters", &params->sublist("FELIX Physical Parameters"));
    p->set<Teuchos::ParameterList*>("Parameter List", &params->sublist("FELIX Basal Friction Coefficient"));

    // Output
    p->set<std::string>("Effective Pressure Variable Name","Effective Pressure");

    ev = Teuchos::rcp(new FELIX::EffectivePressure<EvalT,PHAL::AlbanyTraits>(*p,dl_basal));
    fm0.template registerEvaluator<EvalT>(ev);

    //--- Shared Parameter for basal friction coefficient: alpha ---//
    p = Teuchos::rcp(new Teuchos::ParameterList("Basal Friction Coefficient: alpha"));

    param_name = "Hydraulic-Over-Hydrostatic Potential Ratio";
    p->set<std::string>("Parameter Name", param_name);
    p->set< Teuchos::RCP<ParamLib> >("Parameter Library", paramLib);

    Teuchos::RCP<FELIX::SharedParameter<EvalT,PHAL::AlbanyTraits,StokesParamEnum,Alpha>> ptr_alpha;
    ptr_alpha = Teuchos::rcp(new FELIX::SharedParameter<EvalT,PHAL::AlbanyTraits,StokesParamEnum,Alpha>(*p,dl));
    ptr_alpha->setNominalValue(params->sublist("FELIX Basal Friction Coefficient").get<double>(param_name,-1.0));
    fm0.template registerEvaluator<EvalT>(ptr_alpha);

    //--- Shared Parameter for basal friction coefficient: lambda ---//
    p = Teuchos::rcp(new Teuchos::ParameterList("Basal Friction Coefficient: lambda"));

    param_name = "Bed Roughness";
    p->set<std::string>("Parameter Name", param_name);
    p->set< Teuchos::RCP<ParamLib> >("Parameter Library", paramLib);

    Teuchos::RCP<FELIX::SharedParameter<EvalT,PHAL::AlbanyTraits,StokesParamEnum,Lambda>> ptr_lambda;
    ptr_lambda = Teuchos::rcp(new FELIX::SharedParameter<EvalT,PHAL::AlbanyTraits,StokesParamEnum,Lambda>(*p,dl));
    ptr_lambda->setNominalValue(params->sublist("FELIX Basal Friction Coefficient").get<double>(param_name,0.0));
    fm0.template registerEvaluator<EvalT>(ptr_lambda);

    //--- Shared Parameter for basal friction coefficient: mu ---//
    p = Teuchos::rcp(new Teuchos::ParameterList("Basal Friction Coefficient: mu"));

    param_name = "Coulomb Friction Coefficient";
    p->set<std::string>("Parameter Name", param_name);
    p->set< Teuchos::RCP<ParamLib> >("Parameter Library", paramLib);

    Teuchos::RCP<FELIX::SharedParameter<EvalT,PHAL::AlbanyTraits,StokesParamEnum,Mu>> ptr_mu;
    ptr_mu = Teuchos::rcp(new FELIX::SharedParameter<EvalT,PHAL::AlbanyTraits,StokesParamEnum,Mu>(*p,dl));
    ptr_mu->setNominalValue(params->sublist("FELIX Basal Friction Coefficient").get<double>(param_name,-1.0));
    fm0.template registerEvaluator<EvalT>(ptr_mu);

    //--- Shared Parameter for basal friction coefficient: power ---//
    p = Teuchos::rcp(new Teuchos::ParameterList("Basal Friction Coefficient: power"));

    param_name = "Power Exponent";
    p->set<std::string>("Parameter Name", param_name);
    p->set<double>("Parameter Value", params->sublist("FELIX Basal Friction Coefficient").get<double>(param_name,0.0));
    p->set< Teuchos::RCP<PHX::DataLayout> >("Data Layout", dl_basal->shared_param);
    p->set< Teuchos::RCP<ParamLib> >("Parameter Library", paramLib);

    Teuchos::RCP<FELIX::SharedParameter<EvalT,PHAL::AlbanyTraits,StokesParamEnum,Power>> ptr_power;
    ptr_power = Teuchos::rcp(new FELIX::SharedParameter<EvalT,PHAL::AlbanyTraits,StokesParamEnum,Power>(*p,dl));
    ptr_power->setNominalValue(params->sublist("FELIX Basal Friction Coefficient").get<double>(param_name,-1.0));
    fm0.template registerEvaluator<EvalT>(ptr_power);

    //--- FELIX basal friction coefficient ---//
    p = Teuchos::rcp(new Teuchos::ParameterList("FELIX Basal Friction Coefficient"));

    //Input
    p->set<std::string>("Sliding Velocity Side QP Variable Name", "Sliding Velocity");
    p->set<std::string>("BF Side Variable Name", "BF " + basalSideName);
    p->set<std::string>("Effective Pressure Side QP Variable Name", "Effective Pressure");
    p->set<std::string>("Side Set Name", basalSideName);
    p->set<std::string>("Coordinate Vector Variable Name", "Coord Vec " + basalSideName);
    p->set<Teuchos::ParameterList*>("Parameter List", &params->sublist("FELIX Basal Friction Coefficient"));
    p->set<Teuchos::ParameterList*>("Stereographic Map", &params->sublist("Stereographic Map"));

    //Output
    p->set<std::string>("Basal Friction Coefficient Variable Name", "Beta");

    ev = Teuchos::rcp(new FELIX::BasalFrictionCoefficient<EvalT,PHAL::AlbanyTraits>(*p,dl_basal));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (basalSideName!="INVALID")
  {
    fieldName = "Flux Divergence";
    stateName = "flux_divergence";
    p = Teuchos::rcp(new Teuchos::ParameterList("Flux Divergence"));

    //Input
    p->set<std::string>("Averaged Velocity Side QP Variable Name", "Averaged Velocity");
    p->set<std::string>("Averaged Velocity Side QP Divergence Name", "Averaged Velocity Divergence");
    if(isThicknessAParameter) {
      p->set<std::string>("Thickness Side QP Variable Name", "Ice Thickness Param");
      p->set<std::string>("Thickness Gradient Name", "Ice Thickness Param Gradient");
    } else {
      p->set<std::string>("Thickness Side QP Variable Name", "Ice Thickness");
      p->set<std::string>("Thickness Gradient Name", "Ice Thickness Gradient");
    }

    p->set<std::string>("Field Name",  "Flux Divergence");
    p->set<std::string> ("Side Set Name", basalSideName);

    ev = Teuchos::rcp(new FELIX::FluxDiv<EvalT,PHAL::AlbanyTraits>(*p,dl_basal));
    fm0.template registerEvaluator<EvalT>(ev);
  }

#ifdef ALBANY_EPETRA
  p = Teuchos::rcp(new Teuchos::ParameterList("Gather Averaged Velocity"));
  p->set<std::string>("Averaged Velocity Name", "Averaged Velocity");
  p->set<std::string>("Mesh Part", "basalside");
  p->set<Teuchos::RCP<const CellTopologyData> >("Cell Topology",Teuchos::rcp(new CellTopologyData(meshSpecs.ctd)));
  ev = Teuchos::rcp(new GatherVerticallyAveragedVelocity<EvalT,PHAL::AlbanyTraits>(*p,dl));
  fm0.template registerEvaluator<EvalT>(ev);
#endif

  //--- FELIX viscosity ---//
  p = Teuchos::rcp(new Teuchos::ParameterList("FELIX Viscosity"));

  //Input
  p->set<std::string>("Coordinate Vector Variable Name", "Coord Vec");
  p->set<std::string>("Velocity QP Variable Name", "Velocity");
  p->set<std::string>("Velocity Gradient QP Variable Name", "Velocity Gradient");
  p->set<std::string>("Temperature Variable Name", "temperature");
  p->set<std::string>("Flow Factor Variable Name", "flow_factor");
  p->set<Teuchos::RCP<ParamLib> >("Parameter Library", paramLib);
  p->set<Teuchos::ParameterList*>("Stereographic Map", &params->sublist("Stereographic Map"));
  p->set<Teuchos::ParameterList*>("Parameter List", &params->sublist("FELIX Viscosity"));

  //Output
  p->set<std::string>("Viscosity QP Variable Name", "FELIX Viscosity");

  ev = Teuchos::rcp(new FELIX::ViscosityFO<EvalT,PHAL::AlbanyTraits>(*p,dl));
  fm0.template registerEvaluator<EvalT>(ev);

#ifdef CISM_HAS_FELIX
  //--- FELIX surface gradient from CISM ---//
  p = Teuchos::rcp(new Teuchos::ParameterList("FELIX Surface Gradient"));

  //Input
  p->set<std::string>("CISM Surface Height Gradient X Variable Name", "CISM Surface Height Gradient X");
  p->set<std::string>("CISM Surface Height Gradient Y Variable Name", "CISM Surface Height Gradient X");
  p->set<std::string>("BF Variable Name", "BF");

  //Output
  p->set<std::string>("Surface Height Gradient QP Variable Name", "CISM Surface Height Gradient");
  ev = Teuchos::rcp(new FELIX::CismSurfaceGradFO<EvalT,PHAL::AlbanyTraits>(*p,dl));
  fm0.template registerEvaluator<EvalT>(ev);
#endif

  //--- Body Force ---//
  p = Teuchos::rcp(new Teuchos::ParameterList("Body Force"));

  //Input
  p->set<std::string>("FELIX Viscosity QP Variable Name", "FELIX Viscosity");
#ifdef CISM_HAS_FELIX
  p->set<std::string>("Surface Height Gradient QP Variable Name", "CISM Surface Height Gradient");
#endif
  p->set<std::string>("Coordinate Vector Variable Name", "Coord Vec");
  p->set<std::string>("Surface Height Gradient Name", "Surface Height Gradient");
  p->set<std::string>("Surface Height Name", "Surface Height");
  p->set<Teuchos::ParameterList*>("Parameter List", &params->sublist("Body Force"));
  p->set<Teuchos::ParameterList*>("Stereographic Map", &params->sublist("Stereographic Map"));
  p->set<Teuchos::ParameterList*>("Physical Parameter List", &params->sublist("FELIX Physical Parameters"));

  //Output
  p->set<std::string>("Body Force Variable Name", "Body Force");

  ev = Teuchos::rcp(new FELIX::StokesFOBodyForce<EvalT,PHAL::AlbanyTraits>(*p,dl));
  fm0.template registerEvaluator<EvalT>(ev);

  if (surfaceSideName!="INVALID")
  {
    // Load surface velocity
    entity= Albany::StateStruct::NodalDataToElemNode;
    stateName = "surface_velocity";
    fieldName = "Observed Surface Velocity";
    p = stateMgr.registerSideSetStateVariable(surfaceSideName, stateName, fieldName, dl_surface->side_node_vector, surfaceEBName, true, &entity);
    ev = Teuchos::rcp(new PHAL::LoadSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    // Load surface velocity rms
    entity= Albany::StateStruct::NodalDataToElemNode;
    stateName = "surface_velocity_rms";
    fieldName = "Observed Surface Velocity RMS";
    p = stateMgr.registerSideSetStateVariable(surfaceSideName, stateName, fieldName, dl_surface->side_node_vector, surfaceEBName, true, &entity);
    ev = Teuchos::rcp(new PHAL::LoadSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (basalSideName!="INVALID")
  {
    //--- FELIX basal friction coefficient gradient ---//
    p = Teuchos::rcp(new Teuchos::ParameterList("FELIX Basal Friction Coefficient Gradient"));

    // Input
    p->set<std::string>("Given Beta Variable Name", "Beta");
    p->set<std::string>("Gradient BF Side Variable Name", "Grad BF "+basalSideName);
    p->set<std::string>("Side Set Name", basalSideName);
    p->set<std::string>("Effective Pressure QP Name", "Effective Pressure");
    p->set<std::string>("Effective Pressure Gradient QP Name", "Effective Pressure Gradient");
    p->set<std::string>("Basal Velocity QP Name", "Basal Velocity");
    p->set<std::string>("Basal Velocity Gradient QP Name", "Basal Velocity Gradient");
    p->set<std::string>("Sliding Velocity QP Name", "Sliding Velocity");
    p->set<std::string>("Coordinate Vector Variable Name", "Coord Vec");
    p->set<Teuchos::ParameterList*>("Stereographic Map", &params->sublist("Stereographic Map"));
    p->set<Teuchos::ParameterList*>("Parameter List", &params->sublist("FELIX Basal Friction Coefficient"));

    // Output
    p->set<std::string>("Basal Friction Coefficient Gradient Name","Beta Gradient");

    ev = Teuchos::rcp(new FELIX::BasalFrictionCoefficientGradient<EvalT,PHAL::AlbanyTraits>(*p,dl_basal));
    fm0.template registerEvaluator<EvalT>(ev);

    // Load surface mass balance
    entity= Albany::StateStruct::NodalDataToElemNode;
    stateName = "surface_mass_balance";
    fieldName = "Surface Mass Balance";
    p = stateMgr.registerSideSetStateVariable(basalSideName, stateName, fieldName, dl_basal->side_node_scalar, basalEBName, true, &entity);
    ev = Teuchos::rcp(new PHAL::LoadSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    // Load surface mass balance RMS
    entity= Albany::StateStruct::NodalDataToElemNode;
    stateName = "surface_mass_balance_RMS";
    fieldName = "Surface Mass Balance RMS";
    p = stateMgr.registerSideSetStateVariable(basalSideName, stateName, fieldName, dl_basal->side_node_scalar, basalEBName, true, &entity);
    ev = Teuchos::rcp(new PHAL::LoadSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    // Load observed thickness
    entity = Albany::StateStruct::NodalDataToElemNode;
    stateName = "observed_thickness";
    fieldName = "Observed Ice Thickness";
    p = stateMgr.registerSideSetStateVariable(basalSideName, stateName, fieldName, dl_basal->side_node_scalar, basalEBName, true, &entity);
    ev = Teuchos::rcp(new PHAL::LoadSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    // Load thickness RMS
    entity= Albany::StateStruct::NodalDataToElemNode;
    stateName = "thickness_RMS";
    fieldName = "Ice Thickness RMS";
    p = stateMgr.registerSideSetStateVariable(basalSideName, stateName, fieldName, dl_basal->side_node_scalar, basalEBName, true, &entity);
    ev = Teuchos::rcp(new PHAL::LoadSideSetStateField<EvalT,PHAL::AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (fieldManagerChoice == Albany::BUILD_RESID_FM)
  {
    // Require scattering of residual
    PHX::Tag<typename EvalT::ScalarT> res_tag("Scatter Stokes", dl->dummy);
    fm0.requireField<EvalT>(res_tag);
  }
  else if (fieldManagerChoice == Albany::BUILD_RESPONSE_FM)
  {
    // ----------------------- Responses --------------------- //
    Teuchos::RCP<Teuchos::ParameterList> paramList = Teuchos::rcp(new Teuchos::ParameterList("Param List"));
    paramList->set<Teuchos::RCP<ParamLib> >("Parameter Library", paramLib);
    paramList->set<std::string>("Basal Friction Coefficient Gradient Name","Beta Gradient");
    paramList->set<std::string>("Thickness Gradient Name","Ice Thickness Param Gradient");
    paramList->set<std::string>("Surface Velocity Side QP Variable Name","Surface Velocity");
    paramList->set<std::string>("SMB Side QP Variable Name","Surface Mass Balance");
    paramList->set<std::string>("SMB RMS Side QP Variable Name","Surface Mass Balance RMS");
    paramList->set<std::string>("Flux Divergence Side QP Variable Name","Flux Divergence");
    paramList->set<std::string>("Thickness Side QP Variable Name","Ice Thickness Param");
    paramList->set<std::string>("Thickness RMS Side QP Variable Name","Ice Thickness RMS");
    paramList->set<std::string>("Observed Thickness Side QP Variable Name","Observed Ice Thickness");
    paramList->set<std::string>("Observed Surface Velocity Side QP Variable Name","Observed Surface Velocity");
    paramList->set<std::string>("Observed Surface Velocity RMS Side QP Variable Name","Observed Surface Velocity RMS");
    paramList->set<std::string>("BF Surface Name","BF " + surfaceSideName);
    paramList->set<std::string>("Weighted Measure Basal Name","Weighted Measure " + basalSideName);
    paramList->set<std::string>("Weighted Measure 2D Name","Weighted Measure " + basalSideName);
    paramList->set<std::string>("Weighted Measure Surface Name","Weighted Measure " + surfaceSideName);
    paramList->set<std::string>("Inverse Metric Basal Name","Inv Metric " + basalSideName);
    paramList->set<std::string>("Basal Side Name", basalSideName);
    paramList->set<std::string>("Surface Side Name", surfaceSideName);
    paramList->set<Teuchos::RCP<const CellTopologyData> >("Cell Topology",Teuchos::rcp(new CellTopologyData(meshSpecs.ctd)));

    Albany::ResponseUtilities<EvalT, PHAL::AlbanyTraits> respUtils(dls);
    return respUtils.constructResponses(fm0, *responseList, paramList, stateMgr);
  }

  return Teuchos::null;
}

#endif // FELIX_STOKES_FO_PROBLEM_HPP
