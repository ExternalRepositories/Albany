//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifdef ALBANY_SEACAS

#ifndef ALBANY_MPAS_STKMESHSTRUCT_HPP
#define ALBANY_MPAS_STKMESHSTRUCT_HPP

#include "Albany_GenericSTKMeshStruct.hpp"

#include <Ionit_Initializer.h>


namespace Albany {

  class MpasSTKMeshStruct : public GenericSTKMeshStruct {

    public:

	MpasSTKMeshStruct(const Teuchos::RCP<Teuchos::ParameterList>& params,
	                                               const Teuchos::RCP<const Epetra_Comm>& comm,
	                                               const std::vector<int>& indexToTriangleID, const std::vector<int>& verticesOnTria, int nGlobalTriangles);

	MpasSTKMeshStruct(const Teuchos::RCP<Teuchos::ParameterList>& params,
	                                             const Teuchos::RCP<const Epetra_Comm>& comm,
	                                             const std::vector<int>& indexToTriangleID, const std::vector<int>& verticesOnTria, int nGlobalTriangles, int numLayers, int Ordering = 0);

    ~MpasSTKMeshStruct();

    void setFieldAndBulkData(
                      const Teuchos::RCP<const Epetra_Comm>& comm,
                      const Teuchos::RCP<Teuchos::ParameterList>& params,
                      const unsigned int neq_,
                      const Teuchos::RCP<Albany::StateInfoStruct>& sis,
                      const unsigned int worksetSize);

    void constructMesh(
            const Teuchos::RCP<const Epetra_Comm>& comm,
            const Teuchos::RCP<Teuchos::ParameterList>& params,
            const Teuchos::RCP<Albany::StateInfoStruct>& sis,
            const std::vector<int>& indexToVertexID, const std::vector<double>& verticesCoords, const std::vector<bool>& isVertexBoundary, int nGlobalVertices,
            const std::vector<int>& verticesOnTria,
            const std::vector<bool>& isBoundaryEdge, const std::vector<int>& trianglesOnEdge, const std::vector<int>& trianglesPositionsOnEdge,
            const std::vector<int>& verticesOnEdge, const std::vector<int>& indexToEdgeID, int nGlobalEdges,
            const unsigned int worksetSize);

    void constructMesh(
		   const Teuchos::RCP<const Epetra_Comm>& comm,
		   const Teuchos::RCP<Teuchos::ParameterList>& params,
		   const Teuchos::RCP<Albany::StateInfoStruct>& sis,
		   const std::vector<int>& indexToVertexID, const std::vector<double>& verticesCoords, const std::vector<bool>& isVertexBoundary, int nGlobalVertices,
		   const std::vector<int>& verticesOnTria,
		   const std::vector<bool>& isBoundaryEdge, const std::vector<int>& trianglesOnEdge, const std::vector<int>& trianglesPositionsOnEdge,
		   const std::vector<int>& verticesOnEdge,
		   const std::vector<int>& indexToEdgeID, int nGlobalEdges,
		   const std::vector<int>& indexToTriangleID,
		   const unsigned int worksetSize,
		   int numLayers, int Ordering = 0);

    private:
    Ioss::Init::Initializer ioInit;

    Teuchos::RCP<const Teuchos::ParameterList>
      getValidDiscretizationParameters() const;

    Teuchos::RCP<Teuchos::FancyOStream> out;
    bool periodic;
    int NumEles; //number of elements
    Teuchos::RCP<Epetra_Map> elem_map; //element map 
    /*
    const std::vector<int>& indexToTriangleID;
    const std::vector<int>& verticesOnTria;
    const int nGlobalTriangles;
    const std::vector<int>& indexToVertexID;
    const std::vector<double>& verticesCoords;
    const std::vector<bool>& isVertexBoundary;
    const int nGlobalVertices;
    const std::vector<bool>& isBoundaryEdge;
    const std::vector<int>& trianglesOnEdge;
    const std::vector<int>& trianglesPositionsOnEdge;
    const std::vector<int>& verticesOnEdge;
    const std::vector<int>& indexToEdgeID;
    const int nGlobalEdges;
    const int numDim;
    const int numLayers;
    const int Ordering;
    */
  };

}
#endif
#endif
