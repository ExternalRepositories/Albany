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
#include "Intrepid_RealSpaceTools.hpp"

namespace LCM {

  //**********************************************************************
  template<typename EvalT, typename Traits>
  TLPoroPlasticityResidMass<EvalT, Traits>::
  TLPoroPlasticityResidMass(const Teuchos::ParameterList& p) :
    wBF         (p.get<std::string>                   ("Weighted BF Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("Node QP Scalar Data Layout") ),
    porePressure (p.get<std::string>                   ("QP Pore Pressure Name"),
		  p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
    Tdot        (p.get<std::string>                   ("QP Time Derivative Variable Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
	stabParameter        (p.get<std::string>                   ("Material Property Name"),
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
	coordVec      (p.get<std::string>                   ("Coordinate Vector Name"),
				 p.get<Teuchos::RCP<PHX::DataLayout> >("Coordinate Data Layout") ),
    cubature      (p.get<Teuchos::RCP <Intrepid::Cubature<RealType> > >("Cubature")),
	cellType      (p.get<Teuchos::RCP <shards::CellTopology> > ("Cell Type")),
	weights       (p.get<std::string>                   ("Weights Name"),
		         p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
	deltaTime (p.get<std::string>("Delta Time Name"),
		       p.get<Teuchos::RCP<PHX::DataLayout> >("Workset Scalar Data Layout")),
	J           (p.get<std::string>                   ("DetDefGrad Name"),
		       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
	defgrad     (p.get<std::string>                   ("DefGrad Name"),
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

    this->addDependentField(stabParameter);
    this->addDependentField(deltaTime);
    this->addDependentField(weights);
    this->addDependentField(coordVec);
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
    this->addDependentField(J);
    this->addDependentField(defgrad);
    this->addEvaluatedField(TResidual);

    Teuchos::RCP<PHX::DataLayout> vector_dl =
      p.get< Teuchos::RCP<PHX::DataLayout> >("Node QP Vector Data Layout");
    std::vector<PHX::DataLayout::size_type> dims;
    vector_dl->dimensions(dims);

    // Get data from previous converged time step
    strainName = p.get<std::string>("Strain Name")+"_old";
    porosityName = p.get<std::string>("Porosity Name")+"_old";
    porePressureName = p.get<std::string>("QP Pore Pressure Name")+"_old";
    JName =p.get<std::string>("DetDefGrad Name")+"_old";



    worksetSize = dims[0];
    numNodes = dims[1];
    numQPs  = dims[2];
    numDims = dims[3];

    // Works space FCs
    C.resize(worksetSize, numQPs, numDims, numDims);
    Cinv.resize(worksetSize, numQPs, numDims, numDims);
    F_inv.resize(worksetSize, numQPs, numDims, numDims);
    F_invT.resize(worksetSize, numQPs, numDims, numDims);
    JF_invT.resize(worksetSize, numQPs, numDims, numDims);
    KJF_invT.resize(worksetSize, numQPs, numDims, numDims);
    Kref.resize(worksetSize, numQPs, numDims, numDims);



    // Allocate workspace
    flux.resize(dims[0], numQPs, numDims);
    fluxdt.resize(dims[0], numQPs, numDims);
    pterm.resize(dims[0], numQPs);

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

    this->setName("TLPoroPlasticityResidMass"+PHX::TypeString<EvalT>::value);

  }

  //**********************************************************************
  template<typename EvalT, typename Traits>
  void TLPoroPlasticityResidMass<EvalT, Traits>::
  postRegistrationSetup(typename Traits::SetupData d,
			PHX::FieldManager<Traits>& fm)
  {
	this->utils.setFieldData(stabParameter,fm);
	this->utils.setFieldData(deltaTime,fm);
	this->utils.setFieldData(weights,fm);
    this->utils.setFieldData(coordVec,fm);
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
    this->utils.setFieldData(J,fm);
    this->utils.setFieldData(defgrad,fm);
    this->utils.setFieldData(TResidual,fm);
  }

//**********************************************************************
template<typename EvalT, typename Traits>
void TLPoroPlasticityResidMass<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  typedef Intrepid::FunctionSpaceTools FST;
  typedef Intrepid::RealSpaceTools<ScalarT> RST;


  Albany::MDArray strainold = (*workset.stateArrayPtr)[strainName];
  Albany::MDArray porosityold = (*workset.stateArrayPtr)[porosityName];
  Albany::MDArray porePressureold = (*workset.stateArrayPtr)[porePressureName];
  Albany::MDArray Jold = (*workset.stateArrayPtr)[JName];

  // Set Warning message
  if (porosityold(1,1) < 0 || porosity(1,1) < 0 ) {
	  cout << "negative porosity detected. Error! \n";
  }



  // Pore-fluid diffusion coupling.
  for (std::size_t cell=0; cell < workset.numCells; ++cell) {

	  for (std::size_t node=0; node < numNodes; ++node) {
		  TResidual(cell,node)=0.0;
		  for (std::size_t qp=0; qp < numQPs; ++qp) {

			      // set correction for 1st time step
			      if (J(cell,qp) == 0)
			    		  J(cell,qp) = 1.0;
			      if (Jold(cell,qp) == 0)
			      		  Jold(cell,qp) = 1.0;



 				  // Volumetric Constraint Term
 				  TResidual(cell,node) += -biotCoefficient(cell, qp)*(

 					 (J(cell,qp)-Jold(cell,qp))

 						  )
             		  *wBF(cell, node, qp)  ;

 				  // Pore-fluid Resistance Term
 				  TResidual(cell,node) +=  -(
 						 -(J(cell,qp)-Jold(cell,qp))*porePressure(cell,qp) +
 						 J(cell,qp)*(porePressure(cell,qp)-porePressureold(cell, qp) )
 						                  /
 						                		  (J(cell,qp)*J(cell,qp))
 						                				  )
             		                    		/biotModulus(cell, qp)*
             		                    		wBF(cell, node, qp);
			  }
		  }
	  }




  // Pore-Fluid Diffusion Term

   ScalarT dt = deltaTime(0);

   RST::inverse(F_inv, defgrad);
   RST::transpose(F_invT, F_inv);
   FST::scalarMultiplyDataData<ScalarT>(JF_invT, J, F_invT);
   FST::scalarMultiplyDataData<ScalarT>(KJF_invT, kcPermeability, JF_invT);
   FST::tensorMultiplyDataData<ScalarT>(Kref, F_inv, KJF_invT);

   FST::tensorMultiplyDataData<ScalarT> (flux, Kref, TGrad); // flux_i = k I_ij p_j

   for (std::size_t cell=0; cell < workset.numCells; ++cell){
      for (std::size_t qp=0; qp < numQPs; ++qp) {
    	  for (std::size_t dim=0; dim <numDims; ++dim){
    		  fluxdt(cell, qp, dim) = -flux(cell,qp,dim)*dt; // should replace the number with dt
    	  }
      }
  }


  FST::integrate<ScalarT>(TResidual, fluxdt, wGradBF, Intrepid::COMP_CPP, true); // "true" sums into

  //---------------------------------------------------------------------------//
  // Stabilization Term (only 2D and 3D problem need stabilizer)

// Penalty Term


  for (std::size_t cell=0; cell < workset.numCells; ++cell){

   porePbar = 0.0;
   vol = 0.0;
   for (std::size_t qp=0; qp < numQPs; ++qp) {
	porePbar += weights(cell,qp)*(
			-(J(cell,qp)-Jold(cell,qp))*porePressure(cell,qp)
			 						 +  J(cell,qp)*(porePressure(cell,qp)-porePressureold(cell, qp) )
			 						 / (J(cell,qp)*J(cell,qp))
			                      );
	vol  += weights(cell,qp);
   }
   porePbar /= vol;
   for (std::size_t qp=0; qp < numQPs; ++qp) {
   pterm(cell,qp) = porePbar;
        }

 }

  for (std::size_t cell=0; cell < workset.numCells; ++cell) {

	  for (std::size_t node=0; node < numNodes; ++node) {
		  for (std::size_t qp=0; qp < numQPs; ++qp) {
 				  TResidual(cell,node) -= (
 						 -(J(cell,qp)-Jold(cell,qp))*porePressure(cell,qp)
 						 +  J(cell,qp)*(porePressure(cell,qp)-porePressureold(cell, qp) )
 						 / (J(cell,qp)*J(cell,qp))
 						                               )
                    		                    		*stabParameter(cell, qp)/biotModulus(cell, qp)*
                    		                    		wBF(cell, node, qp);
 				  TResidual(cell,node) += pterm(cell,qp)*stabParameter(cell, qp)/biotModulus(cell, qp)*
                  		wBF(cell, node, qp);


//                  // Use biotModulus as pre-conditioner
// 				  TResidual(cell,node) = biotModulus(cell,qp)*TResidual(cell,node);


		  }
	  }
  }






}

//**********************************************************************
}


