//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#ifndef ALBANY_APFMESHSTRUCT_HPP
#define ALBANY_APFMESHSTRUCT_HPP

#include "Albany_AbstractMeshStruct.hpp"
#include "Albany_PUMIQPData.hpp"
#include "Albany_PUMINodeData.hpp"

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Albany_StateInfoStruct.hpp"
#ifdef ALBANY_EPETRA
#include "EpetraExt_MultiComm.h"
#endif
#include <PHAL_Dimension.hpp>

#include <apf.h>
#include <apfMesh2.h>
#if defined(HAVE_STK) && defined(ALBANY_SEACAS)
#include <apfSTK.h>
#else
#include <apfAlbany.h>
#endif
#include <gmi.h>

namespace Albany {

class SolutionLayout {
  public:

 Teuchos::Array<std::string>& getTimeDeriv(int i){ return solNames[i]; }
 const Teuchos::Array<std::string>& getTimeDeriv(int i) const { return solNames[i]; }

 Teuchos::Array<int>& getTimeIdx(int i){ return solIndex[i]; }
 const Teuchos::Array<int>& getTimeIdx(int i) const { return solIndex[i]; }

 SolutionLayout* timeDeriv(int i){ td_val = i; return this; }

 std::string& getDOFIndex(int i){ return solNames[td_val][i]; }

 int& getDOFIndexSize(int i){ return solIndex[td_val][i]; }

 void resizeTimeDeriv(size_t size){ solNames.resize(size); solIndex.resize(size); }

 Teuchos::Array<Teuchos::Array<std::string> > solNames;
 Teuchos::Array<Teuchos::Array<int> > solIndex; // solIndex[time_deriv_vector][DOF_component]

 int td_val;

};

class APFMeshStruct : public Albany::AbstractMeshStruct {

  public:

    void init(const Teuchos::RCP<Teuchos::ParameterList>& params,
              const Teuchos::RCP<const Teuchos_Comm>& commT);

    virtual ~APFMeshStruct();

    void setFieldAndBulkData(
                  const Teuchos::RCP<const Teuchos_Comm>& commT,
                  const Teuchos::RCP<Teuchos::ParameterList>& params,
                  const unsigned int neq_,
                  const Albany::AbstractFieldContainer::FieldContainerRequirements& req,
                  const Teuchos::RCP<Albany::StateInfoStruct>& sis,
                  const unsigned int worksetSize,
                  const std::map<std::string,Teuchos::RCP<Albany::StateInfoStruct> >& /*side_set_sis*/ = {},
                  const std::map<std::string,AbstractFieldContainer::FieldContainerRequirements>& /*side_set_req*/ = {});


    void splitFields(Teuchos::Array<Teuchos::Array<std::string> >& fieldLayout);

    Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >& getMeshSpecs();
    const Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >& getMeshSpecs() const;


    std::vector<Teuchos::RCP<PUMIQPData<double, 1> > > scalarValue_states;
    std::vector<Teuchos::RCP<PUMIQPData<double, 2> > > qpscalar_states;
    std::vector<Teuchos::RCP<PUMIQPData<double, 3> > > qpvector_states;
    std::vector<Teuchos::RCP<PUMIQPData<double, 4> > > qptensor_states;

    std::vector<std::string> nsNames;
    std::vector<std::string> ssNames;

    apf::Mesh2* getMesh() { return mesh; }
    gmi_model* getMdl() { return model; }
    apf::StkModels& getSets() { return sets; }

    // Solution history
    int solutionFieldHistoryDepth;
    void loadSolutionFieldHistory(int step);

    bool useCompositeTet(){ return compositeTet; }

    const Albany::DynamicDataArray<Albany::CellSpecs>::type& getMeshDynamicData() const
        { return meshDynamicData; }

    //! Process PUMI mesh for element block specific info
    void setupMeshBlkInfo();

    virtual apf::Field* createNodalField(char const* name, int valueType) = 0;

    bool hasRestartSolution;
    double restartDataTime;
    int restartWriteStep;

    int neq;
    int numDim;
    int cubatureDegree;
    bool interleavedOrdering;
    bool solutionInitialized;
    bool residualInitialized;

    Teuchos::Array<Teuchos::Array<std::string> > solVectorLayout;

    double time;

    // Info to map element block to physics set
    bool allElementBlocksHaveSamePhysics;
    std::map<std::string, int> ebNameToIndex;

    int worksetSize;

    std::string outputFileName;
    int outputInterval;
    bool useNullspaceTranslationOnly;
    bool useTemperatureHack;
    bool useDOFOffsetHack;

    // Number of distinct solution vectors handled (<=3)
    int num_time_deriv;

    static const char* solution_name[3];
    static const char* residual_name;

protected:

    Teuchos::RCP<Teuchos::ParameterList>
      getValidDiscretizationParameters() const;

    //! Utility function that uses some integer arithmetic to choose a good worksetSize
    int computeWorksetSize(const int worksetSizeMax, const int ebSizeMax) const;

    Teuchos::RCP<Teuchos::FancyOStream> out;

    Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> > meshSpecs;

    // Information that changes when the mesh adapts
    Albany::DynamicDataArray<Albany::CellSpecs>::type meshDynamicData;

    apf::Mesh2* mesh;
    gmi_model* model;
    apf::StkModels sets;

    bool compositeTet;

};

}

#endif
