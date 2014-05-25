//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef ALBPUMI_FMDBDISCRETIZATION_HPP
#define ALBPUMI_FMDBDISCRETIZATION_HPP

#include <vector>
#include <fstream>

#include "Teuchos_ParameterList.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Epetra_Comm.h"

#include "AlbPUMI_AbstractPUMIDiscretization.hpp"
#include "AlbPUMI_FMDBMeshStruct.hpp"
#include "AlbPUMI_FMDBVtk.hpp"
#include "AlbPUMI_FMDBExodus.hpp"

#include "Piro_NullSpaceUtils.hpp" // has defn of struct that holds null space info for ML

#include "Epetra_CrsMatrix.h"
#include "Epetra_Vector.h"

namespace AlbPUMI {

template<class Output>
  class FMDBDiscretization : public AbstractPUMIDiscretization {
  public:

    //! Constructor
    FMDBDiscretization(
       Teuchos::RCP<AlbPUMI::FMDBMeshStruct> fmdbMeshStruct,
       const Teuchos::RCP<const Epetra_Comm>& comm,
       const Teuchos::RCP<Piro::MLRigidBodyModes>& rigidBodyModes = Teuchos::null);


    //! Destructor
    ~FMDBDiscretization();

    //! Get Tpetra DOF map
    Teuchos::RCP<const Tpetra_Map> getMapT() const;

    //! Get Tpetra overlapped DOF map
    Teuchos::RCP<const Tpetra_Map> getOverlapMapT() const;

    //! Get Tpetra Jacobian graph
    Teuchos::RCP<const Tpetra_CrsGraph> getJacobianGraphT() const;

    //! Get Tpetra overlap Jacobian graph
    Teuchos::RCP<const Tpetra_CrsGraph> getOverlapJacobianGraphT() const;

    //! Get Tpetra Node map
    Teuchos::RCP<const Tpetra_Map> getNodeMapT() const;


    //! Process coords for ML
    void setupMLCoords();

    //! Get Node set lists (typedef in Albany_AbstractDiscretization.hpp)
    const Albany::NodeSetList& getNodeSets() const { return nodeSets; };
    const Albany::NodeSetCoordList& getNodeSetCoords() const { return nodeSetCoords; };

    //! Get Side set lists (typedef in Albany_AbstractDiscretization.hpp)
    const Albany::SideSetList& getSideSets(const int workset) const { return sideSets[workset]; };

   //! Get connectivity map from elementGID to workset
    Albany::WsLIDList& getElemGIDws() { return elemGIDws; };

    //! Get map from (Ws, El, Local Node) -> NodeLID
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > > >::type& getWsElNodeEqID() const;

    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > >::type& getWsElNodeID() const;

    //! Retrieve coodinate vector (num_used_nodes * 3)
    Teuchos::ArrayRCP<double>& getCoordinates() const;

    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type& getCoords() const;

    // FIXME - Dummy FELIX accessor functions
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type& getSurfaceHeight() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type& getTemperature() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type& getBasalFriction() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type& getThickness() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type& getSurfaceVelocity() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type& getVelocityRMS() const;
    const Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type& getFlowFactor() const;

    //! Print coords for debugging
    void printCoords() const;

   //! Get number of spatial dimensions
    int getNumDim() const { return fmdbMeshStruct->numDim; }

    virtual Teuchos::RCP<const Epetra_Comm> getComm() const { return comm; }

    //! Get number of total DOFs per node
    int getNumEq() const { return neq; }

    Albany::StateArrays& getStateArrays() {return stateArrays;};

    //! Retrieve Vector (length num worksets) of element block names
    const Albany::WorksetArray<std::string>::type&  getWsEBNames() const;
    //! Retrieve Vector (length num worksets) of physics set index
    const Albany::WorksetArray<int>::type&  getWsPhysIndex() const;

    void writeAnySolution(const ST* soln, const double time, const bool overlapped = false);
    void writeSolutionT(const Tpetra_Vector& soln, const double time, const bool overlapped = false);

    Teuchos::RCP<Tpetra_Vector> getSolutionFieldT() const;

    void setResidualFieldT(const Tpetra_Vector& residualT);

    // Retrieve mesh struct
    Teuchos::RCP<AlbPUMI::FMDBMeshStruct> getFMDBMeshStruct() {return fmdbMeshStruct;}
    Teuchos::RCP<Albany::AbstractMeshStruct> getMeshStruct() const {return fmdbMeshStruct;}

    //! Flag if solution has a restart values -- used in Init Cond
    bool hasRestartSolution() const {return fmdbMeshStruct->hasRestartSolution;}

    //! If restarting, convenience function to return restart data time
    double restartDataTime() const {return fmdbMeshStruct->restartDataTime;}

    //! FMDB does not support MOR
    virtual bool supportsMOR() const { return false; }

    apf::GlobalNumbering* getAPFGlobalNumbering() {return elementNumbering;}

    // Before mesh modification, qp data may be needed for solution transfer
    void attachQPData();

    // After mesh modification, qp data needs to be removed
    void detachQPData();

    // After mesh modification, need to update the element connectivity and nodal coordinates
    void updateMesh(bool shouldTransferIPData);

    //! Accessor function to get coordinates for ML. Memory controlled here.
    void getOwned_xyz(double **x, double **y, double **z, double **rbm,
                      int& nNodes, int numPDEs, int numScalar, int nullSpaceDim);

    // Function that transforms an FMDB mesh of a unit cube (for FELIX problems)
    // not supported in FMDB now
    void transformMesh(){}

    GO getDOF(const GO inode, const int eq) const
    {
      if (interleavedOrdering) return inode*neq + eq;
      else  return inode + numOwnedNodes*eq;
    }

    // Copy field data from Tpetra_Vector to APF
    void setField(
        const char* name,
        const ST* data,
        bool overlapped,
        int offset = 0);
    void setSplitFields(std::vector<std::string> names, std::vector<int> indices, 
        const ST* data, bool overlapped);

    // Copy field data from APF to Tpetra_Vector
    void getField(
        const char* name,
        ST* dataT,
        bool overlapped,
        int offset = 0) const;
    void getSplitFields(std::vector<std::string> names, std::vector<int> indices,
        ST* dataT, bool overlapped) const;

    // Rename exodus output file when the problem is resized
    void reNameExodusOutput(const std::string& str){ meshOutput.setFileName(str);}

    /* DAI: old Epetra functions still used by parts of Albany/Trilinos
       Remove when we get to full Tpetra */
    virtual Teuchos::RCP<const Epetra_Map>
    getMap() const { return map; }
    virtual Teuchos::RCP<const Epetra_Map>
    getOverlapMap() const { return overlap_map; }
    virtual Teuchos::RCP<const Epetra_CrsGraph>
    getJacobianGraph() const { return graph; }
    virtual Teuchos::RCP<const Epetra_CrsGraph>
    getOverlapJacobianGraph() const { return overlap_graph; }
    virtual Teuchos::RCP<const Epetra_Map>
    getNodeMap() const {
      fprintf(stderr,"PUMI Discretization unsupported call getNodeMap\n");
      abort();
      return Teuchos::RCP<const Epetra_Map>();
    }
    virtual Teuchos::RCP<Epetra_Vector> getSolutionField() const;
    virtual void setResidualField(const Epetra_Vector& residual);
    virtual void writeSolution(const Epetra_Vector&, const double, const bool);
    void setSolutionField(const Epetra_Vector&) {
      fprintf(stderr,"PUMI Discretization unsupported call setSolutionField\n");
      abort();
    }
    void debugMeshWriteNative(const Epetra_Vector&, const char*) {
      fprintf(stderr,"PUMI Discretization unsupported call debugMeshWriteNative\n");
      abort();
    }
    void debugMeshWrite(const Epetra_Vector&, const char*) {
      fprintf(stderr,"PUMI Discretization unsupported call debugMeshWrite\n");
      abort();
    }

  private:

    //! Private to prohibit copying
    FMDBDiscretization(const FMDBDiscretization&);

    //! Private to prohibit copying
    FMDBDiscretization& operator=(const FMDBDiscretization&);

    int nonzeroesPerRow(const int neq) const;
    double monotonicTimeLabel(const double time);

    //! Process FMDB mesh for Owned nodal quantitites
    void computeOwnedNodesAndUnknowns();
    //! Process FMDB mesh for Overlap nodal quantitites
    void computeOverlapNodesAndUnknowns();
    //! Process FMDB mesh for CRS Graphs
    void computeGraphs();
    //! Process FMDB mesh for Workset/Bucket Info
    void computeWorksetInfo();
    //! Process FMDB mesh for NodeSets
    void computeNodeSets();
    //! Process FMDB mesh for SideSets
    void computeSideSets();

    //! Transfer QPData to APF
    void copyQPScalarToAPF(unsigned nqp, QPData<double, 2>& state, apf::Field* f);
    void copyQPVectorToAPF(unsigned nqp, QPData<double, 3>& state, apf::Field* f);
    void copyQPTensorToAPF(unsigned nqp, QPData<double, 4>& state, apf::Field* f);
    void copyQPStatesToAPF(apf::Field* f, apf::FieldShape* fs);
    void removeQPStatesFromAPF();

    //! Transfer QP Fields from APF to QPData
    void copyQPScalarFromAPF(unsigned nqp, QPData<double, 2>& state, apf::Field* f);
    void copyQPVectorFromAPF(unsigned nqp, QPData<double, 3>& state, apf::Field* f);
    void copyQPTensorFromAPF(unsigned nqp, QPData<double, 4>& state, apf::Field* f);
    void copyQPStatesFromAPF();

    // ! Split Solution fields
    std::vector<std::string> solNames;
    std::vector<std::string> resNames;
    std::vector<int> solIndex;

    //! Call stk_io for creating exodus output file
    Teuchos::RCP<Teuchos::FancyOStream> out;

    double previous_time_label;

    // Transformation types for FELIX problems
    enum TRANSFORMTYPE {NONE, ISMIP_HOM_TEST_A};
    TRANSFORMTYPE transform_type;

  protected:

    //! Output object
    Output meshOutput;

    //! Stk Mesh Objects

    //! Epetra communicator
    Teuchos::RCP<const Epetra_Comm> comm;

   //! Tpetra communicator and Kokkos node
    Teuchos::RCP<const Teuchos::Comm<int> > commT;
    Teuchos::RCP<KokkosNode> nodeT;

    //! Node map
    Teuchos::RCP<const Tpetra_Map> node_mapT;

    //! Unknown Map
    Teuchos::RCP<const Tpetra_Map> mapT;
    Teuchos::RCP<Epetra_Map> map;

    //! Overlapped unknown map, and node map
    Teuchos::RCP<const Tpetra_Map> overlap_mapT;
    Teuchos::RCP<Epetra_Map> overlap_map;
    Teuchos::RCP<const Tpetra_Map> overlap_node_mapT;

    //! Jacobian matrix graph
    Teuchos::RCP<Tpetra_CrsGraph> graphT;
    Teuchos::RCP<Epetra_CrsGraph> graph;

    //! Overlapped Jacobian matrix graph
    Teuchos::RCP<Tpetra_CrsGraph> overlap_graphT;
    Teuchos::RCP<Epetra_CrsGraph> overlap_graph;

    //! Number of equations (and unknowns) per node
    const unsigned int neq;

    //! node sets stored as std::map(string ID, int vector of GIDs)
    Albany::NodeSetList nodeSets;
    Albany::NodeSetCoordList nodeSetCoords;

    //! side sets stored as std::map(string ID, SideArray classes) per workset (std::vector across worksets)
    std::vector<Albany::SideSetList> sideSets;

    //! Connectivity array [workset, element, local-node, Eq] => LID
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > > >::type wsElNodeEqID;

    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > >::type wsElNodeID;

    mutable Teuchos::ArrayRCP<double> coordinates;
    Albany::WorksetArray<std::string>::type wsEBNames;
    Albany::WorksetArray<int>::type wsPhysIndex;
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type coords;

    // FELIX unused variables (FIXME)
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type sHeight;
    Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type temperature;
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type basalFriction;
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > >::type thickness;
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type surfaceVelocity;
    Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type velocityRMS;
    Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type flowFactor;

    //! Connectivity map from elementGID to workset and LID in workset
    Albany::WsLIDList  elemGIDws;

    // States: vector of length num worksets of a map from field name to shards array
    Albany::StateArrays stateArrays;

    apf::GlobalNumbering* globalNumbering;
    apf::GlobalNumbering* elementNumbering;

    //! list of all overlap nodes, saved for setting solution
    apf::DynamicArray<apf::Node> nodes;

    //! Number of elements on this processor
    int numOwnedNodes;
    int numOverlapNodes;
    long numGlobalNodes;

    // Coordinate vector in format needed by ML. Need to own memory here.
    double *xx, *yy, *zz, *rr;
    bool allocated_xyz;

    Teuchos::RCP<AlbPUMI::FMDBMeshStruct> fmdbMeshStruct;

    bool interleavedOrdering;

    std::vector< std::vector<apf::MeshEntity*> > buckets; // bucket of elements

    // storage to save the node coordinates of the nodesets visible to this PE
    std::map<std::string, std::vector<double> > nodeset_node_coords;

    // Needed to pass coordinates to ML. 
    Teuchos::RCP<Piro::MLRigidBodyModes> rigidBodyModes;

    // counter for limiting data writes to output file
    int outputInterval;

  };

}

// Define macro for explicit template instantiation
#define FMDB_INSTANTIATE_TEMPLATE_CLASS_VTK(name) \
  template class name<AlbPUMI::FMDBVtk>;
#define FMDB_INSTANTIATE_TEMPLATE_CLASS_EXODUS(name) \
  template class name<AlbPUMI::FMDBExodus>;

#define FMDB_INSTANTIATE_TEMPLATE_CLASS(name) \
  FMDB_INSTANTIATE_TEMPLATE_CLASS_VTK(name) \
  FMDB_INSTANTIATE_TEMPLATE_CLASS_EXODUS(name)

#endif // ALBANY_FMDBDISCRETIZATION_HPP
