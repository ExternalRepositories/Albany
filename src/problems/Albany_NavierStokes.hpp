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


#ifndef ALBANY_NAVIERSTOKES_HPP
#define ALBANY_NAVIERSTOKES_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"

namespace Albany {

  /*!
   * \brief Abstract interface for representing a 1-D finite element
   * problem.
   */
  class NavierStokes : public AbstractProblem {
  public:
  
    //! Default constructor
    NavierStokes(
                         const Teuchos::RCP<Teuchos::ParameterList>& params,
                         const Teuchos::RCP<ParamLib>& paramLib,
                         const int numDim_);

    //! Destructor
    ~NavierStokes();

     //! Build the PDE instantiations, boundary conditions, and initial solution
    void buildProblem(
       Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
       StateManager& stateMgr,
       Teuchos::ArrayRCP< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses);

    //! Each problem must generate it's list of valide parameters
    Teuchos::RCP<const Teuchos::ParameterList> getValidProblemParameters() const;

  private:

    //! Private to prohibit copying
    NavierStokes(const NavierStokes&);
    
    //! Private to prohibit copying
    NavierStokes& operator=(const NavierStokes&);

    //! Main problem setup routine. Not directly called, but indirectly by following functions
    template <typename EvalT>
    void constructEvaluators(
            PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
            const Albany::MeshSpecsStruct& meshSpecs,
            Albany::StateManager& stateMgr,
            Albany::FieldManagerChoice fmchoice,
            Teuchos::ArrayRCP< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses);

    //! Interface for Residual (PDE) field manager
    template <typename EvalT>
    void constructResidEvaluators(
            PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
            const Albany::MeshSpecsStruct& meshSpecs,
            Albany::StateManager& stateMgr)
    {
      Teuchos::ArrayRCP< Teuchos::RCP<Albany::AbstractResponseFunction> > junk;
      constructEvaluators<EvalT>(fm0, meshSpecs, stateMgr, BUILD_RESID_FM, junk);
    }

    //! Interface for Response field manager, except for residual type
    template <typename EvalT>
    void constructResponseEvaluators(
            PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
            const Albany::MeshSpecsStruct& meshSpecs,
            Albany::StateManager& stateMgr)
    {
      Teuchos::ArrayRCP< Teuchos::RCP<Albany::AbstractResponseFunction> > junk;
      constructEvaluators<EvalT>(fm0, meshSpecs, stateMgr, BUILD_RESPONSE_FM, junk);
    }

    //! Interface for Response field manager, Residual type.
    // This version loads the responses variable, that needs to be constructed just once
    template <typename EvalT>
    void constructResponseEvaluators(
            PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
            const Albany::MeshSpecsStruct& meshSpecs,
            Albany::StateManager& stateMgr,
            Teuchos::ArrayRCP< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses)
    {
      constructEvaluators<EvalT>(fm0, meshSpecs, stateMgr, BUILD_RESPONSE_FM, responses);
    }

    void constructDirichletEvaluators(const Albany::MeshSpecsStruct& meshSpecs);

  protected:

    
    bool periodic;     //! periodic BCs
    int numDim;        //! number of spatial dimensions

    bool haveFlow;     //! have flow equations (momentum+continuity)
    bool haveHeat;     //! have heat equation (temperature)
    bool haveNeut;     //! have neutron flux equation
    bool haveSource;   //! have source term in heat equation
    bool haveNeutSource;   //! have source term in neutron flux equation
    bool havePSPG;     //! have pressure stabilization
    bool haveSUPG;     //! have SUPG stabilization
    bool porousMedia;  //! flow through porous media problem
    
  };

}

#include "Intrepid_FieldContainer.hpp"
#include "Intrepid_DefaultCubatureFactory.hpp"
#include "Shards_CellTopology.hpp"

#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_EvaluatorUtils.hpp"
#include "Albany_ResponseUtilities.hpp"

#include "PHAL_NSContravarientMetricTensor.hpp"
#include "PHAL_NSMaterialProperty.hpp"
#include "PHAL_Source.hpp"
#include "PHAL_NSBodyForce.hpp"
#include "PHAL_NSPermeabilityTerm.hpp"
#include "PHAL_NSForchheimerTerm.hpp"
#include "PHAL_NSRm.hpp"
#include "PHAL_NSTauM.hpp"
#include "PHAL_NSTauT.hpp"
#include "PHAL_NSMomentumResid.hpp"
#include "PHAL_NSContinuityResid.hpp"
#include "PHAL_NSThermalEqResid.hpp"
#include "PHAL_NSNeutronEqResid.hpp"

template <typename EvalT>
void Albany::NavierStokes::constructEvaluators(
        PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
        const Albany::MeshSpecsStruct& meshSpecs,
        Albany::StateManager& stateMgr,
        Albany::FieldManagerChoice fieldManagerChoice,
        Teuchos::ArrayRCP< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses)
{
  using Teuchos::RCP;
  using Teuchos::rcp;
  using Teuchos::ParameterList;
  using PHX::DataLayout;
  using PHX::MDALayout;
  using std::vector;
  using std::map;
  using PHAL::AlbanyTraits;
  
  RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
    intrepidBasis = Albany::getIntrepidBasis(meshSpecs.ctd);
  RCP<shards::CellTopology> cellType = rcp(new shards::CellTopology (&meshSpecs.ctd));
  
  const int numNodes = intrepidBasis->getCardinality();
  const int worksetSize = meshSpecs.worksetSize;
  
  Intrepid::DefaultCubatureFactory<RealType> cubFactory;
  RCP <Intrepid::Cubature<RealType> > cubature = cubFactory.create(*cellType, meshSpecs.cubatureDegree);
  
  const int numQPts = cubature->getNumPoints();
  const int numVertices = cellType->getNodeCount();
  
  *out << "Field Dimensions: Workset=" << worksetSize 
       << ", Vertices= " << numVertices
       << ", Nodes= " << numNodes
       << ", QuadPts= " << numQPts
       << ", Dim= " << numDim << endl;
  

   RCP<Albany::Layouts> dl = rcp(new Albany::Layouts(worksetSize,numVertices,numNodes,numQPts,numDim));
   Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils(dl);
   bool supportsTransient=true;
   int offset=0;

   // Temporary variable used numerous times below
   Teuchos::RCP<PHX::Evaluator<AlbanyTraits> > ev;

   // Define Field Names

   if (haveFlow) {

     Teuchos::ArrayRCP<string> dof_names(1);
     Teuchos::ArrayRCP<string> dof_names_dot(1);
     Teuchos::ArrayRCP<string> resid_names(1);
     dof_names[0] = "Velocity";
     dof_names_dot[0] = dof_names[0]+"_dot";
     resid_names[0] = "Momentum Residual";
     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructGatherSolutionEvaluator(true, dof_names, dof_names_dot, offset));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFVecInterpolationEvaluator(dof_names[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFVecInterpolationEvaluator(dof_names_dot[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFVecGradInterpolationEvaluator(dof_names[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructScatterResidualEvaluator(true, resid_names,offset, "Scatter Momentum"));
     offset += numDim;
   }

   if (haveFlow) {
     Teuchos::ArrayRCP<string> dof_names(1);
     Teuchos::ArrayRCP<string> dof_names_dot(1);
     Teuchos::ArrayRCP<string> resid_names(1);
     dof_names[0] = "Pressure";
     dof_names_dot[0] = dof_names[0]+"_dot";
     resid_names[0] = "Continuity Residual";
     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructGatherSolutionEvaluator(false, dof_names, dof_names_dot, offset));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFInterpolationEvaluator(dof_names[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFInterpolationEvaluator(dof_names_dot[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFGradInterpolationEvaluator(dof_names[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructScatterResidualEvaluator(false, resid_names,offset, "Scatter Continuity"));
     offset ++;
   }

   if (haveHeat) { // Gather Solution Temperature
     Teuchos::ArrayRCP<string> dof_names(1);
     Teuchos::ArrayRCP<string> dof_names_dot(1);
     Teuchos::ArrayRCP<string> resid_names(1);
     dof_names[0] = "Temperature";
     dof_names_dot[0] = dof_names[0]+"_dot";
     resid_names[0] = dof_names[0]+" Residual";
     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructGatherSolutionEvaluator(false, dof_names, dof_names_dot, offset));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFInterpolationEvaluator(dof_names[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFInterpolationEvaluator(dof_names_dot[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFGradInterpolationEvaluator(dof_names[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructScatterResidualEvaluator(false, resid_names, offset, "Scatter Temperature"));
     offset ++;

   }
   if (haveNeut) { // Gather Solution Neutron
     Teuchos::ArrayRCP<string> dof_names(1);
     Teuchos::ArrayRCP<string> dof_names_dot(1);
     Teuchos::ArrayRCP<string> resid_names(1);
     dof_names[0] = "Neutron";
     dof_names_dot[0] = dof_names[0]+"_dot";
     resid_names[0] = dof_names[0]+" Residual";
     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructGatherSolutionEvaluator(false, dof_names, dof_names_dot, offset));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFInterpolationEvaluator(dof_names[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFInterpolationEvaluator(dof_names_dot[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFGradInterpolationEvaluator(dof_names[0]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructScatterResidualEvaluator(false, resid_names, offset, "Scatter Neutron"));
     offset ++;
   }

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructGatherCoordinateVectorEvaluator());

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructMapToPhysicalFrameEvaluator(cellType, cubature));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructComputeBasisFunctionsEvaluator(cellType, intrepidBasis, cubature));

  if (havePSPG || haveSUPG) { // Compute Contravarient Metric Tensor
    RCP<ParameterList> p = 
      rcp(new ParameterList("Contravarient Metric Tensor"));

    // Inputs: X, Y at nodes, Cubature, and Basis
    p->set<string>("Coordinate Vector Name","Coord Vec");
    p->set< RCP<DataLayout> >("Coordinate Data Layout", dl->vertices_vector);
    p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", cubature);

    p->set<RCP<shards::CellTopology> >("Cell Type", cellType);

    // Outputs: BF, weightBF, Grad BF, weighted-Grad BF, all in physical space
    p->set<string>("Contravarient Metric Tensor Name", "Gc");
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    ev = rcp(new PHAL::NSContravarientMetricTensor<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  { // Density
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Density");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Density");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveFlow) { // Viscosity
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Viscosity");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Viscosity");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveHeat) { // Specific Heat
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Specific Heat");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Specific Heat");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveHeat) { // Thermal conductivity
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Thermal Conductivity");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Thermal Conductivity");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveNeut) { // Neutron diffusion
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Neutron Diffusion");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set<string>("Temperature QP Variable Name", "Temperature");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Neutron Diffusion");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveNeut) { // Reference temperature
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Reference Temperature");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Reference Temperature");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveNeut) { // Neutron absorption cross section
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Neutron Absorption");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set<string>("Temperature QP Variable Name", "Temperature");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Neutron Absorption");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveNeut) { // Neutron fission cross section
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Neutron Fission");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set<string>("Temperature QP Variable Name", "Temperature");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Neutron Fission");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveNeut && haveHeat) { // Proportionality constant
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Proportionality Constant");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Proportionality Constant");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveFlow && haveHeat) { // Volumetric Expansion Coefficient
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", 
		   "Volumetric Expansion Coefficient");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      params->sublist("Volumetric Expansion Coefficient");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (porousMedia) { // Porosity, \phi
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", 
		   "Porosity");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      params->sublist("Porosity");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (porousMedia) { // Permeability Tensor, K
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", 
		   "Permeability");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      params->sublist("Permeability");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (porousMedia) { // Forchheimer Tensor, F
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", 
		   "Forchheimer");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      params->sublist("Forchheimer");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  } 

  if (haveHeat && haveSource) { // Source
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Source Name", "Source");
    p->set<string>("Variable Name", "Temperature");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Source Functions");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::Source<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveNeut && haveNeutSource) { // Source
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Source Name", "Source");
    p->set<string>("Variable Name", "Neutron");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Source Functions");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::Source<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveFlow) { // Body Force
    RCP<ParameterList> p = rcp(new ParameterList("Body Force"));

    //Input
    p->set<bool>("Have Heat", haveHeat);
    p->set<string>("Temperature QP Variable Name", "Temperature");
    p->set<string>("Density QP Variable Name", "Density");
    p->set<string>("Volumetric Expansion Coefficient QP Variable Name", "Volumetric Expansion Coefficient");

    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector); 

    Teuchos::ParameterList& paramList = params->sublist("Body Force");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);
  
    //Output
    p->set<string>("Body Force Name", "Body Force");

    ev = rcp(new PHAL::NSBodyForce<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

   if (porousMedia) { // Permeability term in momentum equation
    std::cout <<"This is flow through porous media problem" << std::endl;
    RCP<ParameterList> p = rcp(new ParameterList("Permeability Term"));

    //Input
    p->set<string>("Velocity QP Variable Name", "Velocity");
    p->set<string>("Density QP Variable Name", "Density");
    p->set<string>("Viscosity QP Variable Name", "Viscosity");
    p->set<string>("Porosity QP Variable Name", "Porosity");
    p->set<string>("Permeability QP Variable Name", "Permeability");

    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  
    //Output
    p->set<string>("Permeability Term", "Permeability Term");

    ev = rcp(new PHAL::NSPermeabilityTerm<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  } 

  if (porousMedia) { // Forchheimer term in momentum equation
    RCP<ParameterList> p = rcp(new ParameterList("Forchheimer Term"));

    //Input
    p->set<string>("Velocity QP Variable Name", "Velocity");
    p->set<string>("Density QP Variable Name", "Density");
    p->set<string>("Viscosity QP Variable Name", "Viscosity");
    p->set<string>("Porosity QP Variable Name", "Porosity");
    p->set<string>("Permeability QP Variable Name", "Permeability");
    p->set<string>("Forchheimer QP Variable Name", "Forchheimer");

    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  
    //Output
    p->set<string>("Forchheimer Term", "Forchheimer Term");

    ev = rcp(new PHAL::NSForchheimerTerm<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  } 

  if (haveFlow) { // Rm
    RCP<ParameterList> p = rcp(new ParameterList("Rm"));

    //Input
    p->set<string>("Velocity QP Variable Name", "Velocity");
    p->set<string>("Velocity Dot QP Variable Name", "Velocity_dot");
    p->set<string>("Velocity Gradient QP Variable Name", "Velocity Gradient");
    p->set<string>("Pressure Gradient QP Variable Name", "Pressure Gradient");
    p->set<string>("Density QP Variable Name", "Density");
    p->set<string>("Body Force QP Variable Name", "Body Force");
    p->set<string>("Porosity QP Variable Name", "Porosity");
    p->set<string>("Permeability Term", "Permeability Term");
    p->set<string>("Forchheimer Term", "Forchheimer Term");
 
    p->set<bool>("Porous Media", porousMedia);

    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  
    //Output
    p->set<string>("Rm Name", "Rm");

    ev = rcp(new PHAL::NSRm<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveFlow && (haveSUPG || havePSPG)) { // Tau M
    RCP<ParameterList> p = rcp(new ParameterList("Tau M"));

    //Input
    p->set<string>("Velocity QP Variable Name", "Velocity");
    p->set<std::string>("Contravarient Metric Tensor Name", "Gc"); 
    p->set<string>("Density QP Variable Name", "Density");
    p->set<string>("Viscosity QP Variable Name", "Viscosity");

    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    //Output
    p->set<string>("Tau M Name", "Tau M");

    ev = rcp(new PHAL::NSTauM<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveHeat && haveFlow && haveSUPG) { // Tau T
    RCP<ParameterList> p = rcp(new ParameterList("Tau T"));

    //Input
    p->set<string>("Velocity QP Variable Name", "Velocity");
    p->set<std::string>("Contravarient Metric Tensor Name", "Gc"); 
    p->set<string>("Thermal Conductivity Name", "Thermal Conductivity");
    p->set<string>("Density QP Variable Name", "Density");
    p->set<string>("Specific Heat QP Variable Name", "Specific Heat");

    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    //Output
    p->set<string>("Tau T Name", "Tau T");

    ev = rcp(new PHAL::NSTauT<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveFlow) { // Momentum Resid
    RCP<ParameterList> p = rcp(new ParameterList("Momentum Resid"));

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set<string>("Velocity Gradient QP Variable Name", "Velocity Gradient");
    p->set<string>("Pressure QP Variable Name", "Pressure");
    p->set<string>("Pressure Gradient QP Variable Name", "Pressure Gradient");
    p->set<string>("Viscosity QP Variable Name", "Viscosity");
    p->set<string>("Rm Name", "Rm");

    p->set<bool>("Have SUPG", haveSUPG);
    p->set<string>("Velocity QP Variable Name", "Velocity");
    p->set<string>("Density QP Variable Name", "Density");
    p->set<string> ("Tau M Name", "Tau M");
 
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);    
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);
    
    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  
    //Output
    p->set<string>("Residual Name", "Momentum Residual");
    p->set< RCP<DataLayout> >("Node Vector Data Layout", dl->node_vector);

    ev = rcp(new PHAL::NSMomentumResid<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveFlow) { // Continuity Resid
    RCP<ParameterList> p = rcp(new ParameterList("Continuity Resid"));

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set<string>("Gradient QP Variable Name", "Velocity Gradient");
    p->set<string>("Density QP Variable Name", "Density");

    p->set<bool>("Have PSPG", havePSPG);
    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set<std::string> ("Tau M Name", "Tau M");
    p->set<std::string> ("Rm Name", "Rm");

    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);  
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);
    p->set< RCP<PHX::DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<PHX::DataLayout> >("QP Vector Data Layout", dl->qp_vector);
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    //Output
    p->set<string>("Residual Name", "Continuity Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", dl->node_scalar);

    ev = rcp(new PHAL::NSContinuityResid<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

   if (haveHeat) { // Temperature Resid
    RCP<ParameterList> p = rcp(new ParameterList("Temperature Resid"));

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set<string>("QP Variable Name", "Temperature");
    p->set<string>("QP Time Derivative Variable Name", "Temperature_dot");
    p->set<string>("Gradient QP Variable Name", "Temperature Gradient");
    p->set<string>("Density QP Variable Name", "Density");
    p->set<string>("Specific Heat QP Variable Name", "Specific Heat");
    p->set<string>("Thermal Conductivity Name", "Thermal Conductivity");
    p->set<string>("Proportionality Constant Name", "Proportionality Constant");
    p->set<string>("Neutron Fission Name", "Neutron Fission");
    
    p->set<bool>("Have Source", haveSource);
    p->set<string>("Source Name", "Source");

    p->set<bool>("Have Flow", haveFlow);
    p->set<string>("Velocity QP Variable Name", "Velocity");

    p->set<bool>("Have Neutron", haveNeut);
    p->set<string>("Neutron QP Variable Name", "Neutron");
    
    p->set<bool>("Have SUPG", haveSUPG);
    p->set<string> ("Tau T Name", "Tau T");
 
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    //Output
    p->set<string>("Residual Name", "Temperature Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", dl->node_scalar);

    ev = rcp(new PHAL::NSThermalEqResid<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

   if (haveNeut) { // Neutron Resid
    RCP<ParameterList> p = rcp(new ParameterList("Neutron Resid"));

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set<string>("QP Variable Name", "Neutron");
    p->set<string>("Gradient QP Variable Name", "Neutron Gradient");
    p->set<string>("Neutron Diffusion Name", "Neutron Diffusion");
    p->set<string>("Neutron Absorption Name", "Neutron Absorption");
    p->set<string>("Neutron Fission Name", "Neutron Fission");
    p->set<string>("Reference Temperature Name", "Reference Temperature");
    
    p->set<bool>("Have Neutron Source", haveNeutSource);
    p->set<string>("Source Name", "Neutron Source");

    p->set<bool>("Have Flow", haveFlow);
    p->set<string>("Velocity QP Variable Name", "Velocity");

    p->set<bool>("Have Heat", haveHeat);
    p->set<string> ("Temperature QP Variable Name", "Temperature");
 
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    //Output
    p->set<string>("Residual Name", "Neutron Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", dl->node_scalar);

    ev = rcp(new PHAL::NSNeutronEqResid<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (fieldManagerChoice == Albany::BUILD_RESID_FM)  {
    if (haveFlow) {
      PHX::Tag<typename EvalT::ScalarT> mom_tag("Scatter Momentum", dl->dummy);
      fm0.requireField<EvalT>(mom_tag);
      PHX::Tag<typename EvalT::ScalarT> con_tag("Scatter Continuity", dl->dummy);
      fm0.requireField<EvalT>(con_tag);
    }
    if (haveHeat) {
      PHX::Tag<typename EvalT::ScalarT> heat_tag("Scatter Temperature", dl->dummy);
      fm0.requireField<EvalT>(heat_tag);
    }
    if (haveNeut) {
      PHX::Tag<typename EvalT::ScalarT> neut_tag("Scatter Neutron", dl->dummy);
      fm0.requireField<EvalT>(neut_tag);
    }
  }
  else {
    Teuchos::ParameterList& responseList = params->sublist("Response Functions");
    Albany::ResponseUtilities<EvalT, PHAL::AlbanyTraits> respUtils(dl);
    respUtils.constructResponses(fm0, responses, responseList, stateMgr);
  }

}
#endif // ALBANY_NAVIERSTOKES_HPP
