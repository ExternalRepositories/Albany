//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "PHAL_Utilities.hpp"
#include "Intrepid2_FunctionSpaceTools.hpp"
#include "Teuchos_CommHelpers.hpp"
#include "Phalanx.hpp"

namespace AMP {
 template<typename EvalT, typename Traits>
 Energy<EvalT, Traits>::
 Energy(Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl) :
 weighted_measure	("Weights", dl->qp_scalar),
 T_			("Temperature Name", dl->qp_scalar),
 time_			("Time Name", dl->workset_scalar),
 phi_			("Phi Name", dl->qp_scalar),
 rho_Cp_		("Rho Cp Name", dl->qp_scalar)

{
 // get parameters from problem
 Teuchos::RCP<Teuchos::ParameterList> pFromProb = p.get<Teuchos::RCP<Teuchos::ParameterList>>("Parameters From Problem");
 // get properties
 Cl_ = pFromProb->get<RealType>("Volumetric Heat Capacity Liquid Value");
 L_  = pFromProb->get<RealType>("Latent Heat Value"); 
 Tm_ = pFromProb->get<RealType>("Melting Temperature Value"); 
 

 this->addDependentField(weighted_measure);
 this->addDependentField(T_);
 this->addDependentField(phi_);
 this->addDependentField(rho_Cp_);
 this->addDependentField(time_);

 // number of quad points per cell and dimension of space 
 std::vector<PHX::Device::size_type> dims;
 Teuchos::RCP<PHX::DataLayout> scalar_dl = dl->qp_scalar;
 scalar_dl->dimensions(dims);
 workset_size_ = dims[0];
 num_qps_ = dims[1];


 Teuchos::ParameterList* cond_list = p.get<Teuchos::ParameterList*>("Parameter List");
 Teuchos::RCP<const Teuchos::ParameterList> reflist = this->getValidResponseParameters();
 cond_list->validateParameters(*reflist, 0);

 this->setName("Energy" + PHX::typeAsString<EvalT>());      

 using PHX::MDALayout;
 //Setup scatter evaluater
 p.set("Stand-alone Evaluator", false);
 std::string local_response_name = "Local Response Energy";
 std::string global_response_name = "Global Response Energy";
        
 int responseSize = 2;

 Teuchos::RCP<PHX::DataLayout> local_response_layout = Teuchos::rcp(
 new MDALayout<Cell,Dim>(workset_size_, responseSize));
 PHX::Tag<ScalarT> local_response_tag(local_response_name, local_response_layout);
 p.set("Local Response Field Tag", local_response_tag);

 Teuchos::RCP<PHX::DataLayout> global_response_layout = Teuchos::rcp(new MDALayout<Dim>(responseSize));
 PHX::Tag<ScalarT> global_response_tag(global_response_name, global_response_layout);
 p.set("Global Response Field Tag", global_response_tag);
 PHAL::SeparableScatterScalarResponse<EvalT,Traits>::setup(p,dl);
		
}

    //**********************************************************************

    template<typename EvalT, typename Traits>
    void Energy<EvalT, Traits>::
    postRegistrationSetup(typename Traits::SetupData d,
            PHX::FieldManager<Traits>& fm) 
	{
        this->utils.setFieldData(weighted_measure, fm);
        this->utils.setFieldData(T_, fm);
        this->utils.setFieldData(time_, fm);
        this->utils.setFieldData(phi_, fm);
        this->utils.setFieldData(rho_Cp_, fm);
        //this->utils.setFieldData(energy_, fm);
		PHAL::SeparableScatterScalarResponse<EvalT,Traits>::postRegistrationSetup(d,fm);
    }

    //**********************************************************************
	template<typename EvalT, typename Traits>
	void Energy<EvalT, Traits>::
	preEvaluate(typename Traits::PreEvalData workset)
	{
		const int imax = this->global_response.size();
		PHAL::set(this->global_response, 0.0);
		// Do global initialization
		PHAL::SeparableScatterScalarResponse<EvalT,Traits>::preEvaluate(workset);
	}
	
	//**********************************************************************
	template<typename EvalT, typename Traits>
	void Energy<EvalT, Traits>::
	evaluateFields(typename Traits::EvalData workset)
	{
	/*
		// current time
		const RealType t = workset.current_time;
		if (t == 0.0)
		{
			// initializing energy values:
			ScalarT TotalEnergy=0.0;
        	for (std::size_t cell = 0; cell < workset.numCells; ++cell)
				for (std::size_t qp = 0; qp < num_qps_; ++qp)
					energy_(cell, qp) = 0.0;
				
			
		}
    */
	PHAL::set(this->local_response, 0.0);

	ScalarT volume;
	ScalarT energy;
    
	// temporal variables
    ScalarT phi; // phi = phi_(cell,qp)
    ScalarT Cs; // Cs = Rho_Cp_(cell,qp)
    ScalarT Cd; // Volumetric heat capacity of solid. For now same as powder.
    ScalarT p; // Variable used to compute p = phi^3 * (10-15*phi+6*phi^2)

    for (std::size_t cell = 0; cell < workset.numCells; ++cell)
    {
        for (std::size_t qp = 0; qp < num_qps_; ++qp)
        {
	    volume = weighted_measure(cell,qp);
            phi = phi_(cell, qp);
            Cs = rho_Cp_(cell, qp);
            Cd = Cs;
            std:: cout<<"phi="<<phi<<std::endl;
            std:: cout<<"Cs="<<Cs<<std::endl;
            std:: cout<<"Cd="<<Cd<<std::endl;

	    this->local_response(cell, 0) += volume;
	    this->global_response(0) += volume;
			
            // Compute Phase function, p
            p = phi * phi * phi * (10.0 - 15.0 * phi + 6.0 * phi * phi);
            // compute energy
            energy = (Cs * T_(cell, qp) +  p * (L_ + (Cl_ - Cs) * (T_(cell, qp) - Tm_)))*weighted_measure(cell,qp);
			
	    this->local_response(cell, 2) += energy;
	    this->global_response(2) += energy;
            std:: cout<<"volume="<<volume<<std::endl;
            std:: cout<<"energy="<<energy<<std::endl;
        }
    }
	
	// Do any local-scattering necessary
	PHAL::SeparableScatterScalarResponse<EvalT,Traits>::evaluateFields(workset);
 }

    //**********************************************************************

	template<typename EvalT, typename Traits>
	void Energy<EvalT, Traits>::
	postEvaluate(typename Traits::PostEvalData workset)
	{
		PHAL::reduceAll(*workset.comm, Teuchos::REDUCE_SUM, this->global_response);

		// Do global scattering
		PHAL::SeparableScatterScalarResponse<EvalT,Traits>::postEvaluate(workset);

		PHAL::MDFieldIterator<ScalarT> gr(this->global_response);
		std::cout << "Total Volume is " << *gr << std::endl;
		++gr;
		std::cout << "Total Energy is " << *gr << std::endl;  
	}
    //**********************************************************************

    template<typename EvalT, typename Traits>
    Teuchos::RCP<const Teuchos::ParameterList>
    Energy<EvalT, Traits>::
    getValidResponseParameters() const {
        Teuchos::RCP<Teuchos::ParameterList> valid_pl =
                rcp(new Teuchos::ParameterList("Valid Energy Params"));
		Teuchos::RCP<const Teuchos::ParameterList> baseValidPL =
			PHAL::SeparableScatterScalarResponse<EvalT,Traits>::getValidResponseParameters();
		valid_pl->setParameters(*baseValidPL);
/*
        valid_pl->set<double>("Volumetric Heat Capacity Liquid Value", 5.95e6);
        valid_pl->set<double>("Latent Heat Value", 2.18e9);
*/
        return valid_pl;
    }

    //**********************************************************************

}
