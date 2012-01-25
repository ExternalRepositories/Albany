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


#include <fstream>
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Teuchos_CommHelpers.hpp"
#include "Phalanx.hpp"

template<typename EvalT, typename Traits>
QCAD::ResponseCenterOfMass<EvalT, Traits>::
ResponseCenterOfMass(Teuchos::ParameterList& p,
		     const Teuchos::RCP<Albany::Layouts>& dl) :
  coordVec("Coord Vec", dl->qp_vector),
  weights("Weights", dl->qp_scalar)
{
  // get and validate Response parameter list
  Teuchos::ParameterList* plist = 
    p.get<Teuchos::ParameterList*>("Parameter List");
  Teuchos::RCP<const Teuchos::ParameterList> reflist = 
    this->getValidResponseParameters();
  plist->validateParameters(*reflist,0);

  // number of quad points per cell and dimension of space
  Teuchos::RCP<PHX::DataLayout> scalar_dl = dl->qp_scalar;
  Teuchos::RCP<PHX::DataLayout> vector_dl = dl->qp_vector;
  
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  // User-specified parameters
  fieldName  = plist->get<std::string>("Field Name");
  opDomain     = plist->get<std::string>("Operation Domain", "box");

  if(opDomain == "box") {
    limitX = limitY = limitZ = false;

    if( plist->isParameter("x min") && plist->isParameter("x max") ) {
      limitX = true; TEUCHOS_TEST_FOR_EXCEPT(numDims <= 0);
      xmin = plist->get<double>("x min");
      xmax = plist->get<double>("x max");
    }
    if( plist->isParameter("y min") && plist->isParameter("y max") ) {
      limitY = true; TEUCHOS_TEST_FOR_EXCEPT(numDims <= 1);
      ymin = plist->get<double>("y min");
      ymax = plist->get<double>("y max");
    }
    if( plist->isParameter("z min") && plist->isParameter("z max") ) {
      limitZ = true; TEUCHOS_TEST_FOR_EXCEPT(numDims <= 2);
      zmin = plist->get<double>("z min");
      zmax = plist->get<double>("z max");
    }
  }
  else if(opDomain == "element block") {
    ebName = plist->get<string>("Element Block Name");
  }
  else TEUCHOS_TEST_FOR_EXCEPTION (true, Teuchos::Exceptions::InvalidParameter, std::endl 
             << "Error!  Invalid operation domain type " << opDomain << std::endl); 


  // setup field
  PHX::MDField<ScalarT> f(fieldName, scalar_dl); field = f;

  // add dependent fields
  this->addDependentField(field);
  this->addDependentField(coordVec);
  this->addDependentField(weights);
  this->setName(fieldName+" Response Center of Mass"+PHX::TypeString<EvalT>::value);

  using PHX::MDALayout;

  // Setup scatter evaluator
  p.set("Stand-alone Evaluator", false);
  std::string local_response_name = 
    fieldName + " Local Response Center of Mass";
  std::string global_response_name = 
    fieldName + " Global Response Center of Mass";
  int worksetSize = scalar_dl->dimension(0);
  int responseSize = 4;
  Teuchos::RCP<PHX::DataLayout> local_response_layout =
    Teuchos::rcp(new MDALayout<Cell,Dim>(worksetSize, responseSize));
  Teuchos::RCP<PHX::DataLayout> global_response_layout =
    Teuchos::rcp(new MDALayout<Dim>(responseSize));
  PHX::Tag<ScalarT> local_response_tag(local_response_name, 
				       local_response_layout);
  PHX::Tag<ScalarT> global_response_tag(global_response_name, 
					global_response_layout);
  p.set("Local Response Field Tag", local_response_tag);
  p.set("Global Response Field Tag", global_response_tag);
  PHAL::SeparableScatterScalarResponse<EvalT,Traits>::setup(p,dl);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseCenterOfMass<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(field,fm);
  this->utils.setFieldData(coordVec,fm);
  this->utils.setFieldData(weights,fm);
  PHAL::SeparableScatterScalarResponse<EvalT,Traits>::postRegistrationSetup(d,fm);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseCenterOfMass<EvalT, Traits>::
preEvaluate(typename Traits::PreEvalData workset)
{
  for (typename PHX::MDField<ScalarT>::size_type i=0; 
       i<this->global_response.size(); i++)
    this->global_response[i] = 0.0;

  // Do global initialization
  PHAL::SeparableScatterScalarResponse<EvalT,Traits>::preEvaluate(workset);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseCenterOfMass<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  // Zero out local response
  for (typename PHX::MDField<ScalarT>::size_type i=0; 
       i<this->local_response.size(); i++)
    this->local_response[i] = 0.0;

  ScalarT integral, moment;

  if(opDomain == "element block" && workset.EBName != ebName) 
  {
      return;
  }

  for (std::size_t cell=0; cell < workset.numCells; ++cell) 
  {
    // If operation domain is a "box", check whether the current cell is 
    //  at least partially contained within the box
    if(opDomain == "box") {
      bool cellInBox = false;
      for (std::size_t qp=0; qp < numQPs; ++qp) {
        if( (!limitX || (coordVec(cell,qp,0) >= xmin && coordVec(cell,qp,0) <= xmax)) &&
            (!limitY || (coordVec(cell,qp,1) >= ymin && coordVec(cell,qp,1) <= ymax)) &&
            (!limitZ || (coordVec(cell,qp,2) >= zmin && coordVec(cell,qp,2) <= zmax)) ) {
          cellInBox = true; break; }
      }
      if( !cellInBox ) continue;
    }

    // Add to running total volume and mass moment
    for (std::size_t qp=0; qp < numQPs; ++qp) {
      integral = field(cell,qp) * weights(cell,qp);
      this->local_response(cell,3) += integral;
      this->global_response(3) += integral;

      for(std::size_t i=0; i<numDims && i<3; i++) {
	moment = field(cell,qp) * weights(cell,qp) * coordVec(cell,qp,i);
	this->local_response(cell,i) += moment;
	this->global_response(i) += moment;
      }
    }

  }

  // Do any local-scattering necessary
  PHAL::SeparableScatterScalarResponse<EvalT,Traits>::evaluateFields(workset);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseCenterOfMass<EvalT, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  // Add contributions across processors
  Teuchos::RCP< Teuchos::ValueTypeSerializer<int,ScalarT> > serializer =
    workset.serializerManager.template getValue<EvalT>();
  Teuchos::reduceAll(
    *workset.comm, *serializer, Teuchos::REDUCE_SUM,
    this->global_response.size(), &this->global_response[0], 
    &this->global_response[0]);

  int iNormalizer = 3;
  if( fabs(this->global_response[iNormalizer]) > 1e-9 ) {
    for( int i=0; i < this->global_response.size(); i++) {
      if( i == iNormalizer ) continue;
      this->global_response[i] /= this->global_response[iNormalizer];
    }
    this->global_response[iNormalizer] = 1.0;
  }

  // Do global scattering
  PHAL::SeparableScatterScalarResponse<EvalT,Traits>::postEvaluate(workset);
}

// **********************************************************************
template<typename EvalT,typename Traits>
Teuchos::RCP<const Teuchos::ParameterList>
QCAD::ResponseCenterOfMass<EvalT,Traits>::getValidResponseParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
     	rcp(new Teuchos::ParameterList("Valid ResponseCenterOfMass Params"));;
  Teuchos::RCP<const Teuchos::ParameterList> baseValidPL =
    PHAL::SeparableScatterScalarResponse<EvalT,Traits>::getValidResponseParameters();
  validPL->setParameters(*baseValidPL);

  validPL->set<string>("Name", "", "Name of response function");
  validPL->set<int>("Phalanx Graph Visualization Detail", 0, "Make dot file to visualize phalanx graph");
  validPL->set<string>("Field Name", "", "Scalar field from which to compute center of mass");
  validPL->set<string>("Operation Domain", "box", "Region to perform operation: 'box' or 'element block'");

  validPL->set<double>("x min", 0.0, "Box domain minimum x coordinate");
  validPL->set<double>("x max", 0.0, "Box domain maximum x coordinate");
  validPL->set<double>("y min", 0.0, "Box domain minimum y coordinate");
  validPL->set<double>("y max", 0.0, "Box domain maximum y coordinate");
  validPL->set<double>("z min", 0.0, "Box domain minimum z coordinate");
  validPL->set<double>("z max", 0.0, "Box domain maximum z coordinate");

  validPL->set<string>("Element Block Name", "", "Element block name that specifies domain");

  return validPL;
}

// **********************************************************************

