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


#ifndef ALBANY_FROMCUBIT_STKMESHSTRUCT_HPP
#define ALBANY_FROMCUBIT_STKMESHSTRUCT_HPP

#include "Albany_AbstractSTKMeshStruct.hpp"

#ifdef ALBANY_CUTR
#include "STKMeshData.hpp"

namespace Albany {

  struct FromCubitSTKMeshStruct : public AbstractSTKMeshStruct {

    FromCubitSTKMeshStruct(
                  STKMeshData* stkMeshData,
                  const Teuchos::RCP<Teuchos::ParameterList>& params,
                  const unsigned int neq_);

    ~FromCubitSTKMeshStruct();

    Teuchos::RCP<const Teuchos::ParameterList>
      getValidDiscretizationParameters() const;

    bool periodic;

  };

}
#endif

#endif // ALBANY_QUAD2D_STKMESHSTRUCT_HPP
