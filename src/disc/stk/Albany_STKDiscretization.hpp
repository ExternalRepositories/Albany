//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef ALBANY_STK_DISCRETIZATION_HPP
#define ALBANY_STK_DISCRETIZATION_HPP

#include <utility>
#include <vector>

#include "Albany_AbstractDiscretization.hpp"
#include "Albany_AbstractSTKMeshStruct.hpp"
#include "Albany_DataTypes.hpp"
#include "utility/Albany_ThyraUtils.hpp"
#include "utility/Albany_ThyraCrsGraphProxy.hpp"

#include "Albany_NullSpaceUtils.hpp"

// Start of STK stuff
#include <stk_mesh/base/BulkData.hpp>
#include <stk_mesh/base/Field.hpp>
#include <stk_mesh/base/FieldTraits.hpp>
#include <stk_mesh/base/MetaData.hpp>
#include <stk_mesh/base/Types.hpp>
#include <stk_util/parallel/Parallel.hpp>
#ifdef ALBANY_SEACAS
#include <stk_io/StkMeshIoBroker.hpp>
#endif

namespace Albany {

typedef shards::Array<GO, shards::NaturalOrder> GIDArray;

struct DOFsStruct
{
  Teuchos::RCP<const Thyra_VectorSpace> node_vs;
  Teuchos::RCP<const Thyra_VectorSpace> overlap_node_vs;
  Teuchos::RCP<const Thyra_VectorSpace> vs;
  Teuchos::RCP<const Thyra_VectorSpace> overlap_vs;
  NodalDOFManager                dofManager;
  NodalDOFManager                overlap_dofManager;
  std::vector<std::vector<LO>>   wsElNodeEqID_rawVec;
  std::vector<IDArray>           wsElNodeEqID;
  std::vector<std::vector<GO>>   wsElNodeID_rawVec;
  std::vector<GIDArray>          wsElNodeID;
};

struct NodalDOFsStructContainer
{
  typedef std::map<std::pair<std::string, int>, DOFsStruct> MapOfDOFsStructs;

  MapOfDOFsStructs mapOfDOFsStructs;
  std::map<std::string, MapOfDOFsStructs::const_iterator> fieldToMap;

  const DOFsStruct&
  getDOFsStruct(const std::string& field_name) const
  {
    return fieldToMap.find(field_name)->second->second;
  };  // TODO handole errors

  // IKT: added the following function, which may be useful for debugging.
  void
  printFieldToMap() const
  {
    typedef std::map<std::string, MapOfDOFsStructs::const_iterator>::
        const_iterator                  MapIterator;
    Teuchos::RCP<Teuchos::FancyOStream> out =
        Teuchos::VerboseObjectBase::getDefaultOStream();
    for (MapIterator iter = fieldToMap.begin(); iter != fieldToMap.end();
         iter++) {
      std::string key = iter->first;
      *out << "IKT Key: " << key << "\n";
      auto vs = getDOFsStruct(key).vs;
      *out << "IKT Vector Space \n: ";
      describe(vs, *out, Teuchos::VERB_EXTREME);
    }
  }

  void
  addEmptyDOFsStruct(
      const std::string& field_name,
      const std::string& meshPart,
      int                numComps)
  {
    if (numComps != 1)
      mapOfDOFsStructs.insert(make_pair(make_pair(meshPart, 1), DOFsStruct()));

    fieldToMap[field_name] =
        mapOfDOFsStructs
            .insert(make_pair(make_pair(meshPart, numComps), DOFsStruct()))
            .first;
  }
};

class STKDiscretization : public Albany::AbstractDiscretization {
 public:
  //! Constructor
  STKDiscretization(
      const Teuchos::RCP<Teuchos::ParameterList>&  discParams,
      Teuchos::RCP<Albany::AbstractSTKMeshStruct>& stkMeshStruct,
      const Teuchos::RCP<const Teuchos_Comm>&      comm,
      const Teuchos::RCP<Albany::RigidBodyModes>&  rigidBodyModes =
          Teuchos::null,
      const std::map<int, std::vector<std::string>>& sideSetEquations =
          std::map<int, std::vector<std::string>>());

  //! Destructor
  ~STKDiscretization();

  void printConnectivity() const;

  //! Get node vector space (owned and overlapped)
  Teuchos::RCP<const Thyra_VectorSpace> getNodeVectorSpace        () const { return m_node_vs; }
  Teuchos::RCP<const Thyra_VectorSpace> getOverlapNodeVectorSpace () const { return m_overlap_node_vs; }

  //! Get solution DOF vector space (owned and overlapped).
  Teuchos::RCP<const Thyra_VectorSpace> getVectorSpace        () const { return m_vs; }
  Teuchos::RCP<const Thyra_VectorSpace> getOverlapVectorSpace () const { return m_overlap_vs; }

  //! Get Field node vector space (owned and overlapped)
  Teuchos::RCP<const Thyra_VectorSpace> getNodeVectorSpace        (const std::string& field_name) const;
  Teuchos::RCP<const Thyra_VectorSpace> getOverlapNodeVectorSpace (const std::string& field_name) const;

  //! Get Field vector space (owned and overlapped)
  Teuchos::RCP<const Thyra_VectorSpace> getVectorSpace        (const std::string& field_name) const;
  Teuchos::RCP<const Thyra_VectorSpace> getOverlapVectorSpace (const std::string& field_name) const;

  //! Create a Jacobian operator (owned and overlapped)
  Teuchos::RCP<Thyra_LinearOp> createJacobianOp        () const { return m_graph_proxy->createOp();         }
  Teuchos::RCP<Thyra_LinearOp> createOverlapJacobianOp () const { return m_overlap_graph_proxy->createOp(); }

#ifdef ALBANY_AERAS
  //! Create implicit Jacobian operator (owned and overlapped) (for Aeras)
  Teuchos::RCP<Thyra_LinearOp> createImplicitJacobianOp        () const { return m_graph_proxy->createOp();         }
  Teuchos::RCP<Thyra_LinearOp> createImplicitOverlapJacobianOp () const { return m_overlap_graph_proxy->createOp(); }
#endif

  //! Modify CRS Graphs for Peridigm-Albany coupling
  void insertPeridigmNonzerosIntoGraph();

  bool isExplicitScheme() const { return false; }

  //! Get Node set lists (typedef in Albany_AbstractDiscretization.hpp)
  const NodeSetList&      getNodeSets      () const { return nodeSets;      }
  const NodeSetGIDsList&  getNodeSetGIDs   () const { return nodeSetGIDs;   }
  const NodeSetCoordList& getNodeSetCoords () const { return nodeSetCoords; }

  //! Get Side set lists (typedef in Albany_AbstractDiscretization.hpp)
  const SideSetList& getSideSets (const int workset) const { return sideSets[workset]; }

  //! Get connectivity map from elementGID to workset
        WsLIDList& getElemGIDws ()       { return elemGIDws; }
  const WsLIDList& getElemGIDws () const { return elemGIDws; }

  //! Get map from (Ws, El, Local Node) -> NodeLID
  using Albany::AbstractDiscretization::WorksetConn;
  using Albany::AbstractDiscretization::Conn;

  //! Get map from ws, elem, node [, eq] -> [Node|DOF] GID
  const Conn& getWsElNodeEqID () const { return wsElNodeEqID; }
  const WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO>>>::type& getWsElNodeID () const { return wsElNodeID; }

  //! Get IDArray for (Ws, Local Node, nComps) -> (local) NodeLID, works for
  //! both scalar and vector fields
  const std::vector<IDArray>& getElNodeEqID (const std::string& field_name) const {
    return nodalDOFsStructContainer.getDOFsStruct(field_name).wsElNodeEqID;
  }

  const NodalDOFManager& getDOFManager (const std::string& field_name) const {
    return nodalDOFsStructContainer.getDOFsStruct(field_name).dofManager;
  }

  const NodalDOFManager& getOverlapDOFManager (const std::string& field_name) const {
    return nodalDOFsStructContainer.getDOFsStruct(field_name).overlap_dofManager;
  }

  //! Retrieve coodinate vector (num_used_nodes * 3)
  const Teuchos::ArrayRCP<double>& getCoordinates () const;
  void setCoordinates (const Teuchos::ArrayRCP<const double>& c);
  void setReferenceConfigurationManager (const Teuchos::RCP<AAdapt::rc::Manager>& rcm);

#ifdef ALBANY_CONTACT
  //! Get the contact manager
  Teuchos::RCP<const Albany::ContactManager> getContactManager() const { return contactManager; }
#endif

  const WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*>>>::type& getCoords() const { return coords; }
  const WorksetArray<Teuchos::ArrayRCP<double>>::type& getSphereVolume() const { return sphereVolume; }
  const WorksetArray<Teuchos::ArrayRCP<double*>>::type& getLatticeOrientation() const { return latticeOrientation; }

  //! Print the coordinates for debugging
  void printCoords() const;

  //! Set stateArrays
  void setStateArrays(Albany::StateArrays& sa) {
    stateArrays = sa;
  }

  //! Get stateArrays
  Albany::StateArrays& getStateArrays() { return stateArrays; }

  //! Get nodal parameters state info struct
  const Albany::StateInfoStruct& getNodalParameterSIS() const {
    return stkMeshStruct->getFieldContainer()->getNodalParameterSIS();
  }

  //! Retrieve Vector (length num worksets) of element block names
  const Albany::WorksetArray<std::string>::type& getWsEBNames() const { return wsEBNames; }
  //! Retrieve Vector (length num worksets) of physics set index
  const Albany::WorksetArray<int>::type& getWsPhysIndex() const { return wsPhysIndex; }

  // Retrieve mesh struct
  Teuchos::RCP<Albany::AbstractSTKMeshStruct> getSTKMeshStruct() const {
    return stkMeshStruct;
  }
  Teuchos::RCP<Albany::AbstractMeshStruct>  getMeshStruct() const {
    return stkMeshStruct;
  }

  const SideSetDiscretizationsType& getSideSetDiscretizations() const {
    return sideSetDiscretizations;
  }

  const std::map<std::string, std::map<GO, GO>>& getSideToSideSetCellMap() const {
    return sideToSideSetCellMap;
  }

  const std::map<std::string, std::map<GO, std::vector<int>>>& getSideNodeNumerationMap() const {
    return sideNodeNumerationMap;
  }

  //! Flag if solution has a restart values -- used in Init Cond
  bool hasRestartSolution() const {
    return stkMeshStruct->hasRestartSolution();
  }

  //! If restarting, convenience function to return restart data time
  double restartDataTime() const {
    return stkMeshStruct->restartDataTime();
  }

  //! After mesh modification, need to update the element connectivity and nodal
  //! coordinates
  void updateMesh();

  //! Function that transforms an STK mesh of a unit cube (for LandIce problems)
  void transformMesh();

  //! Close current exodus file in stk_io and create a new one for an adapted
  //! mesh and new results
  void reNameExodusOutput(std::string& filename);

  //! Get number of spatial dimensions
  int getNumDim() const { return stkMeshStruct->numDim; }

  //! Get number of total DOFs per node
  int getNumEq() const { return neq; }

  //! Locate nodal dofs in non-overlapping vectors using local indexing
  int getOwnedDOF(const int inode, const int eq) const;

  //! Locate nodal dofs in overlapping vectors using local indexing
  int getOverlapDOF(const int inode, const int eq) const;

  //! Get global id of the stk entity
  GO gid (const stk::mesh::Entity entity) const;

  //! Locate nodal dofs using global indexing
  GO getGlobalDOF (const GO inode, const int eq) const;

  Teuchos::RCP<LayeredMeshNumbering<LO>> getLayeredMeshNumbering() const {
    return stkMeshStruct->layered_mesh_numbering;
  }

  const stk::mesh::MetaData& getSTKMetaData () const { return metaData; }
  const stk::mesh::BulkData& getSTKBulkData () const { return bulkData; }

  // --- Get/set solution/residual/field vectors to/from mesh --- //

  Teuchos::RCP<Thyra_Vector>      getSolutionField (const bool overlapped = false) const;
  Teuchos::RCP<Thyra_MultiVector> getSolutionMV (const bool overlapped = false) const;

  void setResidualField (const Thyra_Vector& residual);

  void getField (Thyra_Vector& field_vector, const std::string& field_name) const;
  void setField (const Thyra_Vector& field_vector, const std::string& field_name, const bool overlapped = false);

  // --- Methods to write solution in the output file --- //

  void writeSolution (const Thyra_Vector& solution,
                      const double time, const bool overlapped = false);
  void writeSolution (const Thyra_Vector& solution,
                      const Thyra_Vector& solution_dot,
                      const double time, const bool overlapped = false);
  void writeSolution (const Thyra_Vector& solution,
                      const Thyra_Vector& solution_dot,
                      const Thyra_Vector& solution_dotdot,
                      const double time, const bool overlapped = false);
  void writeSolutionMV (const Thyra_MultiVector& solution,
                        const double time, const bool overlapped = false);

  //! Write the solution to the mesh database.
  void writeSolutionToMeshDatabase (const Thyra_Vector& solution,
                                    const double /* time */, const bool overlapped = false);
  void writeSolutionToMeshDatabase (const Thyra_Vector& solution,
                                    const Thyra_Vector& solution_dot,
                                    const double /* time */, const bool overlapped = false);
  void writeSolutionToMeshDatabase (const Thyra_Vector& solution,
                                    const Thyra_Vector& solution_dot,
                                    const Thyra_Vector& solution_dotdot,
                                    const double /* time */, const bool overlapped = false);
  void writeSolutionMVToMeshDatabase (const Thyra_MultiVector &solution,
                                      const double /* time */, const bool overlapped = false);

  //! Write the solution to file. Must call writeSolution first.
  void writeSolutionToFile (const Thyra_Vector &solution,
                            const double time, const bool overlapped = false);
  void writeSolutionMVToFile (const Thyra_MultiVector &solution,
                              const double time, const bool overlapped = false);

private:

  void getSolutionField (Thyra_Vector&      result, bool overlapped) const;
  void getSolutionMV    (Thyra_MultiVector& result, bool overlapped) const;

  void setSolutionField (const Thyra_Vector& soln,
                         const bool overlapped);
  void setSolutionField (const Thyra_Vector& soln,
                         const Thyra_Vector& soln_dot,
                         const bool overlapped);
  void setSolutionField (const Thyra_Vector& soln,
                         const Thyra_Vector& soln_dot,
                         const Thyra_Vector& soln_dotdot,
                         const bool overlapped);
  void setSolutionFieldMV(const Thyra_MultiVector& solnT,
                          const bool overlapped);

  double monotonicTimeLabel(const double time);

  void computeNodalVectorSpaces(bool overlapped);

  //! Process STK mesh for CRS Graphs
  virtual void computeGraphs();
  //! Process STK mesh for Owned nodal quantitites
  void computeOwnedNodesAndUnknowns();
  //! Process coords for ML
  void setupMLCoords();
  //! Process STK mesh for Overlap nodal quantitites
  void computeOverlapNodesAndUnknowns();
  //! Process STK mesh for Workset/Bucket Info
  void computeWorksetInfo();
  //! Process STK mesh for NodeSets
  void computeNodeSets();
  //! Process STK mesh for SideSets
  void computeSideSets();
  //! Call stk_io for creating exodus output file
  void setupExodusOutput();
  //! Call stk_io for creating NetCDF output file
  void setupNetCDFOutput();

  int processNetCDFOutputRequest   (const Thyra_Vector&);
  int processNetCDFOutputRequestMV (const Thyra_MultiVector&);

  //! Find the local side id number within parent element
  unsigned determine_local_side_id(const stk::mesh::Entity elem, stk::mesh::Entity side);

  //! Convert the stk mesh on this processor to a nodal graph using SEACAS
  void meshToGraph();

  void writeCoordsToMatrixMarket() const;

  void buildSideSetProjectors();

  double previous_time_label;

  int nonzeroesPerRow(const int neq) const;

  // ==================== Members =================== //

  Teuchos::RCP<Teuchos::FancyOStream> out;


  //! Stk Mesh Objects
  stk::mesh::MetaData& metaData;
  stk::mesh::BulkData& bulkData;

  //! Teuchos communicator
  Teuchos::RCP<const Teuchos_Comm> comm;

  //! Unknown map and node map
  Teuchos::RCP<const Thyra_VectorSpace>   m_vs;
  Teuchos::RCP<const Thyra_VectorSpace>   m_node_vs;

  //! Overlapped unknown map and node map
  Teuchos::RCP<const Thyra_VectorSpace>   m_overlap_vs;
  Teuchos::RCP<const Thyra_VectorSpace>   m_overlap_node_vs;

  //! Jacobian matrix graph proxy (owned and overlap)
  Teuchos::RCP<ThyraCrsGraphProxy> m_graph_proxy;
  Teuchos::RCP<ThyraCrsGraphProxy> m_overlap_graph_proxy;

  NodalDOFsStructContainer nodalDOFsStructContainer;

  //! Jacobian matrix graph
  Teuchos::RCP<Tpetra_CrsGraph> graphT;

  //! Overlapped Jacobian matrix graph
  Teuchos::RCP<Tpetra_CrsGraph> overlap_graphT;

  //! Processor ID
  unsigned int myPID;

  //! Number of equations (and unknowns) per node
  const unsigned int neq;

  //! Equations that are defined only on some side sets of the mesh
  std::map<int, std::vector<std::string>> sideSetEquations;

  //! Number of elements on this processor
  unsigned int numMyElements;

  //! node sets stored as std::map(string ID, int vector of GIDs)
  Albany::NodeSetList      nodeSets;
  Albany::NodeSetGIDsList  nodeSetGIDs;
  Albany::NodeSetCoordList nodeSetCoords;

  //! side sets stored as std::map(string ID, SideArray classes) per workset
  //! (std::vector across worksets)
  std::vector<Albany::SideSetList> sideSets;

  //! Connectivity array [workset, element, local-node, Eq] => LID
  Conn wsElNodeEqID;

  //! Connectivity array [workset, element, local-node] => GID
  Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO>>>::type
      wsElNodeID;

  mutable Teuchos::ArrayRCP<double>       coordinates;
  Teuchos::RCP<Thyra_MultiVector>         coordMV;
  Albany::WorksetArray<std::string>::type wsEBNames;
  Albany::WorksetArray<int>::type         wsPhysIndex;
  Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*>>>::type
                                                         coords;
  Albany::WorksetArray<Teuchos::ArrayRCP<double>>::type  sphereVolume;
  Albany::WorksetArray<Teuchos::ArrayRCP<double*>>::type latticeOrientation;

#ifdef ALBANY_CONTACT
  Teuchos::RCP<Albany::ContactManager> contactManager;
#endif

  //! Connectivity map from elementGID to workset and LID in workset
  WsLIDList elemGIDws;

  // States: vector of length worksets of a map from field name to shards array
  Albany::StateArrays                           stateArrays;
  std::vector<std::vector<std::vector<double>>> nodesOnElemStateVec;

  //! list of all owned nodes, saved for setting solution
  std::vector<stk::mesh::Entity> ownednodes;
  std::vector<stk::mesh::Entity> cells;

  //! list of all overlap nodes, saved for getting coordinates for mesh motion
  std::vector<stk::mesh::Entity> overlapnodes;

  //! Number of elements on this processor
  int numOwnedNodes;
  int numOverlapNodes;
  GO  numGlobalNodes;

  // Needed to pass coordinates to ML.
  Teuchos::RCP<Albany::RigidBodyModes> rigidBodyModes;

  int              netCDFp;
  size_t           netCDFOutputRequest;
  std::vector<int> varSolns;

  //! used when NetCDF output on a latitude-longitude grid is requested.
  // Each struct contains a latitude/longitude index and it's parametric
  // coordinates in an element.
  struct interp
  {
    std::pair<double, double>     parametric_coords;
    std::pair<unsigned, unsigned> latitude_longitude;
  };

  Albany::WorksetArray<Teuchos::ArrayRCP<std::vector<interp>>>::type
      interpolateData;

  // Storage used in periodic BCs to un-roll coordinates. Pointers saved for
  // destructor.
  std::vector<double*> toDelete;

  Teuchos::RCP<Albany::AbstractSTKMeshStruct> stkMeshStruct;

  Teuchos::RCP<Teuchos::ParameterList> discParams;

  // Sideset discretizations
  std::map<std::string, Teuchos::RCP<Albany::AbstractDiscretization>>
      sideSetDiscretizations;
  std::map<std::string, Teuchos::RCP<Albany::STKDiscretization>>
      sideSetDiscretizationsSTK;
  std::map<std::string, std::map<GO, GO>>               sideToSideSetCellMap;
  std::map<std::string, std::map<GO, std::vector<int>>> sideNodeNumerationMap;
  std::map<std::string, Teuchos::RCP<Thyra_LinearOp>> projectors;
  std::map<std::string, Teuchos::RCP<Thyra_LinearOp>> ov_projectors;

// Used in Exodus writing capability
#ifdef ALBANY_SEACAS
  Teuchos::RCP<stk::io::StkMeshIoBroker> mesh_data;

  int outputInterval;

  size_t outputFileIdx;
#endif
  bool interleavedOrdering;

 private:
  Teuchos::RCP<Tpetra_CrsGraph> nodalGraph;

  // find the location of "value" within the first "count" locations of "vector"
  ssize_t
  in_list(const std::size_t value, std::size_t count, std::size_t* vector)
  {
    for (std::size_t i = 0; i < count; i++) {
      if (vector[i] == value) return i;
    }
    return -1;
  }

  ssize_t
  in_list(const std::size_t value, const Teuchos::Array<Tpetra_GO>& vector)
  {
    for (std::size_t i = 0; i < vector.size(); i++)
      if (vector[i] == value) return i;
    return -1;
  }

  ssize_t
  in_list(const std::size_t value, const std::vector<std::size_t>& vector)
  {
    for (std::size_t i = 0; i < vector.size(); i++)
      if (vector[i] == value) return i;
    return -1;
  }

  ssize_t
  entity_in_list(
      const stk::mesh::Entity&              value,
      const std::vector<stk::mesh::Entity>& vec)
  {
    for (std::size_t i = 0; i < vec.size(); i++)
      if (bulkData.identifier(vec[i]) == bulkData.identifier(value)) return i;
    return -1;
  }

  void
  printVertexConnectivity();

  void
  computeGraphsUpToFillComplete();
  void
  fillCompleteGraphs();
};
}

#endif  // ALBANY_STK_DISCRETIZATION_HPP
