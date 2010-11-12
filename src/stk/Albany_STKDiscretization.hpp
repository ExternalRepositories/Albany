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


#ifndef ALBANY_STKDISCRETIZATION_HPP
#define ALBANY_STKDISCRETIZATION_HPP

#include <vector>

#include "Teuchos_ParameterList.hpp"
#include "Epetra_Comm.h"

#include "Albany_AbstractDiscretization.hpp"
#include "Albany_AbstractSTKMeshStruct.hpp"

#include "Epetra_CrsMatrix.h"
#include "Epetra_Vector.h"

// Start of STK stuff
#include <stk_util/parallel/Parallel.hpp>
#include <stk_mesh/base/Types.hpp>
#include <stk_mesh/base/MetaData.hpp>
#include <stk_mesh/base/BulkData.hpp>
#include <stk_mesh/base/Field.hpp>
#include <stk_mesh/fem/FieldTraits.hpp>
#ifdef ALBANY_IOSS
//  #include "Albany_IossSTKMeshStruct.hpp"
  #include <stk_io/util/UseCase_mesh.hpp>
#endif


namespace Albany {

  class STKDiscretization : public Albany::AbstractDiscretization {
  public:

    //! Constructor
    STKDiscretization(
       Teuchos::RCP<Albany::AbstractSTKMeshStruct> stkMeshStruct,
       const Teuchos::RCP<const Epetra_Comm>& comm);


    //! Destructor
    ~STKDiscretization();

    //! Get DOF map
    Teuchos::RCP<const Epetra_Map> getMap() const;

    //! Get overlapped DOF map
    Teuchos::RCP<const Epetra_Map> getOverlapMap() const;

    //! Get Jacobian graph
    Teuchos::RCP<const Epetra_CrsGraph> getJacobianGraph() const;

    //! Get overlap Jacobian graph
    Teuchos::RCP<const Epetra_CrsGraph> getOverlapJacobianGraph() const;

    //! Get Node map
    Teuchos::RCP<const Epetra_Map> getNodeMap() const; 

    //! Get Node set lists (typdef in Albany_AbstractDiscretization.hpp)
    const NodeSetList& getNodeSets() const { return nodeSets; };

    const std::vector<std::string>& getNodeSetIDs() const;

    //! Get map from (El, Local Node) -> NodeLID
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& getElNodeID() const;

    //! Retrieve coodinate vector (num_used_nodes * 3)
    Teuchos::ArrayRCP<double>& getCoordinates() const;

    // 
    void outputToExodus(const Epetra_Vector& soln);
 
    //! Get Cubature Degree from input file
    int getCubatureDegree() const {return stkMeshStruct->cubatureDegree;};

    //! Get Element Topology Type
    const CellTopologyData& getCellTopologyData() const;

    Teuchos::RCP<Epetra_Vector> getSolutionField() const;

  private:

    //! Private to prohibit copying
    STKDiscretization(const STKDiscretization&);

    //! Private to prohibit copying
    STKDiscretization& operator=(const STKDiscretization&);

    // dof calc  nodeID*neq+eqID
    inline int getDOF(stk::mesh::Entity& node, int eq) const;

    // Copy solution vector from Epetra_Vector into STK Mesh
    void setSolutionField(const Epetra_Vector& soln);

  protected:
    
    //! Epetra communicator
    Teuchos::RCP<const Epetra_Comm> comm;

    //! Element map
    Teuchos::RCP<Epetra_Map> elem_map;

    //! Node map
    Teuchos::RCP<Epetra_Map> node_map;

    //! Unknown Map
    Teuchos::RCP<Epetra_Map> map;

    //! Overlapped unknown map
    Teuchos::RCP<Epetra_Map> overlap_map;

    //! Jacobian matrix graph
    Teuchos::RCP<Epetra_CrsGraph> graph;

    //! Overlapped Jacobian matrix graph
    Teuchos::RCP<Epetra_CrsGraph> overlap_graph;

    //! Processor ID
    unsigned int myPID;

    //! Number of equations (and unknowns) per node
    const unsigned int neq;

    //! Number of elements on this processor
    unsigned int numMyElements;

    //! Number of nodes per element
    unsigned int nodes_per_element;

    //! node sets stored as std::map(string ID, int vector of GIDs)
    Albany::NodeSetList nodeSets;

    //! Just the node set ID strings
    std::vector<std::string> nodeSetIDs;

    Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > elNodeID;

    mutable Teuchos::ArrayRCP<double> coordinates;

    //! list of all owned nodes, saved for setting solution
    std::vector< stk::mesh::Entity * > ownednodes ;

    //! list of all overlap nodes, saved for getting coordinates for mesh motion
    std::vector< stk::mesh::Entity * > overlapnodes ;

    Teuchos::RCP<Albany::AbstractSTKMeshStruct> stkMeshStruct;

    // Used in Exodus writing capability
#ifdef ALBANY_IOSS
    stk::io::util::MeshData* mesh_data;
#endif
    mutable double time;

  };

}

#endif // ALBANY_STKDISCRETIZATION_HPP
