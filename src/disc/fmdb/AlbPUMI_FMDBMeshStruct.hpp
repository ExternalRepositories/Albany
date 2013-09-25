//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef ALBPUMI_FMDBMESHSTRUCT_HPP
#define ALBPUMI_FMDBMESHSTRUCT_HPP

#include "Albany_AbstractMeshStruct.hpp"
#include "AlbPUMI_QPData.hpp"

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Albany_StateInfoStruct.hpp"
#include "EpetraExt_MultiComm.h"
#include <PHAL_Dimension.hpp>

#include "pumi_mesh.h"
//#include "ModelTypes.h"
#ifdef SCOREC_ACIS
#include "AcisModel.h"
#endif
#ifdef SCOREC_PARASOLID
#include "ParasolidModel.h"
#endif
#ifdef SCOREC_MESHMODEL
#include "modelerDiscrete.h"
#endif

#define NG_EX_ENTITY_TYPE_MAX 15
#define ENT_DIMS 4

namespace AlbPUMI {

  class FMDBMeshStruct : public Albany::AbstractMeshStruct {

  public:

    FMDBMeshStruct(
                  const Teuchos::RCP<Teuchos::ParameterList>& params,
                  const Teuchos::RCP<const Epetra_Comm>& epetra_comm);

    ~FMDBMeshStruct();

    void setFieldAndBulkData(
                  const Teuchos::RCP<const Epetra_Comm>& comm,
                  const Teuchos::RCP<Teuchos::ParameterList>& params,
                  const unsigned int neq_,
                  const Albany::AbstractFieldContainer::FieldContainerRequirements& req,
                  const Teuchos::RCP<Albany::StateInfoStruct>& sis,
                  const unsigned int worksetSize);

    Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >& getMeshSpecs();

//    std::vector<std::string> scalarValue_states;
    std::vector<Teuchos::RCP<QPData<1> > > scalarValue_states;
    std::vector<Teuchos::RCP<QPData<2> > > qpscalar_states;
    std::vector<Teuchos::RCP<QPData<3> > > qpvector_states;
    std::vector<Teuchos::RCP<QPData<4> > > qptensor_states;

    std::vector<std::string> nsNames;
    std::vector<std::string> ssNames;

    msType meshSpecsType();
    pMeshMdl getMesh() { return mesh; }
    pumi::pGModel getMdl() { return model; }

    // Solution history
    int solutionFieldHistoryDepth;
    void loadSolutionFieldHistory(int step);

    bool hasRestartSolution;
    double restartDataTime;
    int neq;
    int numDim;
    bool interleavedOrdering;
    pTag residual_field_tag;
    pTag solution_field_tag;

    double time;

    // Info to map element block to physics set
    bool allElementBlocksHaveSamePhysics;
    std::map<std::string, int> ebNameToIndex;

    int worksetSize;

    std::string outputFileName;
    int outputInterval;
    int useDistributedMesh;

private:

    Teuchos::RCP<const Teuchos::ParameterList>
      getValidDiscretizationParameters() const;

    const CellTopologyData *getCellTopologyData(const FMDB_EntTopo topo);

    //! Utility function that uses some integer arithmetic to choose a good worksetSize
    int computeWorksetSize(const int worksetSizeMax, const int ebSizeMax) const;

    Teuchos::RCP<Teuchos::FancyOStream> out;

    Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> > meshSpecs;

    pumi::pGModel model;
    pMeshMdl mesh;
  };

}
#endif
