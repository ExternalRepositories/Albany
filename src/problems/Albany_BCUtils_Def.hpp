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

#include "Albany_BCUtils.hpp"

// Dirichlet specialization

//template<>
Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > 
Albany::BCUtils<Albany::DirichletTraits>::constructBCEvaluators(
      const std::vector<std::string>& nodeSetIDs,
      const std::vector<std::string>& bcNames,
      Teuchos::RCP<Teuchos::ParameterList> params,
      Teuchos::RCP<ParamLib> paramLib)
{
   using Teuchos::RCP;
   using Teuchos::rcp;
   using Teuchos::ParameterList;
   using PHX::DataLayout;
   using PHX::MDALayout;
   using std::vector;
   using std::string;

   using PHAL::AlbanyTraits;

   if(!params->isSublist(traits_type::bcParamsPl)){ // If the BC sublist is not in the input file, 
      // but we are inside this function, this means that
      // node sets are contained in the Exodus file but are not defined in the problem statement.This is OK, we
      // just don't do anything

      return Teuchos::null;

    }

   Teuchos::ParameterList BCparams = params->sublist(traits_type::bcParamsPl);
   BCparams.validateParameters(*(getValidBCParameters(nodeSetIDs, bcNames)),0);

   std::map<string, RCP<ParameterList> > evaluators_to_build;
   RCP<DataLayout> dummy = rcp(new MDALayout<Dummy>(0));
   vector<string> bcs;

   // Check for all possible standard BCs (every dof on every nodeset) to see which is set
   for (std::size_t i=0; i<nodeSetIDs.size(); i++) {
     for (std::size_t j=0; j<bcNames.size(); j++) {
       std::string ss = constructBCName(nodeSetIDs[i],bcNames[j]);

       if (BCparams.isParameter(ss)) {
         RCP<ParameterList> p = rcp(new ParameterList);
         p->set<int>("Type", traits_type::type);

         p->set< RCP<DataLayout> >("Data Layout", dummy);
         p->set< string >  ("Dirichlet Name", ss);
         p->set< RealType >("Dirichlet Value", BCparams.get<double>(ss));
         p->set< string >  ("Node Set ID", nodeSetIDs[i]);
        // p->set< int >     ("Number of Equations", dirichletNames.size());
         p->set< int >     ("Equation Offset", j);

         p->set<RCP<ParamLib> >("Parameter Library", paramLib);

         std::stringstream ess; ess << "Evaluator for " << ss;
         evaluators_to_build[ess.str()] = p;

         bcs.push_back(ss);
       }
     }
   }

#ifdef ALBANY_LCM
   ///
   /// Time dependent BC specific
   ///
   for (std::size_t i=0; i<nodeSetIDs.size(); i++) 
   {
     for (std::size_t j=0; j<bcNames.size(); j++) 
     {
       std::string ss = constructTimeDepBCName(nodeSetIDs[i],bcNames[j]);
       
       if (BCparams.isSublist(ss)) 
       {
	 // grab the sublist 
         ParameterList& sub_list = BCparams.sublist(ss);
	 RCP<ParameterList> p = rcp(new ParameterList);
	 p->set<int>("Type", traits_type::typeTd);
	   
	 // Extract the time values into a vector
	 //vector<RealType> timeValues = sub_list.get<Teuchos::Array<RealType> >("Time Values").toVector();
	 //RCP< vector<RealType> > t_ptr = Teuchos::rcpFromRef(timeValues); 
	 //p->set< RCP< vector<RealType> > >("Time Values", t_ptr);
	 p->set< Teuchos::Array<RealType> >("Time Values", sub_list.get<Teuchos::Array<RealType> >("Time Values"));

	 //cout << "timeValues: " << timeValues[0] << " " << timeValues[1] << endl;

	 // Extract the BC values into a vector
	 //vector<RealType> BCValues = sub_list.get<Teuchos::Array<RealType> >("BC Values").toVector();
	 //RCP< vector<RealType> > b_ptr = Teuchos::rcpFromRef(BCValues); 
	 //p->set< RCP< vector<RealType> > >("BC Values", b_ptr);
	 //p->set< vector<RealType> >("BC Values", BCValues);
	 p->set< Teuchos::Array<RealType> >("BC Values", sub_list.get<Teuchos::Array<RealType> >("BC Values"));
         p->set< RCP<DataLayout> >("Data Layout", dummy);
	 p->set< string >  ("Dirichlet Name", ss);
         p->set< RealType >("Dirichlet Value", 0.0);
	 p->set< int >     ("Equation Offset", j);
	 p->set<RCP<ParamLib> >("Parameter Library", paramLib);
	 p->set< string >  ("Node Set ID", nodeSetIDs[i]);

         std::stringstream ess; ess << "Evaluator for " << ss;
         evaluators_to_build[ess.str()] = p;

	 bcs.push_back(ss);
       }
     }
   }

   ///
   /// Torsion BC specific
   ///
   for (std::size_t i=0; i<nodeSetIDs.size(); i++) 
   {
     std::string ss = constructBCName(nodeSetIDs[i],"twist");
     
     if (BCparams.isSublist(ss)) 
     {
       // grab the sublist
       ParameterList& sub_list = BCparams.sublist(ss);

       if (sub_list.get<string>("BC Function") == "Torsion" )
       {
	 RCP<ParameterList> p = rcp(new ParameterList);
	 p->set<int>("Type", traits_type::typeTo);

         p->set< RealType >("Theta Dot", sub_list.get< RealType >("Theta Dot"));
         p->set< RealType >("X0", sub_list.get< RealType >("X0"));
         p->set< RealType >("Y0", sub_list.get< RealType >("Y0"));

	 // Fill up ParameterList with things DirichletBase wants
	 p->set< RCP<DataLayout> >("Data Layout", dummy);
	 p->set< string >  ("Dirichlet Name", ss);
         p->set< RealType >("Dirichlet Value", 0.0);
	 p->set< string >  ("Node Set ID", nodeSetIDs[i]);
         //p->set< int >     ("Number of Equations", dirichletNames.size());
	 p->set< int >     ("Equation Offset", 0);
	 
	 p->set<RCP<ParamLib> >("Parameter Library", paramLib);
	 std::stringstream ess; ess << "Evaluator for " << ss;
	 evaluators_to_build[ess.str()] = p;

	 bcs.push_back(ss);
       }
     }
   }

   ///
   /// Kfield BC specific
   ///
   for (std::size_t i=0; i<nodeSetIDs.size(); i++) 
   {
     std::string ss = constructBCName(nodeSetIDs[i],"K");
     
     if (BCparams.isSublist(ss)) 
     {
       // grab the sublist
       ParameterList& sub_list = BCparams.sublist(ss);

       if (sub_list.get<string>("BC Function") == "Kfield" )
       {
	 RCP<ParameterList> p = rcp(new ParameterList);
	 p->set<int>("Type", traits_type::typeKf);

	 p->set< Teuchos::Array<RealType> >("Time Values", sub_list.get<Teuchos::Array<RealType> >("Time Values"));
	 p->set< Teuchos::Array<RealType> >("KI Values", sub_list.get<Teuchos::Array<RealType> >("KI Values"));
	 p->set< Teuchos::Array<RealType> >("KII Values", sub_list.get<Teuchos::Array<RealType> >("KII Values"));

	 // This BC needs a shear modulus and poissons ratio defined
	 TEUCHOS_TEST_FOR_EXCEPTION(!params->isSublist("Shear Modulus"), 
				    Teuchos::Exceptions::InvalidParameter, 
				    "This BC needs a Shear Modulus");
	 ParameterList& shmd_list = params->sublist("Shear Modulus");
	 TEUCHOS_TEST_FOR_EXCEPTION(!(shmd_list.get("Shear Modulus Type","") == "Constant"), 
				    Teuchos::Exceptions::InvalidParameter,
				    "Invalid Shear Modulus type");
	 p->set< RealType >("Shear Modulus", shmd_list.get("Value", 1.0));

	 TEUCHOS_TEST_FOR_EXCEPTION(!params->isSublist("Poissons Ratio"), 
				    Teuchos::Exceptions::InvalidParameter, 
				    "This BC needs a Poissons Ratio");
	 ParameterList& pr_list = params->sublist("Poissons Ratio");
	 TEUCHOS_TEST_FOR_EXCEPTION(!(pr_list.get("Poissons Ratio Type","") == "Constant"), 
				    Teuchos::Exceptions::InvalidParameter,
				    "Invalid Poissons Ratio type");
	 p->set< RealType >("Poissons Ratio", pr_list.get("Value", 1.0));


//	 p->set< Teuchos::Array<RealType> >("BC Values", sub_list.get<Teuchos::Array<RealType> >("BC Values"));
//	 p->set< RCP<DataLayout> >("Data Layout", dummy);

	 // Extract BC parameters
	 p->set< string >("Kfield KI Name", "Kfield KI");
	 p->set< string >("Kfield KII Name", "Kfield KII");
	 p->set< RealType >("KI Value", sub_list.get<double>("Kfield KI"));
	 p->set< RealType >("KII Value", sub_list.get<double>("Kfield KII"));

	 // Fill up ParameterList with things DirichletBase wants
	 p->set< RCP<DataLayout> >("Data Layout", dummy);
	 p->set< string >  ("Dirichlet Name", ss);
         p->set< RealType >("Dirichlet Value", 0.0);
	 p->set< string >  ("Node Set ID", nodeSetIDs[i]);
         //p->set< int >     ("Number of Equations", dirichletNames.size());
	 p->set< int >     ("Equation Offset", 0);
	 
	 p->set<RCP<ParamLib> >("Parameter Library", paramLib);
	 std::stringstream ess; ess << "Evaluator for " << ss;
	 evaluators_to_build[ess.str()] = p;

	 bcs.push_back(ss);
       }
     }
   }
#endif

   string allBC="Evaluator for all Dirichlet BCs";
   {
      RCP<ParameterList> p = rcp(new ParameterList);
      p->set<int>("Type", traits_type::typeDa);

      p->set<vector<string>* >("DBC Names", &bcs);
      p->set< RCP<DataLayout> >("Data Layout", dummy);
      p->set<string>("DBC Aggregator Name", allBC);
      evaluators_to_build[allBC] = p;
   }

   // Build Field Evaluators for each evaluation type
   PHX::EvaluatorFactory<AlbanyTraits,PHAL::DirichletFactoryTraits<AlbanyTraits> > factory;
   RCP< vector< RCP<PHX::Evaluator_TemplateManager<AlbanyTraits> > > > evaluators;
   evaluators = factory.buildEvaluators(evaluators_to_build);

   // Create a DirichletFieldManager
   Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > dfm
     = Teuchos::rcp(new PHX::FieldManager<AlbanyTraits>);

   // Register all Evaluators
   PHX::registerEvaluators(evaluators, *dfm);

   PHX::Tag<AlbanyTraits::Residual::ScalarT> res_tag0(allBC, dummy);
   dfm->requireField<AlbanyTraits::Residual>(res_tag0);

   PHX::Tag<AlbanyTraits::Jacobian::ScalarT> jac_tag0(allBC, dummy);
   dfm->requireField<AlbanyTraits::Jacobian>(jac_tag0);

   PHX::Tag<AlbanyTraits::Tangent::ScalarT> tan_tag0(allBC, dummy);
   dfm->requireField<AlbanyTraits::Tangent>(tan_tag0);

   PHX::Tag<AlbanyTraits::SGResidual::ScalarT> sgres_tag0(allBC, dummy);
   dfm->requireField<AlbanyTraits::SGResidual>(sgres_tag0);

   PHX::Tag<AlbanyTraits::SGJacobian::ScalarT> sgjac_tag0(allBC, dummy);
   dfm->requireField<AlbanyTraits::SGJacobian>(sgjac_tag0);

   PHX::Tag<AlbanyTraits::SGTangent::ScalarT> sgtan_tag0(allBC, dummy);
   dfm->requireField<AlbanyTraits::SGTangent>(sgtan_tag0);

   PHX::Tag<AlbanyTraits::MPResidual::ScalarT> mpres_tag0(allBC, dummy);
   dfm->requireField<AlbanyTraits::MPResidual>(mpres_tag0);

   PHX::Tag<AlbanyTraits::MPJacobian::ScalarT> mpjac_tag0(allBC, dummy);
   dfm->requireField<AlbanyTraits::MPJacobian>(mpjac_tag0);

   PHX::Tag<AlbanyTraits::MPTangent::ScalarT> mptan_tag0(allBC, dummy);
   dfm->requireField<AlbanyTraits::MPTangent>(mptan_tag0);

   return dfm;
}

//template<>
Teuchos::RCP<const Teuchos::ParameterList>
Albany::BCUtils<Albany::DirichletTraits>::getValidBCParameters(
  const std::vector<std::string>& nodeSetIDs,
  const std::vector<std::string>& bcNames) const
{
  Teuchos::RCP<Teuchos::ParameterList>validPL =
     Teuchos::rcp(new Teuchos::ParameterList("Valid Dirichlet BC List"));;

  for (std::size_t i=0; i<nodeSetIDs.size(); i++) {
    for (std::size_t j=0; j<bcNames.size(); j++) {
      std::string ss = constructBCName(nodeSetIDs[i],bcNames[j]);
      std::string tt = constructTimeDepBCName(nodeSetIDs[i],bcNames[j]);
      validPL->set<double>(ss, 0.0, "Value of BC corresponding to nodeSetID and dofName");
      validPL->sublist(tt, false, "SubList of BC corresponding to nodeSetID and dofName");
    }
  }
  
  for (std::size_t i=0; i<nodeSetIDs.size(); i++) 
  {
    std::string ss = constructBCName(nodeSetIDs[i],"K");
    std::string tt = constructBCName(nodeSetIDs[i],"twist");
    validPL->sublist(ss, false, "");
    validPL->sublist(tt, false, "");
  }

  return validPL;
}

//template<>
std::string
Albany::BCUtils<Albany::DirichletTraits>::constructBCName(const std::string ns, const std::string dof) const
{

  std::stringstream ss; ss << "DBC on NS " << ns << " for DOF " << dof;

  return ss.str();
}

//template<typename BCTraits>
std::string
Albany::BCUtils<Albany::DirichletTraits>::constructTimeDepBCName(const std::string ns, const std::string dof) const
{
  std::stringstream ss; ss << "Time Dependent " << constructBCName(ns, dof);
  return ss.str();
}

// Neumann specialization

//template<>
bool
Albany::BCUtils<Albany::NeumannTraits>::haveNeumann(const Teuchos::RCP<Teuchos::ParameterList>& params) const {

      // If the NBC sublist is not in the input file, 
      // side sets might be contained in the Exodus file but are not defined in the problem statement.This is OK, we
      // just return 

   return params->isSublist(traits_type::bcParamsPl);
}


//template<>
Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > 
Albany::BCUtils<Albany::NeumannTraits>::constructBCEvaluators(
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
      const Teuchos::RCP<QCAD::MaterialDatabase>& materialDB,
      bool isTensorField)
{
   using Teuchos::RCP;
   using Teuchos::rcp;
   using Teuchos::ParameterList;
   using PHX::DataLayout;
   using PHX::MDALayout;
   using std::vector;
   using std::string;

   using PHAL::AlbanyTraits;

   numDim = meshSpecs->numDim;

   // Drop into the "Neumann BCs" sublist
   Teuchos::ParameterList BCparams = params->sublist(traits_type::bcParamsPl);
   BCparams.validateParameters(*(getValidBCParameters(meshSpecs->ssNames, bcNames, conditions)),0);

   std::map<string, RCP<ParameterList> > evaluators_to_build;
//   RCP<DataLayout> dummy = rcp(new MDALayout<Dummy>(0));
   vector<string> bcs;

   // Check for all possible standard BCs (every dof on every sideset) to see which is set
   for (std::size_t i=0; i<meshSpecs->ssNames.size(); i++) {
     for (std::size_t j=0; j<bcNames.size(); j++) {
       for (std::size_t k=0; k<conditions.size(); k++) {

        // construct input.xml string like:
        // "NBC on SS sidelist_12 for DOF T set dudn"
        //  or
        // "NBC on SS sidelist_12 for DOF T set (dudx, dudy)"
        // or
        // "NBC on SS surface_1 for DOF all set P"

         std::string ss = constructBCName(meshSpecs->ssNames[i], bcNames[j], conditions[k]);

         // Have a match of the line in input.xml

         if (BCparams.isParameter(ss)) {

//           std::cout << "Constructing NBC: " << ss << std::endl;

           TEUCHOS_TEST_FOR_EXCEPTION(BCparams.isType<string>(ss), std::logic_error,
               "NBC array information in XML file must be of type Array(double)\n");

           // These are read in the Albany::Neumann constructor (PHAL_Neumann_Def.hpp)

           RCP<ParameterList> p = rcp(new ParameterList);

           p->set<int>                            ("Type", traits_type::type);
  
           p->set<RCP<ParamLib> >                 ("Parameter Library", paramLib);
  
           p->set<string>                         ("Side Set ID", meshSpecs->ssNames[i]);
           p->set<Teuchos::Array< int > >         ("Equation Offset", offsets[j]);
           p->set< RCP<Albany::Layouts> >         ("Base Data Layout", dl);
           p->set< RCP<MeshSpecsStruct> >         ("Mesh Specs Struct", meshSpecs);

           p->set<string>                         ("Coordinate Vector Name", "Coord Vec");

           if(conditions[k] == "robin") {
             p->set<string>  ("DOF Name", dof_names[j]);
	     p->set<bool> ("Vector Field", isVectorField);
	     if (isVectorField) p->set< RCP<DataLayout> >("DOF Data Layout", dl->node_vector);
	     else               p->set< RCP<DataLayout> >("DOF Data Layout", dl->node_scalar);
           }

           // Pass the input file line
           p->set< string >                       ("Neumann Input String", ss);
           p->set< Teuchos::Array<double> >       ("Neumann Input Value", BCparams.get<Teuchos::Array<double> >(ss));
           p->set< string >                       ("Neumann Input Conditions", conditions[k]);

           // If we are doing a Neumann internal boundary with a "scaled jump" (includes "robin" too)
           // The material DB database needs to be passed to the BC object

           if(conditions[k] == "scaled jump" || conditions[k] == "robin"){ 

              TEUCHOS_TEST_FOR_EXCEPTION(materialDB == Teuchos::null,
                Teuchos::Exceptions::InvalidParameter, 
                "This BC needs a material database specified");

              p->set< RCP<QCAD::MaterialDatabase> >("MaterialDB", materialDB);


           }


    // Inputs: X, Y at nodes, Cubature, and Basis
    //p->set<string>("Node Variable Name", "Neumann");

           std::stringstream ess; ess << "Evaluator for " << ss;
           evaluators_to_build[ess.str()] = p;
  
           bcs.push_back(ss);
         }
       }
     }
   }

// Build evaluator for Gather Coordinate Vector

   string NeuGCV="Evaluator for Gather Coordinate Vector";
   {
      RCP<ParameterList> p = rcp(new ParameterList);
      p->set<int>("Type", traits_type::typeGCV);

      // Input: Periodic BC flag
      p->set<bool>("Periodic BC", false);
 
      // Output:: Coordindate Vector at vertices
      p->set< RCP<DataLayout> >  ("Coordinate Data Layout",  dl->vertices_vector);
      p->set< string >("Coordinate Vector Name", "Coord Vec");
 
      evaluators_to_build[NeuGCV] = p;
   }

// Build evaluator for Gather Solution

   string NeuGS="Evaluator for Gather Solution";
   {
     RCP<ParameterList> p = rcp(new ParameterList());
     p->set<int>("Type", traits_type::typeGS);

     p->set< Teuchos::ArrayRCP<std::string> >("Solution Names", dof_names);

     p->set<bool>("Vector Field", isVectorField);
     p->set<bool>("Tensor Field", isTensorField);
     if      (isTensorField) p->set< RCP<DataLayout> >("Data Layout", dl->node_tensor);
     else if (isVectorField) p->set< RCP<DataLayout> >("Data Layout", dl->node_vector);
     else                    p->set< RCP<DataLayout> >("Data Layout", dl->node_scalar);

     p->set<int>("Offset of First DOF", offsetToFirstDOF);
     p->set<bool>("Disable Transient", true);

     evaluators_to_build[NeuGS] = p;
   }


// Build evaluator that causes the evaluation of all the NBCs

   string allBC="Evaluator for all Neumann BCs";
   {
      RCP<ParameterList> p = rcp(new ParameterList);
      p->set<int>("Type", traits_type::typeNa);

      p->set<vector<string>* >("NBC Names", &bcs);
//      p->set< RCP<DataLayout> >("Data Layout", dummy);
      p->set< RCP<DataLayout> >("Data Layout", dl->dummy);
      p->set<string>("NBC Aggregator Name", allBC);
      evaluators_to_build[allBC] = p;
   }

   // Build Field Evaluators for each evaluation type
   PHX::EvaluatorFactory<AlbanyTraits, PHAL::NeumannFactoryTraits<AlbanyTraits> > factory;
   RCP< vector< RCP<PHX::Evaluator_TemplateManager<AlbanyTraits> > > > evaluators;
   evaluators = factory.buildEvaluators(evaluators_to_build);

   // Create a NeumannFieldManager
   Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits> > nfm
     = Teuchos::rcp(new PHX::FieldManager<AlbanyTraits>);

   // Register all Evaluators
   PHX::registerEvaluators(evaluators, *nfm);

   PHX::Tag<AlbanyTraits::Residual::ScalarT> res_tag0(allBC, dl->dummy);
   nfm->requireField<AlbanyTraits::Residual>(res_tag0);

   PHX::Tag<AlbanyTraits::Jacobian::ScalarT> jac_tag0(allBC, dl->dummy);
   nfm->requireField<AlbanyTraits::Jacobian>(jac_tag0);

   PHX::Tag<AlbanyTraits::Tangent::ScalarT> tan_tag0(allBC, dl->dummy);
   nfm->requireField<AlbanyTraits::Tangent>(tan_tag0);

   PHX::Tag<AlbanyTraits::SGResidual::ScalarT> sgres_tag0(allBC, dl->dummy);
   nfm->requireField<AlbanyTraits::SGResidual>(sgres_tag0);

   PHX::Tag<AlbanyTraits::SGJacobian::ScalarT> sgjac_tag0(allBC, dl->dummy);
   nfm->requireField<AlbanyTraits::SGJacobian>(sgjac_tag0);

   PHX::Tag<AlbanyTraits::SGTangent::ScalarT> sgtan_tag0(allBC, dl->dummy);
   nfm->requireField<AlbanyTraits::SGTangent>(sgtan_tag0);

   PHX::Tag<AlbanyTraits::MPResidual::ScalarT> mpres_tag0(allBC, dl->dummy);
   nfm->requireField<AlbanyTraits::MPResidual>(mpres_tag0);

   PHX::Tag<AlbanyTraits::MPJacobian::ScalarT> mpjac_tag0(allBC, dl->dummy);
   nfm->requireField<AlbanyTraits::MPJacobian>(mpjac_tag0);

   PHX::Tag<AlbanyTraits::MPTangent::ScalarT> mptan_tag0(allBC, dl->dummy);
   nfm->requireField<AlbanyTraits::MPTangent>(mptan_tag0);

   return nfm;
}

//template<>
Teuchos::RCP<const Teuchos::ParameterList>
Albany::BCUtils<Albany::NeumannTraits>::getValidBCParameters(
  const std::vector<std::string>& sideSetIDs,
  const std::vector<std::string>& bcNames,
  const std::vector<std::string>& conditions) const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
     Teuchos::rcp(new Teuchos::ParameterList("Valid Neumann BC List"));;

  for (std::size_t i=0; i<sideSetIDs.size(); i++) { // loop over all side sets in the mesh
    for (std::size_t j=0; j<bcNames.size(); j++) { // loop over all possible types of condition
      for (std::size_t k=0; k<conditions.size(); k++) { // loop over all possible types of condition

        std::string ss = constructBCName(sideSetIDs[i], bcNames[j], conditions[k]);
        std::string tt = constructTimeDepBCName(sideSetIDs[i], bcNames[j], conditions[k]);

        if(numDim == 2)
          validPL->set<Teuchos::Array<double> >(ss, Teuchos::tuple<double>(0.0, 0.0),
            "Value of BC corresponding to sideSetID and boundary condition");
        else
          validPL->set<Teuchos::Array<double> >(ss, Teuchos::tuple<double>(0.0, 0.0, 0.0),
            "Value of BC corresponding to sideSetID and boundary condition");

        validPL->sublist(tt, false, "SubList of BC corresponding to sideSetID and boundary condition");

      }
    }
  }
  
  return validPL;

}

//template<>
std::string
Albany::BCUtils<Albany::NeumannTraits>::constructBCName(const std::string ns, const std::string dof,
					 const std::string condition) const
{
  std::stringstream ss; ss << "NBC on SS " << ns << " for DOF " << dof << " set " << condition;
  return ss.str();
}

//template<typename BCTraits>
std::string
Albany::BCUtils<Albany::NeumannTraits>::constructTimeDepBCName(const std::string ns,
						const std::string dof, const std::string condition) const
{
  std::stringstream ss; ss << "Time Dependent " << constructBCName(ns, dof, condition);
  return ss.str();
}
