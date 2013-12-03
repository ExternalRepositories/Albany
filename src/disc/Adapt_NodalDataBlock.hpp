//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#ifndef ADAPT_NODALDATABLOCK_HPP
#define ADAPT_NODALDATABLOCK_HPP

#include "Teuchos_RCP.hpp"
#include "Albany_DataTypes.hpp"
#include "Albany_AbstractNodeFieldContainer.hpp"

namespace Adapt {

/*!
 * \brief This is a container class that deals with managing data values at the nodes of a mesh.
 *
 */
class NodalDataBlock {

  public:

    NodalDataBlock(const Teuchos::RCP<Albany::NodeFieldContainer>& container_,
                   const Teuchos::RCP<const Epetra_Comm>& comm_);

    //! Destructor
    virtual ~NodalDataBlock(){}

    void resizeLocalMap( int numGlobalNodes,
                         int blocksize,
                         const std::vector<int>& local_nodeGIDs);

    void resizeOverlapMap(const std::vector<int>& overlap_nodeGIDs);

    Teuchos::RCP<Epetra_Vector> getOverlapNodeVec(){ return overlap_node_vec; }
    Teuchos::RCP<Epetra_Vector> getLocalNodeVec(){ return local_node_vec; }

    Teuchos::RCP<const Epetra_BlockMap> getOverlapMap() const { return overlap_node_map; }
    Teuchos::RCP<const Epetra_BlockMap> getMap() const { return local_node_map; }

    void initializeVectors(double value){overlap_node_vec->PutScalar(value); local_node_vec->PutScalar(value); }

    void initializeExport();

    void exportNodeDataArray(const std::string& field_name);

  private:

    Teuchos::RCP<const Epetra_BlockMap> overlap_node_map;
    Teuchos::RCP<const Epetra_BlockMap> local_node_map;

    Teuchos::RCP<Epetra_Vector> overlap_node_vec;
    Teuchos::RCP<Epetra_Vector> local_node_vec;

    Teuchos::RCP<Epetra_Export> exporter;

    Teuchos::RCP<Albany::NodeFieldContainer> nodeContainer;

    const Teuchos::RCP<const Epetra_Comm> comm;

    int blocksize;
    int numGlobalNodes;

};


}

#endif // ADAPT_NODALDATABLOCK_HPP
