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


#ifndef ALBANY_BCUTILS_HPP
#define ALBANY_BCUTILS_HPP

#include <vector>
#include <string>

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_DataTypes.hpp"
#include "PHAL_AlbanyTraits.hpp"
#include "Phalanx.hpp"
#include "PHAL_FactoryTraits.hpp"

#include "QCAD_MaterialDatabase.hpp"


namespace Albany {

  /*!
   * \brief Generic Functions to help define BC Field Manager
   */

  //! Traits classes used for BCUtils
  struct DirichletTraits {

    enum { type = PHAL::DirichletFactoryTraits<PHAL::AlbanyTraits>::id_dirichlet };
    enum { typeTd = PHAL::DirichletFactoryTraits<PHAL::AlbanyTraits>::id_timedep_bc };
    enum { typeKf = PHAL::DirichletFactoryTraits<PHAL::AlbanyTraits>::id_kfield_bc };
    enum { typeTo = PHAL::DirichletFactoryTraits<PHAL::AlbanyTraits>::id_torsion_bc };
    enum { typeDa = PHAL::DirichletFactoryTraits<PHAL::AlbanyTraits>::id_dirichlet_aggregator };

    static const std::string bcParamsPl;

  };

  struct NeumannTraits { 

    enum { type = PHAL::NeumannFactoryTraits<PHAL::AlbanyTraits>::id_neumann };
    enum { typeNa = PHAL::NeumannFactoryTraits<PHAL::AlbanyTraits>::id_neumann_aggregator };
    enum { typeGCV = PHAL::NeumannFactoryTraits<PHAL::AlbanyTraits>::id_gather_coord_vector };
    enum { typeGS = PHAL::NeumannFactoryTraits<PHAL::AlbanyTraits>::id_gather_solution };

    static const std::string bcParamsPl;

  };


/*
template<typename BCTraits>

  class BCUtils {

   public:

    //! Type of traits class being used
    typedef BCTraits traits_type;

    BCUtils() {};

    //! Generic implementation of Field Manager for BCs
    Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > 
    constructBCEvaluators(
       const std::vector<std::string>& nodeorsideSetIDs,
       const std::vector<std::string>& bcNames,
       Teuchos::RCP<Teuchos::ParameterList> params,
       Teuchos::RCP<ParamLib> paramLib);

    //! Function to return valid list of parameters in BC section of input file
    Teuchos::RCP<const Teuchos::ParameterList> getValidBCParameters(
                 const std::vector<std::string>& nodeorsideSetIDs,
                 const std::vector<std::string>& bcNames) const;

  private:

    //! Local utility function to construct unique string from Nodeset/Sideset name and dof name
    std::string constructBCName(const std::string ns, const std::string dof,
        const std::string condition = "N/A") const;

    //! Local utility function to construct unique string from Nodeset/Sideset name and dof name
    std::string constructTimeDepBCName(const std::string ns, const std::string dof,
        const std::string condition = "N/A") const;

  };
*/
template<typename BCTraits>

  class BCUtils {};

template<>

  class BCUtils<DirichletTraits> {

   public:

    //! Type of traits class being used
    typedef DirichletTraits traits_type;

    BCUtils() {};

    //! Generic implementation of Field Manager for BCs
    Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > 
    constructBCEvaluators(
       const std::vector<std::string>& nodeorsideSetIDs,
       const std::vector<std::string>& bcNames,
       Teuchos::RCP<Teuchos::ParameterList> params,
       Teuchos::RCP<ParamLib> paramLib);

    //! Function to return valid list of parameters in BC section of input file
    Teuchos::RCP<const Teuchos::ParameterList> getValidBCParameters(
                 const std::vector<std::string>& nodeorsideSetIDs,
                 const std::vector<std::string>& bcNames) const;

  private:

    //! Local utility function to construct unique string from Nodeset/Sideset name and dof name
    std::string constructBCName(const std::string ns, const std::string dof) const;

    //! Local utility function to construct unique string from Nodeset/Sideset name and dof name
    std::string constructTimeDepBCName(const std::string ns, const std::string dof) const;

  };

template<>

  class BCUtils<NeumannTraits> {

   public:

    //! Type of traits class being used
    typedef NeumannTraits traits_type;

    BCUtils() {};

    //! Generic implementation of Field Manager for BCs
    Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > 
    constructBCEvaluators(
       const Teuchos::RCP<Albany::MeshSpecsStruct>& meshSpecs,
       const std::vector<std::string>& bcNames,
       const Teuchos::ArrayRCP<string>& dof_names,
       bool isVectorField, 
       int offsetToFirstDOF, 
       const std::vector<std::string>& conditions,
       const Teuchos::Array<Teuchos::Array<int> >& offsets,
       const Teuchos::RCP<Albany::Layouts>& dl,
       Teuchos::RCP<Teuchos::ParameterList> params,
       Teuchos::RCP<ParamLib> paramLib,
       const Teuchos::RCP<QCAD::MaterialDatabase>& materialDB = Teuchos::null,
       bool isTensorField = false);

    //! Function to return valid list of parameters in BC section of input file
    Teuchos::RCP<const Teuchos::ParameterList> getValidBCParameters(
                 const std::vector<std::string>& nodeorsideSetIDs,
                 const std::vector<std::string>& bcNames,
                 const std::vector<std::string>& condition) const;

    //! Function to check if the Neumann BC section of input file is present
    bool haveNeumann(const Teuchos::RCP<Teuchos::ParameterList>& params) const; 

  private:

    int numDim;

    //! Local utility function to construct unique string from Nodeset/Sideset name and dof name
    std::string constructBCName(const std::string ns, const std::string dof,
        const std::string condition) const;

    //! Local utility function to construct unique string from Nodeset/Sideset name and dof name
    std::string constructTimeDepBCName(const std::string ns, const std::string dof,
        const std::string condition) const;

  };

// Now, for the explicit template function declarations

/*
  template<> Teuchos::RCP<const Teuchos::ParameterList> BCUtils<DirichletTraits>::getValidBCParameters(
      const std::vector<std::string>& nodeSetIDs, const std::vector<std::string>& bcNames) const{};

  template<> std::string BCUtils<DirichletTraits>::constructBCName(const std::string ns, const std::string dof) const;

  template<> Teuchos::RCP<const Teuchos::ParameterList> BCUtils<NeumannTraits>::getValidBCParameters(
      const std::vector<std::string>& sideSetIDs, const std::vector<std::string>& bcNames) const;
  template<> std::string BCUtils<NeumannTraits>::constructBCName(const std::string ns, 
      const std::string condition) const;
*/

}

// Define macro for explicit template instantiation
#define BCUTILS_INSTANTIATE_TEMPLATE_CLASS_DIRICHLET(name) \
  template class name<Albany::DirichletTraits>;
#define BCUTILS_INSTANTIATE_TEMPLATE_CLASS_NEUMANN(name) \
  template class name<Albany::NeumannTraits>;

#define BCUTILS_INSTANTIATE_TEMPLATE_CLASS(name)		 \
  BCUTILS_INSTANTIATE_TEMPLATE_CLASS_DIRICHLET(name)	 \
  BCUTILS_INSTANTIATE_TEMPLATE_CLASS_NEUMANN(name)	

#endif 
