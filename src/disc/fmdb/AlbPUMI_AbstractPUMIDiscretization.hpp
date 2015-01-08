//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//



#ifndef ALBPUMI_ABSTRACTPUMIDISCRETIZATION_HPP
#define ALBPUMI_ABSTRACTPUMIDISCRETIZATION_HPP

#include "Albany_AbstractDiscretization.hpp"
#include "AlbPUMI_FMDBMeshStruct.hpp"

namespace AlbPUMI {

  class AbstractPUMIDiscretization : public Albany::AbstractDiscretization {
  public:

    //! Destructor
    virtual ~AbstractPUMIDiscretization(){}

    // Retrieve mesh struct
    virtual Teuchos::RCP<AlbPUMI::FMDBMeshStruct> getFMDBMeshStruct() = 0;

    virtual apf::GlobalNumbering* getAPFGlobalNumbering() = 0;

    virtual void attachQPData() = 0;
    virtual void detachQPData() = 0;

    // After mesh modification, need to update the element connectivity and nodal coordinates
    virtual void updateMesh(bool shouldTransferIPData) = 0;

#ifdef ALBANY_EPETRA
    virtual void debugMeshWriteNative(const Epetra_Vector& sol, const char* filename) = 0;
    virtual void debugMeshWrite(const Epetra_Vector& sol, const char* filename) = 0;
#endif

    virtual Teuchos::RCP<const Teuchos_Comm> getComm() const = 0;

    virtual void reNameExodusOutput(const std::string& str) = 0;

    //! Create a new field having a name and a value_type of apf::SCALAR,
    //! apf::VECTOR, or apf::MATRIX.
    virtual void createField(const char* name, int value_type) = 0;
    //! Copy field data to APF.
    virtual void setField(const char* name, const ST* data, bool overlapped,
                          int offset = 0) = 0;
    //! Copy field data from APF.
    virtual void getField(const char* name, ST* dataT, bool overlapped,
                          int offset = 0) const = 0;
  };

}

#endif // ALBANY_ABSTRACTPUMIDISCRETIZATION_HPP
