//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#ifndef ALBANY_ASCII_STKMESHSTRUCT_HPP
#define ALBANY_ASCII_STKMESHSTRUCT_HPP

#include "Albany_GenericSTKMeshStruct.hpp"

//#include <Ionit_Initializer.h>

namespace Albany {

  class AsciiSTKMeshStruct : public GenericSTKMeshStruct {

    public:

    AsciiSTKMeshStruct(
                  const Teuchos::RCP<Teuchos::ParameterList>& params, 
                  const Teuchos::RCP<const Epetra_Comm>& epetra_comm);

    ~AsciiSTKMeshStruct();

    void setFieldAndBulkData(
                  const Teuchos::RCP<const Epetra_Comm>& comm,
                  const Teuchos::RCP<Teuchos::ParameterList>& params,
                  const unsigned int neq_,
                  const AbstractFieldContainer::FieldContainerRequirements& req,
                  const Teuchos::RCP<Albany::StateInfoStruct>& sis,
                  const unsigned int worksetSize);

    //! Flag if solution has a restart values -- used in Init Cond
    bool hasRestartSolution() const {return false; }

    //! If restarting, convenience function to return restart data time
    double restartDataTime() const {return -1.0; }

    private:
    //Ioss::Init::Initializer ioInit;

    Teuchos::RCP<const Teuchos::ParameterList>
      getValidDiscretizationParameters() const;

    Teuchos::RCP<Teuchos::FancyOStream> out;
    bool periodic;
    bool contigIDs; //boolean specifying if node / element / face IDs are contiguous; only relevant for 1 processor run 
    int NumNodes; //number of nodes
    int NumEles; //number of elements
    int NumBasalFaces; //number of faces on basal boundary
    double (*xyz)[3]; //hard-coded for 3D for now 
    double* sh;
    double* beta;
    int* globalElesID; //int array to define element map 
    int* globalNodesID; //int array to define node map 
    int* basalFacesID; //int array to define basal face map 
    int (*eles)[8]; //hard-coded for 3D hexes for now 
    double *flwa; //double array that gives value of flow factor  
    bool have_sh; // Does surface height data exist?
    bool have_bf; // Does basal face connectivity file exist?
    bool have_flwa; // Does flwa (flow factor) file exist?
    bool have_beta; // Does beta (basal fraction) file exist?
    int (*bf)[5]; //hard-coded for 3D hexes for now (meaning boundary faces are quads)
    Teuchos::RCP<Epetra_Map> elem_map; //element map 
    Teuchos::RCP<Epetra_Map> node_map; //node map 
    Teuchos::RCP<Epetra_Map> basal_face_map; //basalface map 
  };

}
#endif
