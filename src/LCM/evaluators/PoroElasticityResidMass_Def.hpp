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


#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include "Intrepid_FunctionSpaceTools.hpp"

namespace LCM {

  //**********************************************************************
  template<typename EvalT, typename Traits>
  PoroElasticityResidMass<EvalT, Traits>::
  PoroElasticityResidMass(const Teuchos::ParameterList& p) :
    wBF         (p.get<std::string>                   ("Weighted BF Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("Node QP Scalar Data Layout") ),
    porePressure (p.get<std::string>                   ("QP Variable Name"),
		  p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
    Tdot        (p.get<std::string>                   ("QP Time Derivative Variable Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
    ThermalCond (p.get<std::string>                   ("Thermal Conductivity Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
    kcPermeability (p.get<std::string>            ("Kozeny-Carman Permeability Name"),
		    p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
    porosity (p.get<std::string>                   ("Porosity Name"),
	      p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
    biotCoefficient (p.get<std::string>           ("Biot Coefficient Name"),
		     p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
    biotModulus (p.get<std::string>                   ("Biot Modulus Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
    wGradBF     (p.get<std::string>                   ("Weighted Gradient BF Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("Node QP Vector Data Layout") ),
    TGrad       (p.get<std::string>                   ("Gradient QP Variable Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout") ),
    Source      (p.get<std::string>                   ("Source Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
    strain      (p.get<std::string>                   ("Strain Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") ),
    TResidual   (p.get<std::string>                   ("Residual Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("Node Scalar Data Layout") ),
    haveSource  (p.get<bool>("Have Source")),
    haveConvection(false),
    haveAbsorption  (p.get<bool>("Have Absorption")),
    haverhoCp(false)
  {
    if (p.isType<bool>("Disable Transient"))
      enableTransient = !p.get<bool>("Disable Transient");
    else enableTransient = true;

    this->addDependentField(wBF);
    this->addDependentField(porePressure);
    this->addDependentField(ThermalCond);
    this->addDependentField(kcPermeability);
    this->addDependentField(porosity);
    this->addDependentField(biotCoefficient);
    this->addDependentField(biotModulus);
    if (enableTransient) this->addDependentField(Tdot);
    this->addDependentField(TGrad);
    this->addDependentField(wGradBF);
    if (haveSource) this->addDependentField(Source);
    if (haveAbsorption) {
      Absorption = PHX::MDField<ScalarT,Cell,QuadPoint>(
							p.get<std::string>("Absorption Name"),
							p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout"));
      this->addDependentField(Absorption);
    }



    this->addDependentField(strain);
    this->addEvaluatedField(TResidual);

    Teuchos::RCP<PHX::DataLayout> vector_dl =
      p.get< Teuchos::RCP<PHX::DataLayout> >("Node QP Vector Data Layout");
    std::vector<PHX::DataLayout::size_type> dims;
    vector_dl->dimensions(dims);

    // Get data from previous converged time step
    strainName = p.get<std::string>("Strain Name")+"_old";
    porosityName = p.get<std::string>("Porosity Name")+"_old";
    porePressureName = p.get<std::string>("QP Variable Name")+"_old";
    //this->addEvaluatedField(strain);


    worksetSize = dims[0];
    numNodes = dims[1];
    numQPs  = dims[2];
    numDims = dims[3];



    // Allocate workspace
    flux.resize(dims[0], numQPs, numDims);

    if (haveAbsorption)  aterm.resize(dims[0], numQPs);

    convectionVels = Teuchos::getArrayFromStringParameter<double> (p,
								   "Convection Velocity", numDims, false);
    if (p.isType<std::string>("Convection Velocity")) {
      convectionVels = Teuchos::getArrayFromStringParameter<double> (p,
								     "Convection Velocity", numDims, false);
    }
    if (convectionVels.size()>0) {
      haveConvection = true;
      if (p.isType<bool>("Have Rho Cp"))
	haverhoCp = p.get<bool>("Have Rho Cp");
      if (haverhoCp) {
	PHX::MDField<ScalarT,Cell,QuadPoint> tmp(p.get<string>("Rho Cp Name"),
						 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout"));
	rhoCp = tmp;
	this->addDependentField(rhoCp);
      }
    }

    this->setName("PoroElasticityResidMass"+PHX::TypeString<EvalT>::value);

  }

  //**********************************************************************
  template<typename EvalT, typename Traits>
  void PoroElasticityResidMass<EvalT, Traits>::
  postRegistrationSetup(typename Traits::SetupData d,
			PHX::FieldManager<Traits>& fm)
  {
    this->utils.setFieldData(wBF,fm);
    this->utils.setFieldData(porePressure,fm);
    this->utils.setFieldData(ThermalCond,fm);
    this->utils.setFieldData(kcPermeability,fm);
    this->utils.setFieldData(porosity,fm);
    this->utils.setFieldData(biotCoefficient,fm);
    this->utils.setFieldData(biotModulus,fm);
    this->utils.setFieldData(TGrad,fm);
    this->utils.setFieldData(wGradBF,fm);
    if (haveSource)  this->utils.setFieldData(Source,fm);
    if (enableTransient) this->utils.setFieldData(Tdot,fm);
    if (haveAbsorption)  this->utils.setFieldData(Absorption,fm);
    if (haveConvection && haverhoCp)  this->utils.setFieldData(rhoCp,fm);
    this->utils.setFieldData(strain,fm);
    this->utils.setFieldData(TResidual,fm);
  }

//**********************************************************************
template<typename EvalT, typename Traits>
void PoroElasticityResidMass<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  typedef Intrepid::FunctionSpaceTools FST;


  Albany::MDArray strainold = (*workset.stateArrayPtr)[strainName];
  Albany::MDArray porosityold = (*workset.stateArrayPtr)[porosityName];
  Albany::MDArray porePressureold = (*workset.stateArrayPtr)[porePressureName];

  // Cozeny-Carman relation added. I keep the thermal conductivity for future use. -S Sun


 // FST::integrate<ScalarT>(TResidual, porosity, wBF, Intrepid::COMP_CPP, true); // "true" sums into
//  FST::integrate<ScalarT>(TResidual, Tdot, wBF, Intrepid::COMP_CPP, true); // "true" sums into
//

  // Pore-fluid diffusion coupling.
  for (std::size_t cell=0; cell < workset.numCells; ++cell) {

       for (std::size_t node=0; node < numNodes; ++node) {
    	   TResidual(cell,node)=0.0;
           for (std::size_t qp=0; qp < numQPs; ++qp) {

// Transient partiall saturated flow (work in progress)
//              TResidual(cell,node) = -biotCoefficient(cell, qp)*(  (strain(cell,qp,0,0) + strain(cell,qp,1,1) +
//               		                    strain(cell,qp,2,2))
 //              		                 -(strainold(cell,qp,0,0) + strainold(cell,qp,1,1) +  strainold(cell,qp,2,2))
//                                      ) *wBF(cell, node, qp)/workset.delta_time ; // Div u solid skeleton constraint


//              TResidual(cell,node) += -biotCoefficient(cell, qp)*(
//            		                   porosity(cell, qp) - porosityold(cell, qp)
//                                        ) *wBF(cell, node, qp)/workset.delta_time  ; // Div u solid skeleton constraint
               TResidual(cell,node) +=  -(Tdot(cell, qp))
            		                    /biotModulus(cell, qp)*
             		                     wBF(cell, node, qp); // 1/Mp pore pressure constraint


// Undrained Condition only (not time depdendent)

//        	                 TResidual(cell,node) += -biotCoefficient(cell, qp)*(
//        	                		 strain(cell,qp,0,0) + strain(cell,qp,1,1) +
 //       	                		  strain(cell,qp,2,2)) *wBF(cell, node, qp)  ; // Div u solid skeleton constraint
 //       	                 TResidual(cell,node) +=  -(porePressure(cell, qp))
 //       	               		                    /biotModulus(cell, qp)*
 //       	                		                     wBF(cell, node, qp); // 1/Mp pore pressure constraint



   } } }


  // Pore-Fluid Diffusion Term
   FST::scalarMultiplyDataData<ScalarT> (flux, kcPermeability, TGrad); // flux_i = k I_ij p_j

    FST::integrate<ScalarT>(TResidual, flux, wGradBF, Intrepid::COMP_CPP, true); // "true" sums into









}

//**********************************************************************
}


