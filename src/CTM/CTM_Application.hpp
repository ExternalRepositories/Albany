#ifndef CTM_APPLICATION_HPP
#define CTM_APPLICATION_HPP

#include <vector>
//
#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
//
#include "PHAL_AlbanyTraits.hpp"
#include "PHAL_Workset.hpp"
#include "Phalanx.hpp"
#include "PHAL_Utilities.hpp"
//
#include "Albany_AbstractProblem.hpp"
#include "Albany_AbstractDiscretization.hpp"

namespace CTM {

    class SolutionInfo;

    class Application {
    public:
        Application(Teuchos::RCP<Teuchos::ParameterList> p,
                Teuchos::RCP<SolutionInfo> sinfo,
                Teuchos::RCP<Albany::AbstractProblem> prob,
                Teuchos::RCP<Albany::AbstractDiscretization> d);
        // prohibit copy constructor
        Application(const Application& app) = delete;
        //! Destructor
        ~Application();

        //! Get underlying abstract discretization
        Teuchos::RCP<Albany::AbstractDiscretization> getDiscretization() const;

        //! Get problem object
        Teuchos::RCP<Albany::AbstractProblem> getProblem() const;

        Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >
        getEnrichedMeshSpecs() const {
            return meshSpecs;
        }

        //

        int getNumEquations() const {
            return neq;
        }
        //

        int getSpatialDimension() const {
            return numDim;
        }

        //! Routine to get workset (bucket) size info needed by all Evaluation types
        template <typename EvalT>
        void loadWorksetBucketInfo(PHAL::Workset& workset, const int& ws);

        //! Routine to load common sideset info into workset
        void loadWorksetSidesetInfo(PHAL::Workset& workset, const int ws);

        void postRegSetup(std::string eval);

    private:
        // Problem parameter list
        Teuchos::RCP<Teuchos::ParameterList> params;

        // solution info
        Teuchos::RCP<SolutionInfo> solution_info;

        //! Output stream, defaults to pronting just Proc 0
        Teuchos::RCP<Teuchos::FancyOStream> out;

        // Problem to be solved
        Teuchos::RCP<Albany::AbstractProblem> problem;

        // discretization
        Teuchos::RCP<Albany::AbstractDiscretization> disc;

        //! mesh specs
        Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> > meshSpecs;
        
        //! Phalanx Field Manager for volumetric fills
        Teuchos::ArrayRCP<Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > > fm;

        //! Phalanx Field Manager for Dirichlet Conditions
        Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > dfm;

        //! Phalanx Field Manager for Neumann Conditions
        Teuchos::ArrayRCP<Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > > nfm;

        std::set<std::string> setupSet;
        mutable int phxGraphVisDetail;
        mutable int stateGraphVisDetail;
        bool explicit_scheme;

        unsigned int neq, numDim;

    };

    template <typename EvalT>
    void Application::loadWorksetBucketInfo(PHAL::Workset& workset,
            const int& ws) {

        const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<LO> > > >::type&
                wsElNodeEqID = disc->getWsElNodeEqID();
        const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> > >::type&
                wsElNodeID = disc->getWsElNodeID();
        const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type&
                coords = disc->getCoords();
        const Albany::WorksetArray<std::string>::type& wsEBNames = disc->getWsEBNames();

        workset.numCells = wsElNodeEqID[ws].size();
        workset.wsElNodeEqID = wsElNodeEqID[ws];
        workset.wsElNodeID = wsElNodeID[ws];
        workset.wsCoords = coords[ws];
        workset.EBName = wsEBNames[ws];
        workset.wsIndex = ws;

        workset.local_Vp.resize(workset.numCells);

        // Sidesets are integrated within the Cells
        loadWorksetSidesetInfo(workset, ws);
        //        workset.stateArrayPtr = &stateMgr.getStateArray(Albany::StateManager::ELEM, ws);


        //  workset.wsElNodeEqID_kokkos =
        Kokkos::View<int***, PHX::Device> wsElNodeEqID_kokkos("wsElNodeEqID_kokkos", workset.numCells, wsElNodeEqID[ws][0].size(), wsElNodeEqID[ws][0][0].size());
        workset.wsElNodeEqID_kokkos = wsElNodeEqID_kokkos;
        for (int i = 0; i < workset.numCells; i++)
            for (int j = 0; j < wsElNodeEqID[ws][0].size(); j++)
                for (int k = 0; k < wsElNodeEqID[ws][0][0].size(); k++)
                    workset.wsElNodeEqID_kokkos(i, j, k) = workset.wsElNodeEqID[i][j][k];
    }

}

#endif