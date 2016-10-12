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

        // get solution info
        Teuchos::RCP<SolutionInfo> getSolutionInfo() const;

        //! Routine to get workset (bucket) size info needed by all Evaluation types
        template <typename EvalT>
        void loadWorksetBucketInfo(PHAL::Workset& workset, const int& ws);

        //! Routine to load common sideset info into workset
        void loadWorksetSidesetInfo(PHAL::Workset& workset, const int ws);

        void loadBasicWorksetInfoT(PHAL::Workset& workset,
                double current_time);

        void loadWorksetJacobianInfo(PHAL::Workset& workset,
                const double& alpha, const double& beta, const double& omega);

        //! Routine to load common nodeset info into workset
        void loadWorksetNodesetInfo(PHAL::Workset& workset);

        void postRegSetup(std::string eval);

        void computeGlobalResidualT(const double current_time,
                const Tpetra_Vector* xdotT,
                const Tpetra_Vector* xdotdotT,
                const Tpetra_Vector& xT,
                Tpetra_Vector& fT);

    private:

        void computeGlobalResidualImplT(const double current_time,
                const Teuchos::RCP<const Tpetra_Vector>& xdotT,
                const Teuchos::RCP<const Tpetra_Vector>& xdotdotT,
                const Teuchos::RCP<const Tpetra_Vector>& xT,
                const Teuchos::RCP<Tpetra_Vector>& fT);

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

        unsigned int neq, numDim;

        //! Integer specifying whether user wants to write Jacobian to MatrixMarket file
        // writeToMatrixMarketJac = 0: no writing to MatrixMarket (default)
        // writeToMatrixMarketJac =-1: write to MatrixMarket every time a Jacobian arises
        // writeToMatrixMarketJac = N: write N^th Jacobian to MatrixMarket
        // ...and similarly for writeToMatrixMarketRes (integer specifying whether user wants to write
        // residual to MatrixMarket file)
        int writeToMatrixMarketJac;
        int writeToMatrixMarketRes;
        //! Integer specifying whether user wants to write Jacobian and residual to Standard output (cout)
        int writeToCoutJac;
        int writeToCoutRes;

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