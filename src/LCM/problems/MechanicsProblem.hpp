//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef MECHANICSPROBLEM_HPP
#define MECHANICSPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"
#include "PHAL_AlbanyTraits.hpp"

namespace Albany {

  //----------------------------------------------------------------------------
  ///
  /// \brief Definition for the Mechanics Problem
  ///
  class MechanicsProblem : public Albany::AbstractProblem {
  public:

    typedef Intrepid::FieldContainer<RealType> FC;

    ///
    /// Default constructor
    ///
    MechanicsProblem(const Teuchos::RCP<Teuchos::ParameterList>& params,
                     const Teuchos::RCP<ParamLib>& param_lib,
                     const int num_dims,
                     const Teuchos::RCP<const Epetra_Comm>& comm);
    ///
    /// Destructor
    ///
    virtual
    ~MechanicsProblem();

    ///
    /// Set problem information for computation of rigid body modes 
    /// (in src/Albany_SolverFactory.cpp)
    ///
    void 
    getRBMInfoForML(int& num_PDEs, int& num_elasticity_dim, 
                    int& num_scalar, int& null_space_dim);

    ///
    /// Set problem information for computation of rigid body modes 
    /// (in src/Albany_SolverFactory.cpp)
    ///
    Teuchos::RCP<std::map<std::string, std::string> >
    constructFieldNameMap(bool surface_flag);

    ///
    /// Return number of spatial dimensions
    ///
    virtual 
    int 
    spatialDimension() const { return num_dims_; }

    ///
    /// Build the PDE instantiations, boundary conditions, initial solution
    ///
    virtual 
    void 
    buildProblem(Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> > 
                 meshSpecs,
                 StateManager& stateMgr);

    ///
    /// Build evaluators
    ///
    virtual 
    Teuchos::Array< Teuchos::RCP<const PHX::FieldTag> >
    buildEvaluators(PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
                    const Albany::MeshSpecsStruct& meshSpecs,
                    Albany::StateManager& stateMgr,
                    Albany::FieldManagerChoice fmchoice,
                    const Teuchos::RCP<Teuchos::ParameterList>& responseList);

    ///
    /// Each problem must generate it's list of valid parameters
    ///
    Teuchos::RCP<const Teuchos::ParameterList> 
    getValidProblemParameters() const;

    ///
    /// Retrieve the state data
    ///
    void 
    getAllocatedStates(Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<FC> > > 
                       old_state,
                       Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<FC> > > 
                       new_state) const;

    //----------------------------------------------------------------------------
  private:
    
    ///
    /// Private to prohibit copying
    ///
    MechanicsProblem(const MechanicsProblem&);

    ///
    /// Private to prohibit copying
    ///
    MechanicsProblem& operator=(const MechanicsProblem&);

    //----------------------------------------------------------------------------
  public:

    ///
    /// Main problem setup routine. 
    /// Not directly called, but indirectly by following functions
    ///
    template <typename EvalT> 
    Teuchos::RCP<const PHX::FieldTag>
    constructEvaluators(PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
                        const Albany::MeshSpecsStruct& meshSpecs,
                        Albany::StateManager& stateMgr,
                        Albany::FieldManagerChoice fmchoice,
                        const Teuchos::RCP<Teuchos::ParameterList>& 
                        responseList);

    ///
    /// Setup for the dirichlet BCs
    ///
    void 
    constructDirichletEvaluators(const Albany::MeshSpecsStruct& meshSpecs);

    //----------------------------------------------------------------------------
  protected:

    ///
    /// Enumerated type describing how a variable appears
    /// 
    enum MECH_VAR_TYPE {
      MECH_VAR_TYPE_NONE,      //! Variable does not appear
      MECH_VAR_TYPE_CONSTANT,  //! Variable is a constant
      MECH_VAR_TYPE_DOF        //! Variable is a degree-of-freedom
    };

    ///
    /// Accessor for variable type
    /// 
    void getVariableType(Teuchos::ParameterList& param_list,
                         const std::string& default_type,
                         MECH_VAR_TYPE& variable_type,
                         bool& have_variable,
                         bool& have_equation);

    ///
    /// Conversion from enum to string
    /// 
    std::string variableTypeToString(const MECH_VAR_TYPE variable_type);

    ///
    /// Construct a string for consistent output with surface elements
    /// 
    //std::string stateString(std::string, bool);

    ///
    /// Boundary conditions on source term
    ///
    bool have_source_;

    ///
    /// num of dimensions
    ///
    int num_dims_;

    ///
    /// number of integration points
    ///
    int num_pts_;

    ///
    /// number of element nodes
    ///
    int num_nodes_;

    ///
    /// number of element vertices
    ///
    int num_vertices_;

    ///
    /// Type of mechanics variable (disp or acc)
    ///
    MECH_VAR_TYPE mech_type_;

    ///
    /// Type of heat variable
    ///
    MECH_VAR_TYPE heat_type_;

    ///
    /// Type of pressure variable
    ///
    MECH_VAR_TYPE pressure_type_;

    ///
    /// Type of concentration variable
    ///
    MECH_VAR_TYPE transport_type_;

    ///
    /// Type of concentration variable
    ///
    MECH_VAR_TYPE hydrostress_type_;

    ///
    /// Have mechanics
    ///
    bool have_mech_;

    ///
    /// Have heat
    ///
    bool have_heat_;

    ///
    /// Have pressure
    ///
    bool have_pressure_;

    ///
    /// Have transport
    ///
    bool have_transport_;

    ///
    /// Have transport
    ///
    bool have_hydrostress_;

    ///
    /// Have mechanics equation
    ///
    bool have_mech_eq_;

    ///
    /// Have heat equation
    ///
    bool have_heat_eq_;

    ///
    /// Have pressure equation
    ///
    bool have_pressure_eq_;

    ///
    /// Have transport equation
    ///
    bool have_transport_eq_;
    
    ///
    /// Have projected hydrostatic stress term
    /// in transport equation
    ///
    bool have_hydrostress_eq_;

    ///
    /// RCP to matDB object
    ///
    Teuchos::RCP<QCAD::MaterialDatabase> material_DB_;

    ///
    /// old state data
    ///
    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<FC> > > old_state_;

    ///
    /// new state data
    ///
    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<FC> > > new_state_;

  };
  //----------------------------------------------------------------------------
}


#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_ResponseUtilities.hpp"
#include "Albany_EvaluatorUtils.hpp"

#include "PHAL_NSMaterialProperty.hpp"
#include "PHAL_Source.hpp"
#include "PHAL_SaveStateField.hpp"
#include "PHAL_ThermalConductivity.hpp"

#include "FieldNameMap.hpp"

#include "ElasticModulus.hpp"
#include "PoissonsRatio.hpp"
#include "DefGrad.hpp"
//#include "Neohookean.hpp"
//#include "J2Stress.hpp"
#include "PisdWdF.hpp"
#include "HardeningModulus.hpp"
#include "YieldStrength.hpp"
// #include "SaturationModulus.hpp"
// #include "SaturationExponent.hpp"
#include "DislocationDensity.hpp"
#include "TLElasResid.hpp"
#include "MechanicsResidual.hpp"
#include "Time.hpp"
#include "J2Fiber.hpp"
#include "GursonFD.hpp"
#include "AnisotropicHyperelasticDamage.hpp"
#include "QptLocation.hpp"
//#include "MooneyRivlin.hpp"
#include "MooneyRivlinDamage.hpp"
#include "MooneyRivlin_Incompressible.hpp"
#include "RIHMR.hpp"
#include "RecoveryModulus.hpp"
#include "GursonHMR.hpp"
#include "SurfaceBasis.hpp"
#include "SurfaceVectorJump.hpp"
#include "SurfaceVectorGradient.hpp"
#include "SurfaceScalarJump.hpp"
#include "SurfaceScalarGradient.hpp"
#include "SurfaceVectorResidual.hpp"
#include "CurrentCoords.hpp"
#include "TvergaardHutchinson.hpp"
#include "SurfaceCohesiveResidual.hpp"
#include "ConstitutiveModelInterface.hpp"
#include "ConstitutiveModelParameters.hpp"

// Header files for poroplasticity problem
#include "GradientElementLength.hpp"
#include "BiotCoefficient.hpp"
#include "BiotModulus.hpp"
#include "KCPermeability.hpp"
#include "Porosity.hpp"
#include "TLPoroPlasticityResidMass.hpp"
#include "TLPoroStress.hpp"
#include "SurfaceTLPoroMassResidual.hpp"

// Hear files for thermohydromechanics problem
#include "ThermoPoroPlasticityResidMass.hpp"
#include "ThermoPoroPlasticityResidEnergy.hpp"
#include "PHAL_NSMaterialProperty.hpp"
#include "MixtureThermalExpansion.hpp"
#include "MixtureSpecificHeat.hpp"

// Header files for hydrogen transport
#include "ScalarL2ProjectionResidual.hpp"
#include "HDiffusionDeformationMatterResidual.hpp"
#include "PHAL_NSMaterialProperty.hpp"
#include "DiffusionCoefficient.hpp"
#include "EffectiveDiffusivity.hpp"
#include "EquilibriumConstant.hpp"
#include "TrappedSolvent.hpp"
#include "TrappedConcentration.hpp"
#include "TotalConcentration.hpp"
#include "StrainRateFactor.hpp"
#include "TauContribution.hpp"
#include "UnitGradient.hpp"
#include "LatticeDefGrad.hpp"

//------------------------------------------------------------------------------
template <typename EvalT>
Teuchos::RCP<const PHX::FieldTag>
Albany::MechanicsProblem::
constructEvaluators(PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
                    const Albany::MeshSpecsStruct& meshSpecs,
                    Albany::StateManager& stateMgr,
                    Albany::FieldManagerChoice fieldManagerChoice,
                    const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
  using Teuchos::RCP;
  using Teuchos::rcp;
  using Teuchos::ParameterList;
  using PHX::DataLayout;
  using PHX::MDALayout;
  using std::vector;
  using PHAL::AlbanyTraits;
  using shards::CellTopology;
  using shards::getCellTopologyData;

  // get the name of the current element block
  string ebName = meshSpecs.ebName;

  // get the name of the material model to be used (and make sure there is one)
  string materialModelName = 
    material_DB_->getElementBlockSublist(ebName,
                                       "Material Model").get<string>("Model Name");
  TEUCHOS_TEST_FOR_EXCEPTION(materialModelName.length()==0, std::logic_error,
                             "A material model must be defined for block: "
                             +ebName);

#ifdef ALBANY_VERBOSE
  *out << "In MechanicsProblem::constructEvaluators" << endl;
  *out << "element block name: " << ebName << endl;
  *out << "material model name: " << materialModelName << endl;
#endif

  // define cell topologies
  RCP<CellTopology> comp_cellType = 
    rcp(new CellTopology(getCellTopologyData<shards::Tetrahedron<11> >()));
  RCP<shards::CellTopology> cellType = 
    rcp(new CellTopology (&meshSpecs.ctd));

  // Check if we are setting the composite tet flag
  bool composite = false;
  if ( material_DB_->isElementBlockParam(ebName,"Use Composite Tet 10") ) 
    composite = material_DB_->getElementBlockParam<bool>(ebName,
                                                       "Use Composite Tet 10");

  // Surface element checking
  bool surface_element = false;
  bool cohesive_element = false;
  RealType thickness = 0.0;
  if ( material_DB_->isElementBlockParam(ebName,"Surface Element") ){
    surface_element = 
      material_DB_->getElementBlockParam<bool>(ebName,"Surface Element");
    if ( material_DB_->isElementBlockParam(ebName,"Cohesive Element") )
      cohesive_element = 
        material_DB_->getElementBlockParam<bool>(ebName,
                                               "Cohesive Element");
  }

  if (surface_element) {
    if ( material_DB_->
         isElementBlockParam(ebName,"Localization thickness parameter") ) {
      thickness =
        material_DB_->
        getElementBlockParam<RealType>(ebName,"Localization thickness parameter");
    } else {
      thickness = 0.1;
    }
  }

  string msg = 
    "Surface elements are not yet supported with the composite tet";
  // FIXME, really need to check for WEDGE_12 topologies
  TEUCHOS_TEST_FOR_EXCEPTION(composite && surface_element, 
                             std::logic_error,
                             msg);

  // get the intrepid basis for the given cell topology
  RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
    intrepidBasis = Albany::getIntrepidBasis(meshSpecs.ctd, composite);

  if (composite && 
      meshSpecs.ctd.dimension==3 && 
      meshSpecs.ctd.node_count==10) cellType = comp_cellType;

  Intrepid::DefaultCubatureFactory<RealType> cubFactory;
  RCP <Intrepid::Cubature<RealType> > cubature = 
    cubFactory.create(*cellType, meshSpecs.cubatureDegree);


  // FIXME, this could probably go into the ProblemUtils 
  // just like the call to getIntrepidBasis
  RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > 
    surfaceBasis;
  RCP<shards::CellTopology> surfaceTopology;
  RCP<Intrepid::Cubature<RealType> > surfaceCubature;
  if (surface_element)
  {
#ifdef ALBANY_VERBOSE
    *out << "In Surface Element Logic" << std::endl;
#endif

    string name = meshSpecs.ctd.name;
    if ( name == "Triangle_3" || name == "Quadrilateral_4" )
    {
      surfaceBasis = 
        rcp(new Intrepid::Basis_HGRAD_LINE_C1_FEM<RealType,Intrepid::FieldContainer<RealType> >() );
      surfaceTopology = 
        rcp(new shards::CellTopology( shards::getCellTopologyData<shards::Line<2> >()) );
      surfaceCubature = 
        cubFactory.create(*surfaceTopology, meshSpecs.cubatureDegree);
    }
    else if ( name == "Wedge_6" )
    {
      surfaceBasis = 
        rcp(new Intrepid::Basis_HGRAD_TRI_C1_FEM<RealType, Intrepid::FieldContainer<RealType> >() );
      surfaceTopology = 
        rcp(new shards::CellTopology( shards::getCellTopologyData<shards::Triangle<3> >()) );
      surfaceCubature = 
        cubFactory.create(*surfaceTopology, meshSpecs.cubatureDegree);
    }
    else if ( name == "Hexahedron_8" )
    {
      surfaceBasis = 
        rcp(new Intrepid::Basis_HGRAD_QUAD_C1_FEM<RealType, Intrepid::FieldContainer<RealType> >() );
      surfaceTopology = 
        rcp(new shards::CellTopology( shards::getCellTopologyData<shards::Quadrilateral<4> >()) );
      surfaceCubature = 
        cubFactory.create(*surfaceTopology, meshSpecs.cubatureDegree);
    }

#ifdef ALBANY_VERBOSE
    *out << "surfaceCubature->getNumPoints(): " 
         << surfaceCubature->getNumPoints() << std::endl;
    *out << "surfaceCubature->getDimension(): " 
         << surfaceCubature->getDimension() << std::endl;
#endif
  }

  // Note that these are the volume element quantities
  num_nodes_ = intrepidBasis->getCardinality();
  const int workset_size = meshSpecs.worksetSize;

#ifdef ALBANY_VERBOSE
  *out << "Setting num_pts_, surface element is "
       << surface_element << std::endl;
#endif
  num_dims_ = cubature->getDimension();
  if ( !surface_element ) {
    num_pts_ = cubature->getNumPoints();
  } else {
    num_pts_ = surfaceCubature->getNumPoints();
  }
  num_vertices_ = num_nodes_;

#ifdef ALBANY_VERBOSE
  *out << "Field Dimensions: Workset=" << workset_size 
       << ", Vertices= " << num_vertices_
       << ", Nodes= " << num_nodes_
       << ", QuadPts= " << num_pts_
       << ", Dim= " << num_dims_ << endl;
#endif

  // Construct standard FEM evaluators with standard field names                
  RCP<Albany::Layouts> dl = 
    rcp(new Albany::Layouts(workset_size,num_vertices_,num_nodes_,num_pts_,num_dims_));
  msg = "Data Layout Usage in Mechanics problems assume vecDim = num_dims_";
  TEUCHOS_TEST_FOR_EXCEPTION(dl->vectorAndGradientLayoutsAreEquivalent==false, 
                             std::logic_error,
                             msg);
  Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils(dl);
  int offset = 0;
  // Temporary variable used numerous times below
  RCP<PHX::Evaluator<AlbanyTraits> > ev;

  // Define Field Names

  if (have_mech_eq_) { 
    Teuchos::ArrayRCP<string> dof_names(1);
    Teuchos::ArrayRCP<string> resid_names(1);
    dof_names[0] = "Displacement";
    resid_names[0] = dof_names[0]+" Residual";

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructGatherSolutionEvaluator_noTransient(true, 
                                                              dof_names));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructGatherCoordinateVectorEvaluator());

    if ( !surface_element ) {
      fm0.template registerEvaluator<EvalT>
        (evalUtils.constructDOFVecInterpolationEvaluator(dof_names[0]));
      
      fm0.template registerEvaluator<EvalT>
        (evalUtils.constructDOFVecGradInterpolationEvaluator(dof_names[0]));

      fm0.template registerEvaluator<EvalT>
        (evalUtils.constructMapToPhysicalFrameEvaluator(cellType, 
                                                        cubature));
    
      fm0.template registerEvaluator<EvalT>
        (evalUtils.constructComputeBasisFunctionsEvaluator(cellType, 
                                                           intrepidBasis, 
                                                           cubature));
    }
  
    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructScatterResidualEvaluator(true, 
                                                   resid_names));
    offset += num_dims_;
  }
  else if (have_mech_) { // constant configuration
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Displacement");
    p->set< RCP<DataLayout> >("Data Layout", dl->qp_vector);
    p->set<string>("Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Displacement");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    
  }
  if (have_heat_eq_) { // Gather Solution Temperature
    Teuchos::ArrayRCP<string> dof_names(1);
    Teuchos::ArrayRCP<string> resid_names(1);
    dof_names[0] = "Temperature";
    resid_names[0] = dof_names[0]+" Residual";
    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructGatherSolutionEvaluator_noTransient(false, 
                                                              dof_names, 
                                                              offset));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructDOFInterpolationEvaluator(dof_names[0]));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructDOFGradInterpolationEvaluator(dof_names[0]));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructScatterResidualEvaluator(false, 
                                                   resid_names, 
                                                   offset, 
                                                   "Scatter Temperature"));
    offset++;
  }
  else if (have_heat_ || have_transport_eq_ || have_transport_) {
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Temperature");
    p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
    p->set<string>("Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Heat");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (have_pressure_eq_) {
    Teuchos::ArrayRCP<string> dof_names(1);
    Teuchos::ArrayRCP<string> resid_names(1);
    dof_names[0] = "Pore_Pressure";
    resid_names[0] = dof_names[0]+" Residual";

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructGatherSolutionEvaluator_noTransient(false, 
                                                              dof_names, 
                                                              offset));
    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructGatherCoordinateVectorEvaluator());

    if ( !surface_element ) {
      fm0.template registerEvaluator<EvalT>
        (evalUtils.constructDOFInterpolationEvaluator(dof_names[0]));

      fm0.template registerEvaluator<EvalT>
        (evalUtils.constructDOFGradInterpolationEvaluator(dof_names[0]));

      fm0.template registerEvaluator<EvalT>
        (evalUtils.constructMapToPhysicalFrameEvaluator(cellType, 
                                                        cubature));
    
      fm0.template registerEvaluator<EvalT>
        (evalUtils.constructComputeBasisFunctionsEvaluator(cellType, 
                                                           intrepidBasis, 
                                                           cubature));
    }
  
    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructScatterResidualEvaluator(false, 
                                                   resid_names,
                                                   offset,
                                                   "Scatter Pore_Pressure"));
    offset++;
  }
  else if (have_pressure_) { // constant Pressure
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Pressure");
    p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
    p->set<string>("Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Pressure");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }


  if (have_transport_eq_) { // Gather solution for transport problem
    // Lattice Concentration
    Teuchos::ArrayRCP<string> dof_names(1);
    Teuchos::ArrayRCP<string> resid_names(1);
    dof_names[0] = "Transport";
    resid_names[0] = dof_names[0]+" Residual";
    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructGatherSolutionEvaluator_noTransient(false,
                                                              dof_names,
                                                              offset));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructDOFInterpolationEvaluator(dof_names[0]));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructDOFGradInterpolationEvaluator(dof_names[0]));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructScatterResidualEvaluator(false,
                                                   resid_names,
                                                   offset,
                                                   "Scatter Transport"));
    offset++; // for lattice concentration
  }
  else if (have_transport_) { // Constant transport scalar value
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Transport");
    p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
    p->set<string>("Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Transport");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (have_hydrostress_eq_) { // Gather solution for transport problem
    Teuchos::ArrayRCP<string> dof_names(1);
    Teuchos::ArrayRCP<string> resid_names(1);
    dof_names[0] = "HydroStress";
    resid_names[0] = dof_names[0]+" Residual";
    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructGatherSolutionEvaluator_noTransient(false,
                                                              dof_names,
                                                              offset));
    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructDOFInterpolationEvaluator(dof_names[0]));
    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructDOFGradInterpolationEvaluator(dof_names[0]));

    fm0.template registerEvaluator<EvalT>
      (evalUtils.constructScatterResidualEvaluator(false,
                                                   resid_names,
                                                   offset,
                                                   "Scatter HydroStress"));
    offset++; // for hydrostatic stress
  }

  // generate the field name map to deal with outputing surface element info
  LCM::FieldNameMap field_name_map(surface_element);
  RCP<std::map<std::string, std::string> > fnm = field_name_map.getMap();
  string cauchy       = (*fnm)["Cauchy_Stress"];
  string Fp           = (*fnm)["Fp"];
  string eqps         = (*fnm)["eqps"];
  string totStress    = (*fnm)["Total_Stress"];
  string kcPerm       = (*fnm)["KCPermeability"];
  string biotModulus  = (*fnm)["Biot_Modulus"];
  string biotCoeff    = (*fnm)["Biot_Coefficient"];
  string porosity     = (*fnm)["Porosity"];
  string porePressure = (*fnm)["Pore_Pressure"];
  
  { // Time
    RCP<ParameterList> p = rcp(new ParameterList("Time"));
    p->set<string>("Time Name", "Time");
    p->set<string>("Delta Time Name", "Delta Time");
    p->set< RCP<DataLayout> >("Workset Scalar Data Layout", dl->workset_scalar);
    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    p->set<bool>("Disable Transient", true);
    ev = rcp(new LCM::Time<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("Time",
                                       dl->workset_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (have_mech_eq_) { // Current Coordinates
    RCP<ParameterList> p = rcp(new ParameterList("Current Coordinates"));
    p->set<string>("Reference Coordinates Name", "Coord Vec");
    p->set<string>("Displacement Name", "Displacement");
    p->set<string>("Current Coordinates Name", "Current Coordinates");
    ev = rcp(new LCM::CurrentCoords<EvalT,AlbanyTraits>(*p,dl));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  // if (!surface_element && (have_mech_eq_ || have_pressure_eq_)) { // Strain
  //   RCP<ParameterList> p = rcp(new ParameterList("Strain"));
  //   p->set<string>("Gradient QP Variable Name", "Displacement Gradient");
  //   p->set<string>("Strain Name", "Strain");
  //   ev = rcp(new LCM::Strain<EvalT,AlbanyTraits>(*p,dl));
  //   fm0.template registerEvaluator<EvalT>(ev);

  //   // For some reason the save field below does not work.
  //   p = stateMgr.registerStateVariable(stateString("Strain",surface_element),
  //                                      dl->qp_tensor,
  //                                      dl->dummy,
  //                                      ebName,
  //                                      "scalar",
  //                                      0.0);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);

  // }

  if (have_mech_eq_) { // Elastic Modulus
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("QP Variable Name", "Elastic Modulus");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      material_DB_->getElementBlockSublist(ebName,"Elastic Modulus");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new LCM::ElasticModulus<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (have_mech_eq_) { // Poissons Ratio
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("QP Variable Name", "Poissons Ratio");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      material_DB_->getElementBlockSublist(ebName,"Poissons Ratio");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new LCM::PoissonsRatio<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (have_source_) { // Source
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Source Name", "Source");
    p->set<string>("Variable Name", "Displacement");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Source Functions");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::Source<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  // if (materialModelName == "J2Fiber")
  // {
  //   { // Integration Point Location
  //     RCP<ParameterList> p = rcp(new ParameterList("Integration Point Location"));

  //     p->set<string>("Coordinate Vector Name", "Coord Vec");
  //     p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);

  //     p->set<string>("Gradient BF Name", "Grad BF");
  //     p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

  //     p->set<string>("BF Name", "BF");
  //     p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);

  //     p->set<string>("Integration Point Location Name", "Integration Point Location");
  //     p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

  //     ev = rcp(new LCM::QptLocation<EvalT,AlbanyTraits>(*p));
  //     fm0.template registerEvaluator<EvalT>(ev);

  //     p = stateMgr.registerStateVariable("Integration Point Location",
  //                                        dl->qp_vector, 
  //                                        dl->dummy, 
  //                                        ebName, 
  //                                        "scalar", 
  //                                        0.0);
  //     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //     fm0.template registerEvaluator<EvalT>(ev);
  //   }
  // }

  if (have_mech_eq_) {
    RCP<ParameterList> p = rcp(new ParameterList("Constitutive Model Interface"));
    string matName = material_DB_->getElementBlockParam<string>(ebName,"material");
    Teuchos::ParameterList& param_list = 
      material_DB_->getElementBlockSublist(ebName,matName);
    if (have_heat_ || have_heat_eq_) {
      param_list.set<bool>("Have Temperature", true);
    }
    param_list.set<RCP<std::map<std::string, std::string> > >("Name Map", fnm);
    p->set<Teuchos::ParameterList*>("Material Parameters", &param_list);

    RCP<LCM::ConstitutiveModelInterface<EvalT,AlbanyTraits> > cmiEv = 
      rcp(new LCM::ConstitutiveModelInterface<EvalT,AlbanyTraits>(*p,dl));
    fm0.template registerEvaluator<EvalT>(cmiEv);

    // register state variables
    for (int sv(0); sv < cmiEv->getNumStateVars(); ++sv) {
      cmiEv->fillStateVariableStruct(sv);
      p = stateMgr.registerStateVariable(cmiEv->getName(),
                                         cmiEv->getLayout(),
                                         dl->dummy, 
                                         ebName, 
                                         cmiEv->getInitType(),
                                         cmiEv->getInitValue(),
                                         cmiEv->getStateFlag(),
                                         cmiEv->getOutputFlag());
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
    }
  }

  // if (have_mech_eq_ && materialModelName == "Neohookean") { // Stress
  //   RCP<ParameterList> p = rcp(new ParameterList("Neohookean Stress"));

  //   //Input
  //   p->set<string>("DefGrad Name", "F");
  //   p->set<string>("Elastic Modulus Name", "Elastic Modulus");
  //   p->set<string>("Poissons Ratio Name", "Poissons Ratio");
  //   p->set<string>("DetDefGrad Name", "J");

  //   //Output
  //   p->set<string>("Stress Name", cauchy);

  //   ev = rcp(new LCM::Neohookean<EvalT,AlbanyTraits>(*p,dl));
  //   fm0.template registerEvaluator<EvalT>(ev);

  //   // optional output
  //   bool outputFlag(true);
  //   if ( material_DB_->isElementBlockParam(ebName,"Output " + cauchy) )
  //     outputFlag = 
  //       material_DB_->getElementBlockParam<bool>(ebName,"Output " + cauchy);

  //   p = stateMgr.registerStateVariable(cauchy,
  //                                      dl->qp_tensor, 
  //                                      dl->dummy, 
  //                                      ebName, 
  //                                      "scalar", 
  //                                      0.0, 
  //                                      outputFlag);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }
  
  // if (have_mech_eq_ && materialModelName == "MooneyRivlin") {
  //   RCP<ParameterList> p = rcp(new ParameterList("MooneyRivlin Stress"));

  //   //Input
  //   p->set<string>("DefGrad Name", "F");
  //   p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

  //   p->set<string>("DetDefGrad Name", "J");  // dl->qp_scalar also

  //   // defaults for parameters
  //   RealType c1(0.0),c2(0.0),c(0.0); 
  //   // overide defaults
  //   c1 = material_DB_->getElementBlockParam<RealType>(ebName,"c1");
  //   c2 = material_DB_->getElementBlockParam<RealType>(ebName,"c2");
  //   c = material_DB_->getElementBlockParam<RealType>(ebName,"c");
  //   // pass params into evaluator
  //   p->set<RealType>("c1 Name", c1);
  //   p->set<RealType>("c2 Name", c2);
  //   p->set<RealType>("c Name", c);

  //   //Output
  //   p->set<string>("Stress Name", cauchy); //dl->qp_tensor also

  //   ev = rcp(new LCM::MooneyRivlin<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);

  //   // optional output
  //   bool outputFlag(true);
  //   if ( material_DB_->isElementBlockParam(ebName,"Output " + cauchy) )
  //     outputFlag = 
  //       material_DB_->getElementBlockParam<bool>(ebName,"Output " + cauchy);

  //   p = stateMgr.registerStateVariable(cauchy,
  //                                      dl->qp_tensor, 
  //                                      dl->dummy, 
  //                                      ebName, 
  //                                      "scalar",
  //                                      0.0, 
  //                                      false, 
  //                                      outputFlag);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }
 
  if (have_mech_eq_ && materialModelName == "MooneyRivlinDamage") {
    RCP<ParameterList> p = rcp(new ParameterList("MooneyRivlinDamage Stress"));

    //Input
    p->set<string>("DefGrad Name", "F");
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("DetDefGrad Name", "J");  // dl->qp_scalar also
    p->set<string>("alpha Name", "alpha");

    // defaults for parameters
    RealType c1(0.0), c2(0.0), c(0.0), zeta_inf(0.0), iota(0.0);

    c1       = material_DB_->getElementBlockParam<RealType>(ebName,"c1");
    c2       = material_DB_->getElementBlockParam<RealType>(ebName,"c2");
    c        = material_DB_->getElementBlockParam<RealType>(ebName,"c");
    zeta_inf = material_DB_->getElementBlockParam<RealType>(ebName,"zeta_inf");
    iota     = material_DB_->getElementBlockParam<RealType>(ebName,"iota");

    p->set<RealType>("c1 Name", c1);
    p->set<RealType>("c2 Name", c2);
    p->set<RealType>("c Name", c);
    p->set<RealType>("zeta_inf Name", zeta_inf);
    p->set<RealType>("iota Name", iota);


    //Output
    p->set<string>("Stress Name", cauchy); //dl->qp_tensor also

    ev = rcp(new LCM::MooneyRivlinDamage<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    // optional output
    bool outputFlag(true);
    if ( material_DB_->isElementBlockParam(ebName,"Output " + cauchy) )
      outputFlag = 
        material_DB_->getElementBlockParam<bool>(ebName,"Output " + cauchy);

    p = stateMgr.registerStateVariable(cauchy,
                                       dl->qp_tensor, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar",
                                       0.0, 
                                       false, 
                                       outputFlag);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    outputFlag = true;
    if ( material_DB_->isElementBlockParam(ebName,"Output Alpha") )
      outputFlag = 
        material_DB_->getElementBlockParam<bool>(ebName,"Output Alpha");

    p = stateMgr.registerStateVariable("alpha",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       1.0, 
                                       false, 
                                       outputFlag);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if ( have_mech_eq_ && materialModelName == "MooneyRivlinIncompressible") {
    RCP<ParameterList> p = 
      rcp(new ParameterList("MooneyRivlinIncompressible Stress"));

    //Input
    p->set<string>("DefGrad Name", "F");
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("DetDefGrad Name", "J");  // dl->qp_scalar also

    // defaults for parameters
    RealType c1(0.0), c2(0.0), c(0.0), mu(0.0);

    c1 = material_DB_->getElementBlockParam<RealType>(ebName,"c1");
    c2 = material_DB_->getElementBlockParam<RealType>(ebName,"c2");
    mu = material_DB_->getElementBlockParam<RealType>(ebName,"mu");

    p->set<RealType>("c1 Name", c1);
    p->set<RealType>("c2 Name", c2);
    p->set<RealType>("mu Name", mu);

    //Output
    p->set<string>("Stress Name", cauchy); //dl->qp_tensor also

    ev = rcp(new LCM::MooneyRivlin_Incompressible<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    // optional output
    bool outputFlag(true);
    if ( material_DB_->isElementBlockParam(ebName,"Output " + cauchy) )
      outputFlag = 
        material_DB_->getElementBlockParam<bool>(ebName,"Output " + cauchy);

    p = stateMgr.registerStateVariable(cauchy,
                                       dl->qp_tensor, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar",
                                       0.0, 
                                       false, 
                                       outputFlag);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (have_mech_eq_ &&  ( materialModelName == "J2" || 
                       materialModelName == "J2Fiber" ||
                       materialModelName == "GursonFD"|| 
                       materialModelName == "RIHMR" || 
                       materialModelName == "GursonHMR") ) {
    {
      // Hardening Modulus
      RCP<ParameterList> p = rcp(new ParameterList);

      p->set<string>("QP Variable Name", "Hardening Modulus");
      p->set<string>("QP Coordinate Vector Name", "Coord Vec");
      p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
      p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
      p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

      p->set<RCP<ParamLib> >("Parameter Library", paramLib);
      Teuchos::ParameterList& paramList = 
        material_DB_->getElementBlockSublist(ebName,"Hardening Modulus");
      p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

      ev = rcp(new LCM::HardeningModulus<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
    }

    { // Yield Strength
      RCP<ParameterList> p = rcp(new ParameterList);

      p->set<string>("QP Variable Name", "Yield Strength");
      p->set<string>("QP Coordinate Vector Name", "Coord Vec");
      p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
      p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
      p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

      p->set<RCP<ParamLib> >("Parameter Library", paramLib);
      Teuchos::ParameterList& paramList = 
        material_DB_->getElementBlockSublist(ebName,"Yield Strength");
      p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

      ev = rcp(new LCM::YieldStrength<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
    }
  }

  // if (have_mech_eq_ && ( materialModelName == "J2" || 
  //                     materialModelName == "J2Fiber" || 
  //                     materialModelName == "GursonFD") ) {
  //   { // Saturation Modulus
  //     RCP<ParameterList> p = rcp(new ParameterList);

  //     p->set<string>("Saturation Modulus Name", "Saturation Modulus");
  //     p->set<string>("QP Coordinate Vector Name", "Coord Vec");
  //     p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
  //     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //     p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

  //     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //     Teuchos::ParameterList& paramList = 
  //       material_DB_->getElementBlockSublist(ebName,"Saturation Modulus");
  //     p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //     ev = rcp(new LCM::SaturationModulus<EvalT,AlbanyTraits>(*p));
  //     fm0.template registerEvaluator<EvalT>(ev);
  //   }

  //   { // Saturation Exponent
  //     RCP<ParameterList> p = rcp(new ParameterList);

  //     p->set<string>("Saturation Exponent Name", "Saturation Exponent");
  //     p->set<string>("QP Coordinate Vector Name", "Coord Vec");
  //     p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
  //     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //     p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

  //     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //     Teuchos::ParameterList& paramList = 
  //       material_DB_->getElementBlockSublist(ebName,"Saturation Exponent");
  //     p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //     ev = rcp(new LCM::SaturationExponent<EvalT,AlbanyTraits>(*p));
  //     fm0.template registerEvaluator<EvalT>(ev);
  //   }
  // }

  if (have_mech_eq_ && (materialModelName == "RIHMR" || 
                     materialModelName == "GursonHMR") ) {
    { // Recovery Modulus
      RCP<ParameterList> p = rcp(new ParameterList);

      p->set<string>("QP Variable Name", "Recovery Modulus");
      p->set<string>("QP Coordinate Vector Name", "Coord Vec");
      p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
      p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
      p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

      p->set<RCP<ParamLib> >("Parameter Library", paramLib);
      Teuchos::ParameterList& paramList = 
        material_DB_->getElementBlockSublist(ebName,"Recovery Modulus");
      p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

      ev = rcp(new LCM::RecoveryModulus<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
    }
  }

  // if ( have_mech_eq_ && materialModelName == "J2" ) {
  //   {// Stress
  //     RCP<ParameterList> p = rcp(new ParameterList("J2 Stress"));

  //     //Input
  //     p->set<string>("DefGrad Name", "F");
  //     p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

  //     p->set<string>("Elastic Modulus Name", "Elastic Modulus");
  //     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

  //     p->set<string>("Poissons Ratio Name", "Poissons Ratio");
  //     p->set<string>("Hardening Modulus Name", "Hardening Modulus");
  //     p->set<string>("Saturation Modulus Name", "Saturation Modulus");
  //     p->set<string>("Saturation Exponent Name", "Saturation Exponent");
  //     p->set<string>("Yield Strength Name", "Yield Strength");
  //     p->set<string>("DetDefGrad Name", "J");

  //     //Output
  //     p->set<string>("Stress Name", cauchy);
  //     p->set<string>("Fp Name", Fp);
  //     p->set<string>("Eqps Name", eqps);

  //     // Save state data

  //     ev = rcp(new LCM::J2Stress<EvalT,AlbanyTraits>(*p));
  //     fm0.template registerEvaluator<EvalT>(ev);
  //     p = stateMgr.registerStateVariable(cauchy,
  //                                        dl->qp_tensor, 
  //                                        dl->dummy, 
  //                                        ebName, "scalar", 
  //                                        0.0);
  //     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //     fm0.template registerEvaluator<EvalT>(ev);
  //     p = stateMgr.registerStateVariable(Fp,
  //                                        dl->qp_tensor, 
  //                                        dl->dummy, 
  //                                        ebName, 
  //                                        "identity", 
  //                                        1.0, 
  //                                        true);
  //     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //     fm0.template registerEvaluator<EvalT>(ev);
  //     p = stateMgr.registerStateVariable(eqps,
  //                                        dl->qp_scalar, 
  //                                        dl->dummy, 
  //                                        ebName, 
  //                                        "scalar", 
  //                                        0.0, 
  //                                        true);
  //     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //     fm0.template registerEvaluator<EvalT>(ev);
  //   }
  // }

  if(have_mech_eq_ && materialModelName == "J2Fiber") {
    { // J2Fiber Stress
      RCP<ParameterList> p = rcp(new ParameterList("Stress"));

      //Input
      p->set<string>("DefGrad Name", "F");
      p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

      p->set<string>("Integration Point Location Name",
                     "Coord Vec");
      p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

      p->set<string>("Elastic Modulus Name", "Elastic Modulus");
      p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

      p->set<string>("Poissons Ratio Name", "Poissons Ratio");
      p->set<string>("Hardening Modulus Name", "Hardening Modulus");
      // p->set<string>("Saturation Modulus Name", "Saturation Modulus");
      // p->set<string>("Saturation Exponent Name", "Saturation Exponent");
      p->set<string>("Yield Strength Name", "Yield Strength");
      p->set<string>("DetDefGrad Name", "J");

      // default parameters
      RealType xiinf_J2(0.0), tau_J2(1.0);
      RealType k_f1(0.0), q_f1(1.0), vol_f1(0.0), xiinf_f1(0.0), tau_f1(1.0);
      RealType k_f2(0.0), q_f2(1.0), vol_f2(0.0), xiinf_f2(0.0), tau_f2(1.0);

      // get params for the matrix
      xiinf_J2 = material_DB_->getElementBlockParam<RealType>(ebName,"xiing_J2");
      tau_J2   = material_DB_->getElementBlockParam<RealType>(ebName,"tau_J2");

      // get params for fiber 1
      k_f1     = material_DB_->getElementBlockParam<RealType>(ebName,"k_f1");
      q_f1     = material_DB_->getElementBlockParam<RealType>(ebName,"q_f1");
      vol_f1   = material_DB_->getElementBlockParam<RealType>(ebName,"vol_f1");
      xiinf_f1 = material_DB_->getElementBlockParam<RealType>(ebName,"xiinf_f1");
      tau_f1   = material_DB_->getElementBlockParam<RealType>(ebName,"tau_f1");

      // get params for fiber 2
      k_f2     = material_DB_->getElementBlockParam<RealType>(ebName,"k_f2");
      q_f2     = material_DB_->getElementBlockParam<RealType>(ebName,"q_f2");
      vol_f2   = material_DB_->getElementBlockParam<RealType>(ebName,"vol_f2");
      xiinf_f2 = material_DB_->getElementBlockParam<RealType>(ebName,"xiinf_f2");
      tau_f2   = material_DB_->getElementBlockParam<RealType>(ebName,"tau_f2");

      bool isLocalCoord(false);
      if ( material_DB_->isElementBlockParam(ebName,"isLocalCoord") )
        isLocalCoord = material_DB_->getElementBlockParam<bool>(ebName,
                                                              "isLocalCoord");

      p->set< Teuchos::Array<RealType> >
        ("direction_f1 Values",                                
         (material_DB_->getElementBlockSublist
          (ebName,"direction_f1")).get<Teuchos::Array<RealType> >
         ("direction_f1 Values"));
      p->set< Teuchos::Array<RealType> >
        ("direction_f2 Values",
         (material_DB_->getElementBlockSublist
          (ebName,"direction_f2")).get<Teuchos::Array<RealType> >
         ("direction_f2 Values"));
      p->set< Teuchos::Array<RealType> >
        ("Ring Center Values",
         (material_DB_->getElementBlockSublist
          (ebName,"Ring Center")).get<Teuchos::Array<RealType> >
         ("Ring Center Values"));


      //Output
      p->set<string>("Stress Name", cauchy);
      p->set<string>("Fp Name", "Fp");
      p->set<string>("Eqps Name", "eqps");
      p->set<string>("Energy_J2 Name", "energy_J2");
      p->set<string>("Energy_f1 Name", "energy_f1");
      p->set<string>("Energy_f2 Name", "energy_f2");
      p->set<string>("Damage_J2 Name", "damage_J2");
      p->set<string>("Damage_f1 Name", "damage_f1");
      p->set<string>("Damage_f2 Name", "damage_f2");

      //Declare what state data will need to be saved (name, layout, init_type)

      ev = rcp(new LCM::J2Fiber<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);

      // optional output
      bool outputFlag(true);
      if ( material_DB_->isElementBlockParam(ebName,"Output " + cauchy) )
        outputFlag = material_DB_->getElementBlockParam<bool>(ebName,
                                                            "Output " + cauchy);
      p = stateMgr.registerStateVariable(cauchy,
                                         dl->qp_tensor, 
                                         dl->dummy, 
                                         ebName, 
                                         "scalar", 
                                         0.0, 
                                         false, 
                                         outputFlag);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);

      outputFlag = true;
      if ( material_DB_->isElementBlockParam(ebName,"Output Fp") )
        outputFlag = material_DB_->getElementBlockParam<bool>(ebName,
                                                            "Output Fp");
      p = stateMgr.registerStateVariable("Fp",
                                         dl->qp_tensor, 
                                         dl->dummy, 
                                         ebName, 
                                         "identity", 
                                         1.0, 
                                         true, 
                                         outputFlag);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);

      p = stateMgr.registerStateVariable("eqps",
                                         dl->qp_scalar, 
                                         dl->dummy, 
                                         ebName, 
                                         "scalar", 
                                         0.0, 
                                         true);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
      p = stateMgr.registerStateVariable("energy_J2",
                                         dl->qp_scalar, 
                                         dl->dummy, 
                                         ebName, 
                                         "scalar", 
                                         0.0, 
                                         true);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
      p = stateMgr.registerStateVariable("energy_f1",
                                         dl->qp_scalar, 
                                         dl->dummy, 
                                         ebName, 
                                         "scalar", 
                                         0.0, 
                                         true);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
      p = stateMgr.registerStateVariable("energy_f2",
                                         dl->qp_scalar, 
                                         dl->dummy, 
                                         ebName, 
                                         "scalar", 
                                         0.0, 
                                         true);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
      p = stateMgr.registerStateVariable("damage_J2",
                                         dl->qp_scalar, 
                                         dl->dummy, 
                                         ebName, 
                                         "scalar", 
                                         0.0, 
                                         true);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
      p = stateMgr.registerStateVariable("damage_f1",
                                         dl->qp_scalar, 
                                         dl->dummy, 
                                         ebName, 
                                         "scalar", 
                                         0.0, 
                                         true);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
      p = stateMgr.registerStateVariable("damage_f2",
                                         dl->qp_scalar, 
                                         dl->dummy, 
                                         ebName, 
                                         "scalar", 
                                         0.0, 
                                         true);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
    }
  }

  if(have_mech_eq_ && materialModelName == "AHD") {
    // AHD Stress
    RCP<ParameterList> p = rcp(new ParameterList("Stress"));

    //Input
    p->set<string>("DefGrad Name", "F");
    p->set<string>("QP Coordinate Vector Name","Coord Vec");
    p->set<string>("Elastic Modulus Name", "Elastic Modulus");
    p->set<string>("Poissons Ratio Name", "Poissons Ratio");
    p->set<string>("Hardening Modulus Name", "Hardening Modulus");
    // p->set<string>("Saturation Modulus Name", "Saturation Modulus");
    // p->set<string>("Saturation Exponent Name", "Saturation Exponent");
    p->set<string>("Yield Strength Name", "Yield Strength");
    p->set<string>("DetDefGrad Name", "J");

    string matName = material_DB_->getElementBlockParam<string>(ebName,"material");
    Teuchos::ParameterList& paramList = 
      material_DB_->getElementBlockSublist(ebName,matName);
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    //Output
    p->set<string>("Stress Name", cauchy);
    p->set<string>("EnergyM Name", "energyM");
    p->set<string>("EnergyF1 Name", "energyF1");
    p->set<string>("EnergyF2 Name", "energyF2");
    p->set<string>("DamageM Name", "damageM");
    p->set<string>("DamageF1 Name", "damageF1");
    p->set<string>("DamageF2 Name", "damageF2");

    ev = rcp(new LCM::AnisotropicHyperelasticDamage<EvalT,AlbanyTraits>(*p,dl));
    fm0.template registerEvaluator<EvalT>(ev);

    // optional output
    bool outputFlag(true);
    if ( material_DB_->isElementBlockParam(ebName,"Output " + cauchy) )
      outputFlag = material_DB_->getElementBlockParam<bool>(ebName,
                                                          "Output " + cauchy);
    p = stateMgr.registerStateVariable(cauchy,
                                       dl->qp_tensor, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       false, 
                                       outputFlag);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    p = stateMgr.registerStateVariable("energyM",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("energyF1",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("energyF2",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("damageM",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("damageF1",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("damageF2",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if(have_mech_eq_ && materialModelName == "GursonFD") {
    RCP<ParameterList> p = rcp(new ParameterList("GursonFD Stress"));

    //Input
    p->set<string>("Delta Time Name", "Delta Time");
    p->set< RCP<DataLayout> >("Workset Scalar Data Layout", dl->workset_scalar);

    p->set<string>("DefGrad Name", "F");
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    p->set<string>("Elastic Modulus Name", "Elastic Modulus");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Poissons Ratio Name", "Poissons Ratio");
    p->set<string>("Hardening Modulus Name", "Hardening Modulus");
    // p->set<string>("Saturation Modulus Name", "Saturation Modulus");
    // p->set<string>("Saturation Exponent Name", "Saturation Exponent");
    p->set<string>("Yield Strength Name", "Yield Strength");
    p->set<string>("DetDefGrad Name", "J");

    string matName = material_DB_->getElementBlockParam<string>(ebName,"material");
    Teuchos::ParameterList& paramList =
      material_DB_->getElementBlockSublist(ebName,matName);
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    //Output
    p->set<string>("Stress Name", cauchy);
    p->set<string>("Fp Name", "Fp");
    p->set<string>("Eqps Name", "eqps");
    p->set<string>("Void Volume Name", "voidVolume");

    //Declare what state data will need to be saved (name, layout, init_type)

    ev = rcp(new LCM::GursonFD<EvalT,AlbanyTraits>(*p, dl));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable(cauchy,
                                       dl->qp_tensor, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0,
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("Fp",
                                       dl->qp_tensor, 
                                       dl->dummy, 
                                       ebName, 
                                       "identity", 
                                       1.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("eqps",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("voidVolume",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       material_DB_->getElementBlockParam<RealType>(ebName,"f0"),
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

  }

  if(have_mech_eq_ && materialModelName == "GursonHMR") {
    //Gurson damage model with Hardening Minus Recovery
    RCP<ParameterList> p = rcp(new ParameterList("DursonHMR Stress"));

    //Input
    p->set<string>("DefGrad Name", "F");
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    p->set<string>("Elastic Modulus Name", "Elastic Modulus");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Poissons Ratio Name", "Poissons Ratio");
    p->set<string>("Hardening Modulus Name", "Hardening Modulus");
    p->set<string>("Yield Strength Name", "Yield Strength");
    p->set<string>("DetDefGrad Name", "J");
    p->set<string>("Recovery Modulus Name", "Recovery Modulus");

    // default parameters
    RealType f0(0.0), kw(0.0), eN(0.0), sN(0.0), fN(0.0);
    RealType fc(1.0), ff(1.0), q1(1.0), q2(1.0), q3(1.0);

    if ( material_DB_->isElementBlockParam(ebName,"f0") )
      f0 = material_DB_->getElementBlockParam<RealType>(ebName,"f0");
    if ( material_DB_->isElementBlockParam(ebName,"kw") )
      kw = material_DB_->getElementBlockParam<RealType>(ebName,"kw");
    if ( material_DB_->isElementBlockParam(ebName,"eN") )
      eN = material_DB_->getElementBlockParam<RealType>(ebName,"eN");
    if ( material_DB_->isElementBlockParam(ebName,"sN") )
      sN = material_DB_->getElementBlockParam<RealType>(ebName,"sN");
    if ( material_DB_->isElementBlockParam(ebName,"fN") )
      fN = material_DB_->getElementBlockParam<RealType>(ebName,"fN");
    if ( material_DB_->isElementBlockParam(ebName,"fc") )
      fc = material_DB_->getElementBlockParam<RealType>(ebName,"fc");
    if ( material_DB_->isElementBlockParam(ebName,"ff") )
      ff = material_DB_->getElementBlockParam<RealType>(ebName,"ff");
    if ( material_DB_->isElementBlockParam(ebName,"q1") )
      q1 = material_DB_->getElementBlockParam<RealType>(ebName,"q1");
    if ( material_DB_->isElementBlockParam(ebName,"q2") )
      q2 = material_DB_->getElementBlockParam<RealType>(ebName,"q2");
    if ( material_DB_->isElementBlockParam(ebName,"q3") )
      q3 = material_DB_->getElementBlockParam<RealType>(ebName,"q3");

    p->set<RealType>("f0 Name", f0);
    p->set<RealType>("kw Name", kw);
    p->set<RealType>("eN Name", eN);
    p->set<RealType>("sN Name", sN);
    p->set<RealType>("fN Name", fN);
    p->set<RealType>("fc Name", fc);
    p->set<RealType>("ff Name", ff);
    p->set<RealType>("q1 Name", q1);
    p->set<RealType>("q2 Name", q2);
    p->set<RealType>("q3 Name", q3);

    //Output
    p->set<string>("Stress Name", cauchy); //dl->qp_tensor also
    p->set<string>("Fp Name", "Fp");  // dl->qp_tensor also
    p->set<string>("Ess Name", "ess");  // dl->qp_scalar also
    p->set<string>("Eqps Name", "eqps");  // dl->qp_scalar also
    p->set<string>("Void Volume Name", "voidVolume"); // dl ->qp_scalar
    p->set<string>("IsoHardening Name", "isoHardening"); // dl ->qp_scalar

    //Declare what state data will need to be saved (name, layout, init_type)

    ev = rcp(new LCM::GursonHMR<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable(cauchy,
                                       dl->qp_tensor, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0,
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("Fp",
                                       dl->qp_tensor, 
                                       dl->dummy, 
                                       ebName, 
                                       "identity", 
                                       1.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("ess",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("eqps",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("voidVolume",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       f0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("isoHardening",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if(have_mech_eq_ && materialModelName == "RIHMR") {
    // Rate-Independent Hardening Minus Recovery Evaluator
    RCP<ParameterList> p = rcp(new ParameterList("RIHMR Stress"));

    //input
    p->set<string>("DefGrad Name", "F");
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    p->set<string>("Elastic Modulus Name", "Elastic Modulus");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Poissons Ratio Name", "Poissons Ratio");
    p->set<string>("Hardening Modulus Name", "Hardening Modulus");
    p->set<string>("Yield Strength Name", "Yield Strength");
    p->set<string>("Recovery Modulus Name", "Recovery Modulus");
    p->set<string>("DetDefGrad Name", "J");

    //output
    p->set<string>("Stress Name", cauchy); //dl->qp_tensor also
    p->set<string>("logFp Name", "logFp");  // dl->qp_tensor also
    p->set<string>("Eqps Name", "eqps");  // dl->qp_scalar also
    p->set<string>("IsoHardening Name", "isoHardening"); // dl ->qp_scalar

    //Declare what state data will need to be saved (name, layout, init_type)
    ev = rcp(new LCM::RIHMR<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    p = stateMgr.registerStateVariable(cauchy,
                                       dl->qp_tensor, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("logFp",
                                       dl->qp_tensor, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("eqps",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("isoHardening",
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName, 
                                       "scalar", 
                                       0.0, 
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }
  
  // Surface Element Block
  if ( surface_element )
  {

    {// Surface Basis
     // SurfaceBasis_Def.hpp
      RCP<ParameterList> p = rcp(new ParameterList("Surface Basis"));

      // inputs
      p->set<string>("Reference Coordinates Name", "Coord Vec");
      p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", surfaceCubature);
      p->set< RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >("Intrepid Basis", surfaceBasis);
      p->set<string>("Current Coordinates Name", "Current Coordinates");

      // outputs
      p->set<string>("Reference Basis Name", "Reference Basis");
      p->set<string>("Reference Area Name", "Reference Area");
      p->set<string>("Reference Dual Basis Name", "Reference Dual Basis");
      p->set<string>("Reference Normal Name", "Reference Normal");
      p->set<string>("Current Basis Name", "Current Basis");

      ev = rcp(new LCM::SurfaceBasis<EvalT,AlbanyTraits>(*p,dl));
      fm0.template registerEvaluator<EvalT>(ev);
    }

    if (have_mech_eq_) { // Surface Jump
      //SurfaceVectorJump_Def.hpp
      RCP<ParameterList> p = rcp(new ParameterList("Surface Vector Jump"));

      // inputs
      p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", surfaceCubature);
      p->set< RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >("Intrepid Basis", surfaceBasis);
      p->set<string>("Vector Name", "Current Coordinates");

      // outputs
      p->set<string>("Vector Jump Name", "Vector Jump");

      ev = rcp(new LCM::SurfaceVectorJump<EvalT,AlbanyTraits>(*p,dl));
      fm0.template registerEvaluator<EvalT>(ev);
    }

    if (have_pressure_eq_) { // Surface Jump
      //SurfaceScalarJump_Def.hpp
      RCP<ParameterList> p = rcp(new ParameterList("Surface Scalar Jump"));

      // inputs
      p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", surfaceCubature);
      p->set< RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >("Intrepid Basis", surfaceBasis);
      p->set<string>("Scalar Name", "Pore_Pressure");

      // outputs
      p->set<string>("Scalar Jump Name", "Pore Pressure Jump");
      p->set<string>("Scalar Average Name", porePressure);

      ev = rcp(new LCM::SurfaceScalarJump<EvalT,AlbanyTraits>(*p,dl));
      fm0.template registerEvaluator<EvalT>(ev);

      p = stateMgr.registerStateVariable(porePressure,
                                         dl->qp_scalar, 
                                         dl->dummy, 
                                         ebName, 
                                         "scalar", 
                                         0.0,
                                         true);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
    }

    if (have_mech_eq_) { // Surface Gradient
      //SurfaceVectorGradient_Def.hpp
      RCP<ParameterList> p = rcp(new ParameterList("Surface Vector Gradient"));

      // inputs
      p->set<RealType>("thickness",thickness);
      bool WeightedVolumeAverageJ(false);
      if ( material_DB_->isElementBlockParam(ebName,"Weighted Volume Average J") )
        p->set<bool>("Weighted Volume Average J Name", 
                     material_DB_->getElementBlockParam<bool>(ebName,"Weighted Volume Average J") );
      if ( material_DB_->isElementBlockParam(ebName,"Average J Stabilization Parameter") )
        p->set<RealType>("Averaged J Stabilization Parameter Name",
                         material_DB_->getElementBlockParam<RealType>(ebName,"Average J Stabilization Parameter") );
      p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", surfaceCubature);
      p->set<string>("Weights Name","Reference Area");
      p->set<string>("Current Basis Name", "Current Basis");
      p->set<string>("Reference Dual Basis Name", "Reference Dual Basis");
      p->set<string>("Reference Normal Name", "Reference Normal");
      p->set<string>("Vector Jump Name", "Vector Jump");

      // outputs
      p->set<string>("Surface Vector Gradient Name", "F");
      p->set<string>("Surface Vector Gradient Determinant Name", "J");

      ev = rcp(new LCM::SurfaceVectorGradient<EvalT,AlbanyTraits>(*p,dl));
      fm0.template registerEvaluator<EvalT>(ev);
    }

    if (have_pressure_eq_) { // Surface Gradient
      //SurfaceScalarGradient_Def.hpp
      RCP<ParameterList> p = rcp(new ParameterList("Surface Scalar Gradient"));

      // inputs
      p->set<RealType>("thickness",thickness);
      p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", surfaceCubature);
      p->set< RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >("Intrepid Basis", surfaceBasis);
      p->set<string>("Reference Dual Basis Name", "Reference Dual Basis");
      p->set<string>("Reference Normal Name", "Reference Normal");      
      p->set<string>("Nodal Scalar Name", "Pore_Pressure");
      p->set<string>("Scalar Jump Name", "Pore Pressure Jump");

      // outputs
      p->set<string>("Surface Scalar Gradient Name", "Pore Pressure Gradient");

      ev = rcp(new LCM::SurfaceScalarGradient<EvalT,AlbanyTraits>(*p,dl));
      fm0.template registerEvaluator<EvalT>(ev);
    }

    if(cohesive_element)
    {

      if (have_mech_eq_) { // Surface Traction based on cohesive element
        //TvergaardHutchinson_Def.hpp
        RCP<ParameterList> p = rcp(new ParameterList("Surface Cohesive Traction"));

        // inputs
        p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", surfaceCubature);
        p->set< RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >("Intrepid Basis", surfaceBasis);
        p->set<string>("Vector Jump Name", "Vector Jump");
        p->set<string>("Current Basis Name", "Current Basis");

        if ( material_DB_->isElementBlockParam(ebName,"delta_1") )
          p->set<RealType>("delta_1 Name", material_DB_->getElementBlockParam<RealType>(ebName,"delta_1"));
        else
          p->set<RealType>("delta_1 Name", 0.5);

        if ( material_DB_->isElementBlockParam(ebName,"delta_2") )
          p->set<RealType>("delta_2 Name", material_DB_->getElementBlockParam<RealType>(ebName,"delta_2"));
        else
          p->set<RealType>("delta_2 Name", 0.5);

        if ( material_DB_->isElementBlockParam(ebName,"delta_c") )
          p->set<RealType>("delta_c Name", material_DB_->getElementBlockParam<RealType>(ebName,"delta_c"));
        else
          p->set<RealType>("delta_c Name", 1.0);

        if ( material_DB_->isElementBlockParam(ebName,"sigma_c") )
          p->set<RealType>("sigma_c Name", material_DB_->getElementBlockParam<RealType>(ebName,"sigma_c"));
        else
          p->set<RealType>("sigma_c Name", 1.0);

        if ( material_DB_->isElementBlockParam(ebName,"beta_0") )
          p->set<RealType>("beta_0 Name", material_DB_->getElementBlockParam<RealType>(ebName,"beta_0"));
        else
          p->set<RealType>("beta_0 Name", 0.0);

        if ( material_DB_->isElementBlockParam(ebName,"beta_1") )
          p->set<RealType>("beta_1 Name", material_DB_->getElementBlockParam<RealType>(ebName,"beta_1"));
        else
          p->set<RealType>("beta_1 Name", 0.0);

        if ( material_DB_->isElementBlockParam(ebName,"beta_2") )
          p->set<RealType>("beta_2 Name", material_DB_->getElementBlockParam<RealType>(ebName,"beta_2"));
        else
          p->set<RealType>("beta_2 Name", 1.0);

        // outputs
        p->set<string>("Cohesive Traction Name","Cohesive Traction");
        ev = rcp(new LCM::TvergaardHutchinson<EvalT,AlbanyTraits>(*p,dl));
        fm0.template registerEvaluator<EvalT>(ev);
      }

      { // Surface Cohesive Residual
        // SurfaceCohesiveResidual_Def.hpp
        RCP<ParameterList> p = rcp(new ParameterList("Surface Cohesive Residual"));

        // inputs
        p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", surfaceCubature);
        p->set< RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >("Intrepid Basis", surfaceBasis);
        p->set<string>("Cohesive Traction Name", "Cohesive Traction");
        p->set<string>("Reference Area Name", "Reference Area");

        // outputs
        p->set<string>("Surface Cohesive Residual Name", "Displacement Residual");

        ev = rcp(new LCM::SurfaceCohesiveResidual<EvalT,AlbanyTraits>(*p,dl));
        fm0.template registerEvaluator<EvalT>(ev);
      }

    }
    else
    {

      if (have_mech_eq_) { // Surface Residual
        // SurfaceVectorResidual_Def.hpp
        RCP<ParameterList> p = rcp(new ParameterList("Surface Vector Residual"));

        // inputs
        p->set<RealType>("thickness",thickness);
        p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", surfaceCubature);
        p->set< RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >("Intrepid Basis", surfaceBasis);
        p->set<string>("DefGrad Name", "F");
        p->set<string>("Stress Name", cauchy);
        p->set<string>("Current Basis Name", "Current Basis");
        p->set<string>("Reference Dual Basis Name", "Reference Dual Basis");
        p->set<string>("Reference Normal Name", "Reference Normal");
        p->set<string>("Reference Area Name", "Reference Area");

        // Effective stress theory for poromechanics problem
        if (have_pressure_eq_) {
          p->set<string>("Pore Pressure Name", porePressure);
          p->set<string>("Biot Coefficient Name", biotCoeff);
        }

        // outputs
        p->set<string>("Surface Vector Residual Name", "Displacement Residual");

        ev = rcp(new LCM::SurfaceVectorResidual<EvalT,AlbanyTraits>(*p,dl));
        fm0.template registerEvaluator<EvalT>(ev);
      }
    } // end of coehesive/surface element block
  } else {

    if (have_mech_eq_) { // Deformation Gradient
      RCP<ParameterList> p = rcp(new ParameterList("Deformation Gradient"));

      // set flags to optionally volume average J with a weighted average
      bool WeightedVolumeAverageJ(false);
      if ( material_DB_->isElementBlockParam(ebName,"Weighted Volume Average J") )
        p->set<bool>("Weighted Volume Average J Name",
                     material_DB_->getElementBlockParam<bool>(ebName,"Weighted Volume Average J") );
      if ( material_DB_->isElementBlockParam(ebName,"Average J Stabilization Parameter") )
        p->set<RealType>("Averaged J Stabilization Parameter Name",
                         material_DB_->getElementBlockParam<RealType>(ebName,"Average J Stabilization Parameter") );

      // send in integration weights and the displacement gradient
      p->set<string>("Weights Name","Weights");
      p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
      p->set<string>("Gradient QP Variable Name", "Displacement Gradient");
      p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

      //Outputs: F, J
      p->set<string>("DefGrad Name", "F"); //dl->qp_tensor also
      p->set<string>("DetDefGrad Name", "J"); 
      p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

      ev = rcp(new LCM::DefGrad<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);


      // optional output
      bool outputFlag(false);
      if ( material_DB_->isElementBlockParam(ebName,"Output Deformation Gradient") )
        outputFlag = 
          material_DB_->getElementBlockParam<bool>(ebName,"Output Deformation Gradient");

      p = stateMgr.registerStateVariable("F",
                                         dl->qp_tensor, 
                                         dl->dummy, 
                                         ebName, 
                                         "identity", 
                                         1.0, 
                                         outputFlag);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);

      // need J and J_old to perform time integration for poromechanics problem
      outputFlag = false;
      if ( material_DB_->isElementBlockParam(ebName,"Output J") )
        outputFlag = 
          material_DB_->getElementBlockParam<bool>(ebName,"Output J");
      if (have_pressure_eq_ || outputFlag) {
        p = stateMgr.registerStateVariable("J",
                                           dl->qp_scalar,
                                           dl->dummy,
                                           ebName,
                                           "scalar",
                                           1.0,
                                           true);
        ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
      }
    }


    if (have_mech_eq_)
    { // Residual
      RCP<ParameterList> p = rcp(new ParameterList("Displacement Residual"));
      //Input
      p->set<string>("Stress Name", cauchy);
      p->set<string>("DefGrad Name", "F");
      p->set<string>("DetDefGrad Name", "J");
      p->set<string>("Weighted Gradient BF Name", "wGrad BF");
      p->set<string>("Weighted BF Name", "wBF");

      // Effective stress theory for poromechanics problem
      if (have_pressure_eq_) {
        p->set<string>("Pore Pressure Name", porePressure);
        p->set<string>("Biot Coefficient Name", biotCoeff);
      }

      p->set<RCP<ParamLib> >("Parameter Library", paramLib);
      //Output
      p->set<string>("Residual Name", "Displacement Residual");
      ev = rcp(new LCM::MechanicsResidual<EvalT,AlbanyTraits>(*p,dl));
      fm0.template registerEvaluator<EvalT>(ev);
    }
  }

  if (have_pressure_eq_ || have_transport_eq_) { // Constant Stabilization Parameter
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Material Property Name", "Stabilization Parameter");
    p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
    p->set<string>("Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);
    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      material_DB_->getElementBlockSublist(ebName,"Stabilization Parameter");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  // Element length in the direction of solution gradient
  if ( (have_pressure_eq_ || have_transport_eq_) && !surface_element) {
    RCP<ParameterList> p = rcp(new ParameterList("Gradient Element Length"));

    //Input
    if (have_pressure_eq_){
      p->set<string>("Unit Gradient QP Variable Name", "Pore Pressure Gradient");
    } else if (have_transport_eq_){
      p->set<string>("Unit Gradient QP Variable Name", "Transport Gradient");
    }

    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<string>("Gradient BF Name", "Grad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    //Output
    p->set<string>("Element Length Name", "Gradient Element Length");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    ev = rcp(new LCM::GradientElementLength<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("Gradient Element Length",
                                       dl->qp_scalar,
                                       dl->dummy,
                                       ebName,
                                       "scalar",
                                       1.0);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (have_pressure_eq_) {  // Porosity
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Porosity Name", porosity);
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    // Setting this turns on dependence of strain and pore pressure)
    //p->set<string>("Strain Name", "Strain");
    if (have_mech_eq_) p->set<string>("DetDefGrad Name", "J");
    // porosity update based on Coussy's poromechanics (see p.79)
    p->set<string>("QP Pore Pressure Name", porePressure);
    p->set<string>("Biot Coefficient Name", biotCoeff);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      material_DB_->getElementBlockSublist(ebName,"Porosity");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    // Output Porosity
    ev = rcp(new LCM::Porosity<EvalT,AlbanyTraits>(*p,dl));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable(porosity,
                                       dl->qp_scalar, 
                                       dl->dummy, 
                                       ebName,
                                       "scalar",
                                       0.5);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);


  }

  if (have_pressure_eq_) { // Biot Coefficient
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Biot Coefficient Name", biotCoeff);
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      material_DB_->getElementBlockSublist(ebName,"Biot Coefficient");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new LCM::BiotCoefficient<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    p = stateMgr.registerStateVariable(biotCoeff,
                                       dl->qp_scalar,
                                       dl->dummy,
                                       ebName,
                                       "scalar",
                                       1.0);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (have_pressure_eq_) { // Biot Modulus
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Biot Modulus Name", biotModulus);
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      material_DB_->getElementBlockSublist(ebName,"Biot Modulus");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    // Setting this turns on linear dependence on porosity and Biot's coeffcient
    p->set<string>("Porosity Name", porosity);
    p->set<string>("Biot Coefficient Name", biotCoeff);

    ev = rcp(new LCM::BiotModulus<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable(biotModulus,
                                       dl->qp_scalar,
                                       dl->dummy,
                                       ebName,
                                       "scalar",
                                       1.0e20);
    // Very large value means incompressible phases
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (have_pressure_eq_ || have_heat_eq_) { // Thermal conductivity
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("QP Variable Name", "Thermal Conductivity");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      material_DB_->getElementBlockSublist(ebName,"Thermal Conductivity");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::ThermalConductivity<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (have_pressure_eq_) { // Kozeny-Carman Permeaiblity
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Kozeny-Carman Permeability Name", kcPerm);
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = 
      material_DB_->getElementBlockSublist(ebName,"Kozeny-Carman Permeability");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    // Setting this turns on Kozeny-Carman relation
    p->set<string>("Porosity Name", porosity);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    ev = rcp(new LCM::KCPermeability<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable(kcPerm,
                                       dl->qp_scalar,
                                       dl->dummy,
                                       ebName,
                                       "scalar",
                                       1.0); // Must be nonzero
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  // Pore Pressure Residual (Bulk Element)
  if (have_pressure_eq_ && !surface_element) {
    RCP<ParameterList> p = rcp(new ParameterList("Pore Pressure Residual"));

    //Input

    // Input from nodal points, basis function stuff
    p->set<string>("Weights Name","Weights");
    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout",
                              dl->node_qp_scalar);
    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    // Inputs: X, Y at nodes, Cubature, and Basis
    p->set<string>("Coordinate Vector Name","Coord Vec");
    p->set< RCP<DataLayout> >("Coordinate Data Layout", dl->vertices_vector);
    p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", cubature);
    p->set<RCP<shards::CellTopology> >("Cell Type", cellType);

    // DT for  time integration
    p->set<string>("Delta Time Name", "Delta Time");
    p->set< RCP<DataLayout> >("Workset Scalar Data Layout", dl->workset_scalar);


    p->set<bool>("Have Source", false);
    p->set<string>("Source Name", "Source");
    p->set<bool>("Have Absorption", false);

    // Input from cubature points
    p->set<string>("Element Length Name", "Gradient Element Length");
    p->set<string>("QP Pore Pressure Name", porePressure);
    p->set<string>("QP Time Derivative Variable Name", porePressure);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Material Property Name", "Stabilization Parameter");
    p->set<string>("Porosity Name", "Porosity");
    p->set<string>("Thermal Conductivity Name", "Thermal Conductivity");
    p->set<string>("Kozeny-Carman Permeability Name", kcPerm);
    p->set<string>("Biot Coefficient Name", biotCoeff);
    p->set<string>("Biot Modulus Name", biotModulus);

    p->set<string>("Gradient QP Variable Name", "Pore Pressure Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    if (have_mech_eq_) {
      p->set<bool>("Have Mechanics", true);
      p->set<string>("DefGrad Name", "F");
      p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);
      p->set<string>("DetDefGrad Name", "J");
      p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    }

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);

    //Output
    p->set<string>("Residual Name", "Pore Pressure Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", dl->node_scalar);

    ev = rcp(new LCM::TLPoroPlasticityResidMass<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

    // Output QP pore pressure
    p = stateMgr.registerStateVariable(porePressure,
                                       dl->qp_scalar,
                                       dl->dummy,
                                       ebName,
                                       "scalar",
                                       0.0,
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (have_pressure_eq_ && surface_element) { // Pore Pressure Resid for Surface
    RCP<ParameterList> p = rcp(new ParameterList("Pore Pressure Residual"));

    //Input
    p->set<RealType>("thickness",thickness);
    p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", surfaceCubature);
    p->set< RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >("Intrepid Basis", surfaceBasis);
    p->set<string>("Scalar Gradient Name", "Pore Pressure Gradient");
    p->set<string>("Scalar Jump Name", "Pore Pressure Jump");
    p->set<string>("Current Basis Name", "Current Basis");
    p->set<string>("Reference Dual Basis Name", "Reference Dual Basis");
    p->set<string>("Reference Normal Name", "Reference Normal");
    p->set<string>("Reference Area Name", "Reference Area");
    p->set<string>("Pore Pressure Name", porePressure);
    p->set<string>("Biot Coefficient Name", biotCoeff);
    p->set<string>("Biot Modulus Name", biotModulus);
    p->set<string>("Kozeny-Carman Permeability Name", kcPerm);
    p->set<string>("Delta Time Name", "Delta Time");
    if (have_mech_eq_) {
      p->set<string>("DefGrad Name", "F");
      p->set<string>("DetDefGrad Name", "J");
    }

    //Output
    p->set<string>("Residual Name", "Pore Pressure Residual");

    ev = rcp(new LCM::SurfaceTLPoroMassResidual<EvalT,AlbanyTraits>(*p,dl));
    fm0.template registerEvaluator<EvalT>(ev);

    // Output QP pore pressure
    p = stateMgr.registerStateVariable(porePressure,
                                       dl->qp_scalar,
                                       dl->dummy,
                                       ebName,
                                       "scalar",
                                       0.0,
                                       true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  // Transport problem class
  // if (have_transport_eq_){ // Constant Molar Volume
  //   RCP<ParameterList> p = rcp(new ParameterList);

  //   p->set<string>("Material Property Name", "Molar Volume");
  //   p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
  //   p->set<string>("Coordinate Vector Name", "Coord Vec");
  //   p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);
  //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //   Teuchos::ParameterList& paramList =
  //     material_DB_->getElementBlockSublist(ebName,"Molar Volume");
  //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //   ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Constant Partial Molar Volume
  //   RCP<ParameterList> p = rcp(new ParameterList);

  //   p->set<string>("Material Property Name", "Partial Molar Volume");
  //   p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
  //   p->set<string>("Coordinate Vector Name", "Coord Vec");
  //   p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);
  //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //   Teuchos::ParameterList& paramList =
  //     material_DB_->getElementBlockSublist(ebName,"Partial Molar Volume");
  //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //   ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Constant Stress Free Total Concentration
  //   RCP<ParameterList> p = rcp(new ParameterList);

  //   p->set<string>("Material Property Name", "Stress Free Total Concentration");
  //   p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
  //   p->set<string>("Coordinate Vector Name", "Coord Vec");
  //   p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);
  //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //   Teuchos::ParameterList& paramList =
  //     material_DB_->getElementBlockSublist(ebName,"Stress Free Total Concentration");
  //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //   ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Constant Avogadro Number
  //   RCP<ParameterList> p = rcp(new ParameterList);

  //   p->set<string>("Material Property Name", "Avogadro Number");
  //   p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
  //   p->set<string>("Coordinate Vector Name", "Coord Vec");
  //   p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);
  //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //   Teuchos::ParameterList& paramList =
  //     material_DB_->getElementBlockSublist(ebName,"Avogadro Number");
  //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //   ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Constant Trap Binding Energy
  //   RCP<ParameterList> p = rcp(new ParameterList);

  //   p->set<string>("Material Property Name", "Trap Binding Energy");
  //   p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
  //   p->set<string>("Coordinate Vector Name", "Coord Vec");
  //   p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);
  //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //   Teuchos::ParameterList& paramList =
  //     material_DB_->getElementBlockSublist(ebName,"Trap Binding Energy");
  //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //   ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Constant Ideal Gas Constant
  //   RCP<ParameterList> p = rcp(new ParameterList);

  //   p->set<string>("Material Property Name", "Ideal Gas Constant");
  //   p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
  //   p->set<string>("Coordinate Vector Name", "Coord Vec");
  //   p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);
  //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //   Teuchos::ParameterList& paramList =
  //     material_DB_->getElementBlockSublist(ebName,"Ideal Gas Constant");
  //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //   ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);

  //   //Output
  //   p = stateMgr.registerStateVariable("Ideal Gas Constant",
  //                                      dl->qp_scalar, dl->dummy,
  //                                      ebName, "scalar", 0.0);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Constant Diffusion Activation Enthalpy
  //   RCP<ParameterList> p = rcp(new ParameterList);

  //   p->set<string>("Material Property Name", "Diffusion Activation Enthalpy");
  //   p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
  //   p->set<string>("Coordinate Vector Name", "Coord Vec");
  //   p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);
  //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //   Teuchos::ParameterList& paramList =
  //     material_DB_->getElementBlockSublist(ebName,"Diffusion Activation Enthalpy");
  //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //   ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Constant Pre Exponential Factor
  //   RCP<ParameterList> p = rcp(new ParameterList);
  //   p->set<string>("Material Property Name", "Pre Exponential Factor");
  //   p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
  //   p->set<string>("Coordinate Vector Name", "Coord Vec");
  //   p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);
  //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //   Teuchos::ParameterList& paramList =
  //     material_DB_->getElementBlockSublist(ebName,"Pre Exponential Factor");
  //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //   ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Trapped Solvent
  //   RCP<ParameterList> p = rcp(new ParameterList);
  //   p->set<string>("Trapped Solvent Name", "Trapped Solvent");
  //   p->set<string>("QP Coordinate Vector Name", "Coord Vec");
  //   p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //   p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

  //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //   Teuchos::ParameterList& paramList =
  //     material_DB_->getElementBlockSublist(ebName,"Trapped Solvent");
  //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //   // Setting this turns on dependence on plastic multipler for J2 plasticity
  //   p->set<string>("eqps Name", "eqps");
  //   p->set<string>("Avogadro Number Name", "Avogadro Number");

  //   ev = rcp(new LCM::TrappedSolvent<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  //   p = stateMgr.registerStateVariable("Trapped Solvent",dl->qp_scalar,
  //                                      dl->dummy,
  //                                      ebName, "scalar", 0.0);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Strain Rate Factor
  //   RCP<ParameterList> p = rcp(new ParameterList);

  //   p->set<string>("Strain Rate Factor Name", "Strain Rate Factor");
  //   p->set<string>("QP Coordinate Vector Name", "Coord Vec");
  //   p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //   p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

  //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //   Teuchos::ParameterList& paramList =
  //     material_DB_->getElementBlockSublist(ebName,"Strain Rate Factor");
  //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //   // Setting this turns on dependence on plastic multipler for J2 plasticity
  //   p->set<string>("eqps Name", "eqps");
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //   p->set<string>("Trapped Solvent Name", "Trapped Solvent");

  //   ev = rcp(new LCM::StrainRateFactor<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  //   p = stateMgr.registerStateVariable("Strain Rate Factor",dl->qp_scalar,
  //                                      dl->dummy, ebName, "scalar", 0.0);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Diffusion Coefficient
  //   RCP<ParameterList> p = rcp(new ParameterList("Diffusion Coefficient"));

  //   //Input
  //   p->set<string>("Ideal Gas Constant Name", "Ideal Gas Constant");
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //   p->set<string>("Temperature Name", "Temperature");
  //   p->set<string>("Diffusion Activation Enthalpy Name",
  //                  "Diffusion Activation Enthalpy");
  //   p->set<string>("Pre Exponential Factor Name",
  //                  "Pre Exponential Factor");
    
  //   //Output
  //   p->set<string>("Diffusion Coefficient Name", "Diffusion Coefficient");
  //   ev = rcp(new LCM::DiffusionCoefficient<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  //   p = stateMgr.registerStateVariable("Diffusion Coefficient",dl->qp_scalar,
  //                                      dl->dummy, ebName, "scalar", 1.327e-16);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Equilibrium Constant
  //   RCP<ParameterList> p = rcp(new ParameterList("Equilibrium Constant"));

  //   //Input
  //   p->set<string>("Ideal Gas Constant Name", "Ideal Gas Constant");
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //   p->set<string>("Temperature Name", "Temperature");
  //   p->set<string>("Trap Binding Energy Name", "Trap Binding Energy");

  //   //Output
  //   p->set<string>("Equilibrium Constant Name", "Equilibrium Constant");

  //   ev = rcp(new LCM::EquilibriumConstant<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  //   p = stateMgr.registerStateVariable("Equilibrium Constant",
  //                                      dl->qp_scalar, dl->dummy,
  //                                      ebName, "scalar", 52.53);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Effective Diffusivity
  //   RCP<ParameterList> p = rcp(new ParameterList("Effective Diffusivity"));

  //   //Input
  //   p->set<string>("Equilibrium Constant Name", "Equilibrium Constant");
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //   p->set<string>("Lattice Concentration Name", "Transport");
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //   p->set<string>("Avogadro Number Name", "Avogadro Number");
  //   p->set<string>("Trapped Solvent Name", "Trapped Solvent");
  //   p->set<string>("Molar Volume Name", "Molar Volume");

  //   //Output
  //   p->set<string>("Effective Diffusivity Name", "Effective Diffusivity");
  //   ev = rcp(new LCM::EffectiveDiffusivity<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  //   p = stateMgr.registerStateVariable("Effective Diffusivity",
  //                                      dl->qp_scalar, dl->dummy,
  //                                      ebName, "scalar", 1.0);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Trapped Concentration
  //   RCP<ParameterList> p = rcp(new ParameterList("Trapped Concentration"));

  //   //Input
  //   p->set<string>("Trapped Solvent Name", "Trapped Solvent");
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //   p->set<string>("Lattice Concentration Name", "Transport");
  //   p->set<string>("Equilibrium Constant Name", "Equilibrium Constant");
  //   p->set<string>("Molar Volume Name", "Molar Volume");

  //   //Output
  //   p->set<string>("Trapped Concentration Name", "Trapped Concentration");
  //   ev = rcp(new LCM::TrappedConcentration<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  //   p = stateMgr.registerStateVariable("Trapped Concentration",dl->qp_scalar,
  //                                      dl->dummy, ebName,
  //                                      "scalar", 0.0, true);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Total Concentration
  //   RCP<ParameterList> p = rcp(new ParameterList("Total Concentration"));

  //   //Input
  //   p->set<string>("Lattice Concentration Name", "Transport");
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //   p->set<string>("Trapped Concentration Name", "Trapped Concentration");

  //   //Output
  //   p->set<string>("Total Concentration Name", "Total Concentration");


  //   ev = rcp(new LCM::TotalConcentration<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  //   p = stateMgr.registerStateVariable("Total Concentration",dl->qp_scalar,
  //                                      dl->dummy, ebName,
  //                                      "scalar", 0.0, true);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // if (have_transport_eq_){ // Tau Factor
  //   RCP<ParameterList> p = rcp(new ParameterList("Tau Contribution"));

  //   p->set<string>("Tau Contribution Name", "Tau Contribution");
  //   p->set<string>("QP Coordinate Vector Name", "Coord Vec");
  //   p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
  //   p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

  //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
  //   Teuchos::ParameterList& paramList =
  //     material_DB_->getElementBlockSublist(ebName,"Tau Contribution");
  //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

  //   // Input
  //   p->set<string>("Diffusion Coefficient Name", "Diffusion Coefficient");
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

  //   p->set<string>("QP Variable Name", "Transport");
  //   p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

  //   p->set<string>("Ideal Gas Constant Name", "Ideal Gas Constant");
  //   p->set<string>("Material Property Name", "Temperature");

  //   ev = rcp(new LCM::TauContribution<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  //   p = stateMgr.registerStateVariable("Tau Contribution",
  //                                      dl->qp_scalar,
  //                                      dl->dummy,
  //                                      ebName, "scalar", 0.0);
  //   ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
  //   fm0.template registerEvaluator<EvalT>(ev);
  // }

  // Hydrogen Transport model proposed in Foulk et al 2012
  if (have_transport_eq_ && !surface_element){
    RCP<ParameterList> p = rcp(new ParameterList("Transport Residual"));

    //Input
    p->set<string>("Element Length Name", "Gradient Element Length");
    p->set<string>("Material Property Name", "Stabilization Parameter");

    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);

    p->set<string>("Weights Name","Weights");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    p->set<string>("Gradient BF Name", "Grad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    p->set<string>("QP Variable Name", "Transport");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    //  p->set<bool>("Have Source", false);
    //  p->set<string>("Source Name", "Source");

    p->set<string>("eqps Name", "eqps");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Strain Rate Factor Name", "Strain Rate Factor");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Trapped Concentration Name", "Trapped Concentration");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Trapped Solvent Name", "Trapped Solvent");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Deformation Gradient Name", "F");
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    p->set<string>("Effective Diffusivity Name", "Effective Diffusivity");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Diffusion Coefficient Name", "Diffusion Coefficient");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("QP Variable Name", "Transport");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Gradient QP Variable Name", "Transport Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<string>("Gradient Hydrostatic Stress Name", "HydroStress Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<string>("Stress Name", cauchy);
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    p->set<string>("Tau Contribution Name", "Tau Contribution");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Delta Time Name", "Delta Time");
    p->set< RCP<DataLayout> >("Workset Scalar Data Layout", dl->workset_scalar);

    //Output
    p->set<string>("Residual Name", "Transport Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", dl->node_scalar);

    ev = rcp(new LCM::HDiffusionDeformationMatterResidual<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("Transport",dl->qp_scalar,
                                       dl->dummy, ebName, "scalar", 0.0, true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("Transport Gradient",
                                       dl->qp_vector, dl->dummy ,
                                       ebName, "scalar" , 0.0  , true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);

  }

  if (have_hydrostress_eq_ && !surface_element){ // L2 hydrostatic stress projection
    RCP<ParameterList> p = rcp(new ParameterList("HydroStress Residual"));

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >
      ("Node QP Scalar Data Layout", dl->node_qp_scalar);

    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >
      ("Node QP Vector Data Layout", dl->node_qp_vector);

    p->set<bool>("Have Source", false);
    p->set<string>("Source Name", "Source");

    p->set<string>("Deformation Gradient Name", "F");
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    p->set<string>("QP Variable Name", "HydroStress");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Stress Name", cauchy);
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    //Output
    p->set<string>("Residual Name", "HydroStress Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", dl->node_scalar);

    ev = rcp(new LCM::ScalarL2ProjectionResidual<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("HydroStress",dl->qp_scalar, dl->dummy,
                                       ebName, "scalar", 0.0, true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (fieldManagerChoice == Albany::BUILD_RESID_FM)  {
    Teuchos::RCP<const PHX::FieldTag> ret_tag;
    if (have_mech_eq_) {
      PHX::Tag<typename EvalT::ScalarT> res_tag("Scatter", dl->dummy);
      fm0.requireField<EvalT>(res_tag);
      ret_tag = res_tag.clone();
    }
    if (have_pressure_eq_) {
      PHX::Tag<typename EvalT::ScalarT> pres_tag("Scatter Pore_Pressure", dl->dummy);
      fm0.requireField<EvalT>(pres_tag);
      ret_tag = pres_tag.clone();
    }
    if (have_heat_eq_) {
      PHX::Tag<typename EvalT::ScalarT> heat_tag("Scatter Temperature", dl->dummy);
      fm0.requireField<EvalT>(heat_tag);
      ret_tag = heat_tag.clone();
    }
    if (have_transport_eq_) {
      PHX::Tag<typename EvalT::ScalarT> transport_tag("Scatter Transport", dl->dummy);
      fm0.requireField<EvalT>(transport_tag);
      ret_tag = transport_tag.clone();
    }
    if (have_hydrostress_eq_) {
      PHX::Tag<typename EvalT::ScalarT> l2projection_tag("Scatter HydroStress", dl->dummy);
      fm0.requireField<EvalT>(l2projection_tag);
      ret_tag = l2projection_tag.clone();
    }
    return ret_tag;
  }
  else if (fieldManagerChoice == Albany::BUILD_RESPONSE_FM) {
    Albany::ResponseUtilities<EvalT, PHAL::AlbanyTraits> respUtils(dl);
    return respUtils.constructResponses(fm0, *responseList, stateMgr);
  }

  return Teuchos::null;
}

#endif
