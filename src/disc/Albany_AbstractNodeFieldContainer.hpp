//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#ifndef ALBANY_ABSTRACTNODEFIELDCONT_HPP
#define ALBANY_ABSTRACTNODEFIELDCONT_HPP

#include "Teuchos_RCP.hpp"
#include "Epetra_Map.h"
#include "Epetra_Vector.h"



namespace Albany {

/*!
 * \brief Abstract interface for an STK NodeField container
 *
 */

class AbstractNodeFieldContainer {

  public:

    AbstractNodeFieldContainer(){}
    virtual ~AbstractNodeFieldContainer(){}

    virtual void saveField(const Teuchos::RCP<Epetra_Vector>& block_mv) = 0;
    virtual std::size_t numComponents() = 0;

};

typedef std::map<std::string, Teuchos::RCP<Albany::AbstractNodeFieldContainer> > NodeFieldContainer;

}

#endif // ALBANY_ABSTRACTNODEFIELDCONT_HPP
