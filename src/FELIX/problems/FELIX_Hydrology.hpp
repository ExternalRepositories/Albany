//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef FELIX_HYDROLOGY_PROBLEM_HPP
#define FELIX_HYDROLOGY_PROBLEM_HPP 1

#include "Intrepid_FieldContainer.hpp"
#include "Intrepid_DefaultCubatureFactory.hpp"
#include "Phalanx.hpp"
#include "Shards_CellTopology.hpp"
#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"
#include "Albany_EvaluatorUtils.hpp"
#include "Albany_ResponseUtilities.hpp"

#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"
#include "PHAL_SaveStateField.hpp"
#include "PHAL_LoadStateField.hpp"

#include "FELIX_BasalFrictionCoefficient.hpp"
#include "FELIX_FieldNorm.hpp"
#include "FELIX_HydrologyWaterDischarge.hpp"
#include "FELIX_HydrologyHydrostaticPotential.hpp"
#include "FELIX_HydrologyMelting.hpp"
#include "FELIX_HydrologyResidualEllipticEqn.hpp"
#include "FELIX_HydrologyResidualEvolutionEqn.hpp"

//uncomment the following line if you want debug output to be printed to screen
//#define OUTPUT_TO_SCREEN

namespace FELIX
{

/*!
 * \brief  A 2D problem for the subglacial hydrology
 */
class Hydrology : public Albany::AbstractProblem
{
public:

  //! Default constructor
  Hydrology (const Teuchos::RCP<Teuchos::ParameterList>& params,
             const Teuchos::RCP<ParamLib>& paramLib,
             const int numDimensions);

  //! Destructor
  virtual ~Hydrology();

  //! Return number of spatial dimensions
  virtual int spatialDimension () const
  {
      return numDim;
  }

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

  //! Main problem setup routine. Not directly called, but indirectly by buildEvaluators
  template <typename EvalT> Teuchos::RCP<const PHX::FieldTag>
  constructEvaluators (PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
                       const Albany::MeshSpecsStruct& meshSpecs,
                       Albany::StateManager& stateMgr,
                       Albany::FieldManagerChoice fmchoice,
                       const Teuchos::RCP<Teuchos::ParameterList>& responseList);

  //! Boundary conditions evaluators
  void constructDirichletEvaluators (const Albany::MeshSpecsStruct& meshSpecs);
  void constructNeumannEvaluators   (const Teuchos::RCP<Albany::MeshSpecsStruct>& meshSpecs);

protected:

  bool has_evolution_equation;
  int numDim;

  Teuchos::ArrayRCP<std::string> dof_names;
  Teuchos::ArrayRCP<std::string> dof_names_dot;
  Teuchos::ArrayRCP<std::string> resid_names;

  Teuchos::RCP<Albany::Layouts> dl;

  Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > intrepidBasis;
};

// ===================================== IMPLEMENTATION ======================================= //

template <typename EvalT>
Teuchos::RCP<const PHX::FieldTag>
Hydrology::constructEvaluators (PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
                                const Albany::MeshSpecsStruct& meshSpecs,
                                Albany::StateManager& stateMgr,
                                Albany::FieldManagerChoice fieldManagerChoice,
                                const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
  using Teuchos::RCP;
  using Teuchos::rcp;
  using PHX::DataLayout;

  // Retrieving FE information (basis and cell type)
  if (intrepidBasis.get()==0)
  {
    intrepidBasis = Albany::getIntrepidBasis(meshSpecs.ctd);
  }

  RCP<shards::CellTopology> cellType = rcp(new shards::CellTopology (&meshSpecs.ctd));

  // Building the right quadrature formula
  Intrepid::DefaultCubatureFactory<RealType> cubFactory;
  RCP <Intrepid::Cubature<RealType> > cubature = cubFactory.create(*cellType, meshSpecs.cubatureDegree);

  if (dl.get()==0)
  {
    // Some constants
    const int numNodes = intrepidBasis->getCardinality();
    const int worksetSize = meshSpecs.worksetSize;
    const int numQPts = cubature->getNumPoints();
    const int numVertices = cellType->getNodeCount();

#ifdef OUTPUT_TO_SCREEN
    *out << "Hydrology Field Dimensions: Workset=" << worksetSize
         << ", Vertices= " << numVertices
         << ", Nodes= " << numNodes
         << ", QuadPts= " << numQPts
         << ", Dim= " << numDim << std::endl;
#endif

    // Building the data layout
    dl = rcp(new Albany::Layouts(worksetSize,numVertices,numNodes,numQPts,numDim));
  }

  const std::string& elementBlockName = meshSpecs.ebName;
  int offset = 0;

  // Using the utility for the common evaluators
  Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils(dl);

  // Service variables for registering state variables and evaluators
  Albany::StateStruct::MeshFieldEntity entity;
  RCP<PHX::Evaluator<PHAL::AlbanyTraits> > ev;
  RCP<Teuchos::ParameterList> p;

  // -------------- Registering state variables --------------- //

  // Surface height
  entity = Albany::StateStruct::NodalDataToElemNode;
  p = stateMgr.registerStateVariable("surface_height", dl->node_scalar, elementBlockName, true, &entity);
  p->set<const std::string>("Field Name","Surface Height");
  ev = rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);

  // Basal velocity
  entity = Albany::StateStruct::NodalDataToElemNode;
  p = stateMgr.registerStateVariable("sliding_velocity", dl->node_vector, elementBlockName, true, &entity);
  p->set<const std::string>("Field Name","Basal Velocity");
  ev = rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);

  // Ice thickness
  entity = Albany::StateStruct::NodalDataToElemNode;
  p = stateMgr.registerStateVariable("ice_thickness", dl->node_scalar, elementBlockName, true, &entity);
  p->set<const std::string>("Field Name","H");
  ev = rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);

  // Surface water input
  entity = Albany::StateStruct::NodalDataToElemNode;
  p = stateMgr.registerStateVariable("surface_water_input", dl->node_scalar, elementBlockName, true, &entity);
  p->set<const std::string>("Field Name","Surface Water Input");
  ev = rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);

  // GeothermaL flux
  entity = Albany::StateStruct::NodalDataToElemNode;
  p = stateMgr.registerStateVariable("geothermal_flux", dl->node_scalar, elementBlockName, true, &entity);
  p->set<const std::string>("Field Name","Geothermal Flux");
  ev = rcp(new PHAL::LoadStateField<EvalT,PHAL::AlbanyTraits>(*p));
  fm0.template registerEvaluator<EvalT>(ev);

  // -------------------- Interpolation and utilities ------------------------ //

  // Gather solution field
  if (has_evolution_equation)
  {
    ev = evalUtils.constructGatherSolutionEvaluator (false, dof_names, dof_names_dot);
    fm0.template registerEvaluator<EvalT> (ev);
  }
  else
  {
    ev = evalUtils.constructGatherSolutionEvaluator_noTransient (false, dof_names);
    fm0.template registerEvaluator<EvalT> (ev);
  }

  // Compute basis functions
  ev = evalUtils.constructComputeBasisFunctionsEvaluator(cellType, intrepidBasis, cubature);
  fm0.template registerEvaluator<EvalT> (ev);

  // Gather coordinates
  ev = evalUtils.constructGatherCoordinateVectorEvaluator();
  fm0.template registerEvaluator<EvalT> (ev);

  // Scatter residual
  ev = evalUtils.constructScatterResidualEvaluator(false, resid_names, offset, "Scatter Hydrology");
  fm0.template registerEvaluator<EvalT> (ev);

  // Get qp coordinates (for source terms)
  ev = evalUtils.constructMapToPhysicalFrameEvaluator(cellType, cubature);
  fm0.template registerEvaluator<EvalT> (ev);

  // Interpolate Effective Pressure
  ev = evalUtils.constructDOFInterpolationEvaluator("Effective Pressure");
  fm0.template registerEvaluator<EvalT> (ev);

  // Drainage Sheet Depth
  ev = evalUtils.constructDOFInterpolationEvaluator("Drainage Sheet Depth");
  fm0.template registerEvaluator<EvalT> (ev);

  if (has_evolution_equation)
  {
    // Drainage Sheet Depth Time Derivative
    ev = evalUtils.constructDOFInterpolationEvaluator("Drainage Sheet Depth Dot");
    fm0.template registerEvaluator<EvalT> (ev);
  }

  // Effective Pressure Gradient
  ev = evalUtils.constructDOFGradInterpolationEvaluator("Effective Pressure");
  fm0.template registerEvaluator<EvalT> (ev);

  // Hydrostatic Potential Gradient
  ev = evalUtils.constructDOFGradInterpolationEvaluator("Hydrostatic Potential");
  fm0.template registerEvaluator<EvalT> (ev);

  // Basal Velocity
  ev = evalUtils.constructDOFVecInterpolationEvaluator("Basal Velocity");
  fm0.template registerEvaluator<EvalT> (ev);

  // Surface Water Input
  ev = evalUtils.constructDOFInterpolationEvaluator("Surface Water Input");
  fm0.template registerEvaluator<EvalT> (ev);

  // Geothermal Flux
  ev = evalUtils.constructDOFInterpolationEvaluator("Geothermal Flux");
  fm0.template registerEvaluator<EvalT> (ev);

  // --------------------------------- FELIX evaluators -------------------------------- //

  // ----- Hydrology Hydrostatic Potential ---- //

  p = rcp(new Teuchos::ParameterList("Hydrology Hydrostatic Potential"));

  //Input
  p->set<std::string> ("Ice Thickness Variable Name","Ice Thickness");
  p->set<std::string> ("Surface Height Variable Name","Surface Height");
  p->set<Teuchos::ParameterList*> ("FELIX Physical Parameters",&params->sublist("FELIX Physical Parameters"));

  // Output
  p->set<std::string> ("Hydrostatic Potential Variable Name","Hydrostatic Potential");

  ev = rcp(new FELIX::HydrologyHydrostaticPotential<EvalT,PHAL::AlbanyTraits>(*p,dl));
  fm0.template registerEvaluator<EvalT>(ev);

  // ------- Hydrology Water Discharge -------- //
  p = rcp(new Teuchos::ParameterList("Hydrology Water Discharge"));

  //Input
  p->set<std::string> ("Drainage Sheet Depth QP Variable Name","Drainage Sheet Depth");
  p->set<std::string> ("Hydrostatic Potential Gradient QP Variable Name","Hydrostatic Potential Gradient");
  p->set<std::string> ("Effective Pressure Gradient QP Variable Name","Effective Pressure Gradient");

  p->set<Teuchos::ParameterList*> ("FELIX Hydrology",&params->sublist("FELIX Hydrology"));
  p->set<Teuchos::ParameterList*> ("FELIX Physical Parameters",&params->sublist("FELIX Physical Parameters"));

  //Output
  p->set<std::string> ("Discharge QP Variable Name","Water Discharge");

  ev = rcp(new FELIX::HydrologyWaterDischarge<EvalT,PHAL::AlbanyTraits>(*p,dl));
  fm0.template registerEvaluator<EvalT>(ev);

  // ------- Hydrology Melting Rate -------- //
  p = rcp(new Teuchos::ParameterList("Hydrology Melting Rate"));

  //Input
  p->set<std::string> ("Geothermal Heat Source QP Variable Name","Geothermal Flux");
  p->set<std::string> ("Sliding Velocity QP Variable Name","Sliding Velocity");
  p->set<std::string> ("Basal Friction Coefficient QP Variable Name","Beta");
  p->set<Teuchos::ParameterList*> ("FELIX Physical Parameters",&params->sublist("FELIX Physical Parameters"));

  //Output
  p->set<std::string> ("Melting Rate QP Variable Name","Melting Rate");

  ev = rcp(new FELIX::HydrologyMelting<EvalT,PHAL::AlbanyTraits>(*p,dl));
  fm0.template registerEvaluator<EvalT>(ev);

  // ------- Sliding Velocity -------- //
  p = Teuchos::rcp(new Teuchos::ParameterList("FELIX Velocity Norm"));

  // Input
  p->set<std::string>("Field Name","Basal Velocity");
  p->set<std::string>("Field Layout","Cell QuadPoint");

  // Output
  p->set<std::string>("Field Norm Name","Sliding Velocity");

  ev = Teuchos::rcp(new FELIX::FieldNorm<EvalT,PHAL::AlbanyTraits>(*p,dl));
  fm0.template registerEvaluator<EvalT>(ev);

  //--- FELIX basal friction coefficient ---//
  p = Teuchos::rcp(new Teuchos::ParameterList("FELIX Basal Friction Coefficient"));

  //Input
  p->set<std::string>("Sliding Velocity QP Variable Name", "Sliding Velocity");
  p->set<std::string>("BF Variable Name", "BF");
  p->set<std::string>("Effective Pressure QP Variable Name", "Effective Pressure");
  p->set<Teuchos::ParameterList*>("Parameter List", &params->sublist("FELIX Basal Friction Coefficient"));
  p->set<Teuchos::ParameterList*>("FELIX Physical Parameters", &params->sublist("FELIX Physical Parameters"));

  //Output
  p->set<std::string>("Basal Friction Coefficient Variable Name", "Beta");

  ev = Teuchos::rcp(new FELIX::BasalFrictionCoefficient<EvalT,PHAL::AlbanyTraits>(*p,dl));
  fm0.template registerEvaluator<EvalT>(ev);

  // ------- Hydrology Residual Elliptic Eqn-------- //
  p = rcp(new Teuchos::ParameterList("Hydrology Residual Elliptic Eqn"));

  //Input
  p->set<std::string> ("BF Name", "BF");
  p->set<std::string> ("Gradient BF Name", "Grad BF");
  p->set<std::string> ("Weighted Measure Name", "Weights");

  p->set<std::string> ("Discharge QP Variable Name", "Water Discharge");
  p->set<std::string> ("Effective Pressure QP Variable Name", "Effective Pressure");
  p->set<std::string> ("Drainage Sheet Depth QP Variable Name", "Drainage Sheet Depth");
  p->set<std::string> ("Melting Rate QP Variable Name","Melting Rate");
  p->set<std::string> ("Surface Water Input QP Variable Name","Surface Water Input");
  p->set<std::string> ("Sliding Velocity QP Variable Name","Sliding Velocity");

  p->set<Teuchos::ParameterList*> ("FELIX Physical Parameters",&params->sublist("FELIX Physical Parameters"));
  p->set<Teuchos::ParameterList*> ("FELIX Hydrology",&params->sublist("FELIX Hydrology"));

  //Output
  p->set<std::string> ("Hydrology Elliptic Eqn Residual Name",resid_names[0]);

  ev = rcp(new FELIX::HydrologyResidualEllipticEqn<EvalT,PHAL::AlbanyTraits>(*p,dl));
  fm0.template registerEvaluator<EvalT>(ev);

  if (has_evolution_equation)
  {
    // ------- Hydrology Evolution Residual -------- //
    p = rcp(new Teuchos::ParameterList("Hydrology Residual Evolution"));

    //Input
    p->set<std::string> ("Weighted BF Variable Name", "wBF");
    p->set<std::string> ("Drainage Sheet Depth QP Variable Name","h");
    p->set<std::string> ("Drainage Sheet Depth Dot QP Variable Name","h_dot");
    p->set<std::string> ("Effective Pressure QP Variable Name","N");
    p->set<std::string> ("Melting Rate QP Variable Name","m");
    p->set<std::string> ("Sliding Velocity QP Variable Name","u_b");

    p->set<Teuchos::ParameterList*> ("FELIX Hydrology",&params->sublist("FELIX Hydrology"));
    p->set<Teuchos::ParameterList*> ("FELIX Physics",&params->sublist("FELIX Physics"));

    //Output
    p->set<std::string> ("Residual Evolution Eqn Variable Name", resid_names[1]);

    ev = rcp(new FELIX::HydrologyResidualEvolutionEqn<EvalT,PHAL::AlbanyTraits>(*p,dl));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  // ----------------------------------------------------- //

  if (fieldManagerChoice == Albany::BUILD_RESID_FM)
  {
    PHX::Tag<typename EvalT::ScalarT> res_tag("Scatter Hydrology", dl->dummy);
    fm0.requireField<EvalT>(res_tag);
  }
  else if (fieldManagerChoice == Albany::BUILD_RESPONSE_FM)
  {
    RCP<Teuchos::ParameterList> paramList = rcp(new Teuchos::ParameterList("Param List"));

    RCP<const Albany::MeshSpecsStruct> meshSpecsPtr = Teuchos::rcpFromRef(meshSpecs);
    paramList->set<RCP<const Albany::MeshSpecsStruct> >("Mesh Specs Struct", meshSpecsPtr);
    paramList->set<RCP<ParamLib> >("Parameter Library", paramLib);

    Albany::ResponseUtilities<EvalT, PHAL::AlbanyTraits> respUtils(dl);

    return respUtils.constructResponses(fm0, *responseList, paramList, stateMgr);
  }

  return Teuchos::null;
}

} // Namespace FELIX

#endif // FELIX_HYDROLOGY_PROBLEM_HPP
