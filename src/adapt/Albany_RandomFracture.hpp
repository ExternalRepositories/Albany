//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#ifndef ALBANY_RANDOM_HPP
#define ALBANY_RANDOM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractAdapter.hpp"


// Uses LCM Topology util class
// Note that all topology functions are in Albany::LCM namespace
#include "Topology.h"
#include "Fracture.h"
#include "Albany_STKDiscretization.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"


namespace Albany {

  class RandomFracture : public AbstractAdapter {
  public:

    RandomFracture(const Teuchos::RCP<Teuchos::ParameterList>& params_,
                   const Teuchos::RCP<ParamLib>& paramLib_,
                   Albany::StateManager& StateMgr_,
                   const Teuchos::RCP<const Epetra_Comm>& comm_);
    //! Destructor
    ~RandomFracture();

    //! Check adaptation criteria to determine if the mesh needs adapting
    virtual bool queryAdaptationCriteria();

    //! Apply adaptation method to mesh and problem. Returns true if adaptation is performed successfully.
    virtual bool adaptMesh();

    //! Transfer solution between meshes.
    virtual void solutionTransfer(const Epetra_Vector& oldSolution,
                                  Epetra_Vector& newSolution);

    //! Each adapter must generate it's list of valid parameters
    Teuchos::RCP<const Teuchos::ParameterList> getValidAdapterParameters() const;

  private:

    // Disallow copy and assignment
    RandomFracture(const RandomFracture &);
    RandomFracture &operator=(const RandomFracture &);

    void showTopLevelRelations();
    void showRelations();
    void showRelations(int level, const stk::mesh::Entity& ent);

    // Parallel all-reduce function. Returns the argument in serial, returns the sum of the
    // argument in parallel
    int  accumulateFractured(int num_fractured);

    // Parallel all-gatherv function. Communicates local open list to all processors to form global open list.
    void getGlobalOpenList( std::map<stk::mesh::EntityKey, bool>& local_entity_open,  
          std::map<stk::mesh::EntityKey, bool>& global_entity_open);

    // Build topology object from ../LCM/utils/topology.h

    stk::mesh::BulkData* bulkData;

    Teuchos::RCP<Albany::AbstractSTKMeshStruct> stkMeshStruct;

    Teuchos::RCP<Albany::AbstractDiscretization> disc;

    Albany::STKDiscretization *stk_discretization;

    stk::mesh::fem::FEMMetaData * metaData;

    stk::mesh::EntityRank nodeRank;
    stk::mesh::EntityRank edgeRank;
    stk::mesh::EntityRank faceRank;
    stk::mesh::EntityRank elementRank;

    Teuchos::RCP<LCM::AbstractFractureCriterion> sfcriterion;
    Teuchos::RCP<LCM::topology> topology;

    //! Edges to fracture the mesh on
    std::vector<stk::mesh::Entity*> fractured_edges;

    int numDim;
    int remeshFileIndex;
    std::string baseExoFileName;

    int fracture_interval_;
    double fracture_probability_;
  };

}

#endif //ALBANY_RANDOM_HPP
