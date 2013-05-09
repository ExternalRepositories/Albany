//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#ifdef ALBANY_SEACAS

#ifndef ALBANY_IOSS_STKMESHSTRUCT_HPP
#define ALBANY_IOSS_STKMESHSTRUCT_HPP

#include "Albany_GenericSTKMeshStruct.hpp"
#include <stk_io/MeshReadWriteUtils.hpp>
#include <stk_io/IossBridge.hpp>

#include <Ionit_Initializer.h>

namespace Albany {

  class IossSTKMeshStruct : public GenericSTKMeshStruct {

    public:

    IossSTKMeshStruct(
                  const Teuchos::RCP<Teuchos::ParameterList>& params, 
                  const Teuchos::RCP<const Epetra_Comm>& epetra_comm);

    ~IossSTKMeshStruct();

    void setFieldAndBulkData(
                  const Teuchos::RCP<const Epetra_Comm>& comm,
                  const Teuchos::RCP<Teuchos::ParameterList>& params,
                  const unsigned int neq_,
                  const AbstractFieldContainer::FieldContainerRequirements& req,
                  const Teuchos::RCP<Albany::StateInfoStruct>& sis,
                  const unsigned int worksetSize);

    void loadSolutionFieldHistory(int step);
    int getSolutionFieldHistoryDept(){return m_solutionFieldHistoryDepth;}

    //! Flag if solution has a restart values -- used in Init Cond
    bool hasRestartSolution() const {return m_hasRestartSolution;}

    //! If restarting, convenience function to return restart data time
    double restartDataTime() const {return m_restartDataTime;}

    private:
    Ioss::Init::Initializer ioInit;

    Teuchos::RCP<const Teuchos::ParameterList>
      getValidDiscretizationParameters() const;

    void readSerialMesh(const Teuchos::RCP<const Epetra_Comm>& comm);

// Move back to stk::io someday
    void create_input_mesh(const std::string &mesh_type,
                       const std::string &mesh_filename,
                       stk::ParallelMachine comm,
                       stk::mesh::fem::FEMMetaData &fem_meta,
                       stk::io::MeshData &mesh_data);

    Teuchos::RCP<Teuchos::FancyOStream> out;
    bool usePamgen;
    bool useSerialMesh;
    bool periodic;
    stk::io::MeshData* mesh_data;

    bool m_hasRestartSolution;
    double m_restartDataTime;
    int m_solutionFieldHistoryDepth;

  };

}
#endif
#endif
