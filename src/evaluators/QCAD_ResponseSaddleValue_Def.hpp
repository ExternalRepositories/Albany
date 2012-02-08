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
#include "Teuchos_Array.hpp"
#include "Teuchos_TestForException.hpp"
#include "Teuchos_CommHelpers.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Sacado_ParameterRegistration.hpp"


template<typename EvalT, typename Traits>
QCAD::ResponseSaddleValue<EvalT, Traits>::
ResponseSaddleValue(Teuchos::ParameterList& p,
		    const Teuchos::RCP<Albany::Layouts>& dl) :
  coordVec(p.get<string>("Coordinate Vector Name"), dl->qp_vector),
  coordVec_vertices(p.get<string>("Coordinate Vector Name"), dl->vertices_vector),
  weights(p.get<std::string>("Weights Name"), dl->qp_scalar)
{
  //! get and validate Response parameter list
  Teuchos::ParameterList* plist = 
    p.get<Teuchos::ParameterList*>("Parameter List");

  Teuchos::RCP<const Teuchos::ParameterList> reflist = 
    this->getValidResponseParameters();
  plist->validateParameters(*reflist,0);

  //! get pointer to response function object
  svResponseFn = plist->get<Teuchos::RCP<QCAD::SaddleValueResponseFunction> >
    ("Response Function");

  //! number of quad points per cell and dimension of space
  std::vector<PHX::DataLayout::size_type> dims;
  dl->qp_vector->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];
  dl->vertices_vector->dimensions(dims);
  numVertices = dims[2];

  //! User-specified parameters
  fieldName  = plist->get<std::string>("Field Name");
  fieldGradientName  = plist->get<std::string>("Field Gradient Name");
  scaling = plist->get<double>("Field Scaling Factor",1.0);

  retFieldName = plist->get<std::string>("Return Field Name", fieldName);
  retScaling = plist->get<double>("Return Field Scaling Factor",1.0);
  bReturnSameField = (fieldName == retFieldName);
  //bLateralVolumes = true; // Future: make into a parameter

  //! setup operation field and its gradient, and the return field (if it's different)
  PHX::MDField<ScalarT> f(fieldName, dl->qp_scalar); field = f;
  PHX::MDField<ScalarT> fg(fieldGradientName, dl->qp_vector); fieldGradient = fg;

  if(!bReturnSameField) {
    PHX::MDField<ScalarT> fr(retFieldName, dl->qp_scalar); retField = fr; }


  //! add dependent fields
  this->addDependentField(field);
  this->addDependentField(fieldGradient);
  this->addDependentField(coordVec);
  this->addDependentField(coordVec_vertices);
  this->addDependentField(weights);
  if(!bReturnSameField) this->addDependentField(retField);

  std::string responseID = "QCAD Saddle Value";
  this->setName(responseID + PHX::TypeString<EvalT>::value);

  /*//! response evaluator must evaluate dummy operation
  Teuchos::RCP<PHX::DataLayout> dummy_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("Dummy Data Layout");
  
  response_operation = Teuchos::rcp(new PHX::Tag<ScalarT>(responseID, dummy_dl));
  this->addEvaluatedField(*response_operation);*/

  // Setup scatter evaluator
  p.set("Stand-alone Evaluator", false);

  int responseSize = 5;
  int worksetSize = dl->qp_scalar->dimension(0);
  Teuchos::RCP<PHX::DataLayout> global_response_layout =
    Teuchos::rcp(new PHX::MDALayout<Dim>(responseSize));
  Teuchos::RCP<PHX::DataLayout> local_response_layout =
    Teuchos::rcp(new PHX::MDALayout<Cell,Dim>(worksetSize, responseSize));


  std::string local_response_name = 
    fieldName + " Local Response Saddle Value";
  std::string global_response_name = 
    fieldName + " Global Response Saddle Value";

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
void QCAD::ResponseSaddleValue<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(field,fm);
  this->utils.setFieldData(fieldGradient,fm);
  this->utils.setFieldData(coordVec,fm);
  this->utils.setFieldData(coordVec_vertices,fm);
  this->utils.setFieldData(weights,fm);
  if(!bReturnSameField) this->utils.setFieldData(retField,fm);
  PHAL::SeparableScatterScalarResponse<EvalT,Traits>::postRegistrationSetup(d,fm);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseSaddleValue<EvalT, Traits>::
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
void QCAD::ResponseSaddleValue<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  // Zero out local response
  for (typename PHX::MDField<ScalarT>::size_type i=0; 
       i<this->local_response.size(); i++)
    this->local_response[i] = 0.0;

  const int MAX_DIMS = 3;
  ScalarT fieldVal, retFieldVal, cellVol; //, retCellVol;
  std::vector<ScalarT> fieldGrad(numDims, 0.0);
  double dblAvgCoords[MAX_DIMS], dblFieldGrad[MAX_DIMS], dblFieldVal;

  if(svResponseFn->getMode() == "Point location") {    
    for (std::size_t cell=0; cell < workset.numCells; ++cell) {

      getAvgCellCoordinates(coordVec, cell, dblAvgCoords);
      getCellQuantities(cell, cellVol, fieldVal, retFieldVal, fieldGrad);
      dblFieldVal = QCAD::EvaluatorTools<EvalT,Traits>::getDoubleValue(fieldVal);
      svResponseFn->addBeginPointData(workset.EBName, dblAvgCoords, dblFieldVal);
      svResponseFn->addEndPointData(workset.EBName, dblAvgCoords, dblFieldVal);
    }
  }
  else if(svResponseFn->getMode() == "Collect image point data") {
    for (std::size_t cell=0; cell < workset.numCells; ++cell) {
      getAvgCellCoordinates(coordVec, cell, dblAvgCoords);

      if(svResponseFn->pointIsInImagePtRegion(dblAvgCoords)) {	
	getCellQuantities(cell, cellVol, fieldVal, retFieldVal, fieldGrad);

	dblFieldVal = QCAD::EvaluatorTools<EvalT,Traits>::getDoubleValue(fieldVal);
	for (std::size_t k=0; k < numDims; ++k)
	  dblFieldGrad[k] = QCAD::EvaluatorTools<EvalT,Traits>::getDoubleValue(fieldGrad[k]);

	svResponseFn->addImagePointData(dblAvgCoords, dblFieldVal, dblFieldGrad);
      }
    }
  }
  else if(svResponseFn->getMode() == "Fill saddle point") {
    double wt;
    double totalWt = svResponseFn->getTotalSaddlePointWeight();

    for (std::size_t cell=0; cell < workset.numCells; ++cell) {
      getAvgCellCoordinates(coordVec, cell, dblAvgCoords);

      if( (wt = svResponseFn->getSaddlePointWeight(dblAvgCoords)) > 0.0) {
	getCellQuantities(cell, cellVol, fieldVal, retFieldVal, fieldGrad);
	wt /= totalWt;

	// Return field value
	this->local_response(cell,0) += wt*retFieldVal;
	this->global_response[0] += wt*retFieldVal;

	// Field value (field searched for saddle point)
	this->local_response(cell,1) += wt*fieldVal;
	this->global_response[1] += wt*fieldVal;

	this->global_response[2] = 0.0; // x-coord -- written later: would just be a MeshScalar anyway
	this->global_response[3] = 0.0; // y-coord -- written later: would just be a MeshScalar anyway
	this->global_response[4] = 0.0; // z-coord -- written later: would just be a MeshScalar anyway
      }
    }
  }
  else TEUCHOS_TEST_FOR_EXCEPTION (true, Teuchos::Exceptions::InvalidParameter, std::endl 
		      << "Error!  Invalid mode: " << svResponseFn->getMode() << std::endl); 

  // Do any local-scattering necessary
  PHAL::SeparableScatterScalarResponse<EvalT,Traits>::evaluateFields(workset);
}

//OLD: Keep for reference for now:
    // get a volume or area used for computing an average cell linear size
    /*if( bLateralVolumes ) {
      std::vector<ScalarT> maxCoord(3,-1e10);
      std::vector<ScalarT> minCoord(3,+1e10);

      for (std::size_t v=0; v < numVertices; ++v) {
	for (std::size_t k=0; k < numDims; ++k) {
	  if(maxCoord[k] < coordVec_vertices(cell,v,k)) maxCoord[k] = coordVec_vertices(cell,v,k);
	  if(minCoord[k] > coordVec_vertices(cell,v,k)) minCoord[k] = coordVec_vertices(cell,v,k);
	}
      }

      retCellVol = 1.0;
      for (std::size_t k=0; k < numDims && k < 2; ++k)  //limit to at most 2 dimensions
	retCellVol *= (maxCoord[k] - minCoord[k]);
    }
    else retCellVol = cellVol;
    */


// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseSaddleValue<EvalT, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  // Add contributions across processors
  Teuchos::RCP< Teuchos::ValueTypeSerializer<int,ScalarT> > serializer =
    workset.serializerManager.template getValue<EvalT>();
  Teuchos::reduceAll(
    *workset.comm, *serializer, Teuchos::REDUCE_SUM,
    this->global_response.size(), &this->global_response[0], 
    &this->global_response[0]);

  // Copy in position of saddle point here (no derivative info yet)
  const double* pt = svResponseFn->getSaddlePointPosition();
  for(std::size_t i=0; i<numDims; i++) 
    this->global_response[2+i] = pt[i];
  
  // Do global scattering
  PHAL::SeparableScatterScalarResponse<EvalT,Traits>::postEvaluate(workset);
}


// **********************************************************************
template<typename EvalT,typename Traits>
Teuchos::RCP<const Teuchos::ParameterList>
QCAD::ResponseSaddleValue<EvalT,Traits>::getValidResponseParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
     	rcp(new Teuchos::ParameterList("Valid ResponseSaddleValue Params"));
  Teuchos::RCP<const Teuchos::ParameterList> baseValidPL =
    PHAL::SeparableScatterScalarResponse<EvalT,Traits>::getValidResponseParameters();
  validPL->setParameters(*baseValidPL);

  validPL->set<string>("Name", "", "Name of response function");
  validPL->set<int>("Phalanx Graph Visualization Detail", 0, "Make dot file to visualize phalanx graph");
  //validPL->set<string>("Type", "", "Response type"); //TODO - remove from all
  validPL->set<string>("Field Name", "", "Scalar field on which to find saddle point");
  validPL->set<string>("Field Gradient Name", "", "Gradient of field on which to find saddle point");
  validPL->set<string>("Return Field Name", "<field name>", "Scalar field to return value from");
  validPL->set<double>("Field Scaling Factor", 1.0, "Scaling factor for field on which to find saddle point");
  validPL->set<double>("Return Field Scaling Factor", 1.0, "Scaling factor for return field");

  validPL->set<int>("Number of Image Points", 10, "Number of image points to use, including the two endpoints");
  validPL->set<double>("Image Point Size", 1.0, "Size of image points, modeled as gaussian weight distribution");
  validPL->set<int>("Maximum Iterations", 100, "Maximum number of NEB iterations");
  validPL->set<double>("Max Time Step", 1.0, "Maximum (and initial) time step");
  validPL->set<double>("Min Time Step", 0.002, "Minimum time step");
  validPL->set<double>("Convergence Tolerance", 1e-5, "Convergence criterion when |grad| of saddle is below this number");
  validPL->set<double>("Min Spring Constant", 1.0, "Minimum spring constant used between image points (initial time)");
  validPL->set<double>("Max Spring Constant", 1.0, "Maximum spring constant used between image points (final time)");
  validPL->set<string>("Output Filename", "", "Filename to receive elastic band points and values at given interval");
  validPL->set<int>("Output Interval", 0, "Output elastic band points every <output interval> iterations");
  validPL->set<string>("Debug Filename", "", "Filename for algorithm debug info");
  validPL->set<bool>("Climbing NEB", true, "Whether or not to use the climbing NEB algorithm");
  validPL->set<double>("Anti-Kink Factor", 0.0, "Factor between 0 and 1 giving about of perpendicular spring force to inclue");

  validPL->set<double>("z min", 0.0, "Box domain minimum z coordinate");
  validPL->set<double>("z max", 0.0, "Box domain maximum z coordinate");

  validPL->set<Teuchos::Array<double> >("Begin Point", Teuchos::Array<double>(), "Beginning point of elastic band");
  validPL->set<string>("Begin Element Block", "", "Element block name whose minimum marks the elastic band's beginning");
  validPL->sublist("Begin Polygon", false, "Beginning polygon sublist");

  validPL->set<Teuchos::Array<double> >("End Point", Teuchos::Array<double>(), "Ending point of elastic band");
  validPL->set<string>("End Element Block", "", "Element block name whose minimum marks the elastic band's ending");
  validPL->sublist("End Polygon", false, "Ending polygon sublist");

  validPL->set<Teuchos::Array<double> >("Saddle Point Guess", Teuchos::Array<double>(), "Estimate of where the saddle point lies");

  validPL->set<int>("Debug Mode", 0, "Print verbose debug messages to stdout");
  validPL->set< Teuchos::RCP<QCAD::SaddleValueResponseFunction> >("Response Function", Teuchos::null, "Saddle value response function");

  return validPL;
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseSaddleValue<EvalT, Traits>::
getCellQuantities(const std::size_t cell, typename EvalT::ScalarT& cellVol, typename EvalT::ScalarT& fieldVal, 
		    typename EvalT::ScalarT& retFieldVal, std::vector<typename EvalT::ScalarT>& fieldGrad) const
{
  cellVol = 0.0;
  fieldVal = 0.0;
  retFieldVal = 0.0;
  for (std::size_t k=0; k < numDims; ++k) fieldGrad[k] = 0.0;

  // Get the cell volume
  for (std::size_t qp=0; qp < numQPs; ++qp)
    cellVol += weights(cell,qp);

  // Get cell average value of field
  for (std::size_t qp=0; qp < numQPs; ++qp)
    fieldVal += field(cell,qp) * weights(cell,qp);
  fieldVal *= scaling / cellVol;  

  // Get the cell average of the field to return
  if(bReturnSameField) {
    retFieldVal = retScaling * fieldVal;
  }
  else {
    for (std::size_t qp=0; qp < numQPs; ++qp)
      retFieldVal += retField(cell,qp) * weights(cell,qp);
    retFieldVal *= retScaling / cellVol;
  }

  // Get cell average of the gradient field
  for (std::size_t qp=0; qp < numQPs; ++qp) {
    for (std::size_t k=0; k < numDims; ++k) 
      fieldGrad[k] += fieldGradient(cell,qp,k) * weights(cell,qp);
  }
  for (std::size_t k=0; k < numDims; ++k) fieldGrad[k] *= scaling / cellVol; 

  return;
}


// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseSaddleValue<EvalT, Traits>::
  getAvgCellCoordinates(PHX::MDField<typename EvalT::MeshScalarT,Cell,QuadPoint,Dim> coordVec,
			const std::size_t cell, double* dblAvgCoords) const
{
  std::vector<MeshScalarT> avgCoord(numDims, 0.0); //just a double?

  //Get average cell coordinate (avg of qps)
  for (std::size_t k=0; k < numDims; ++k) avgCoord[k] = 0.0;
  for (std::size_t qp=0; qp < numQPs; ++qp) {
    for (std::size_t k=0; k < numDims; ++k) 
      avgCoord[k] += coordVec(cell,qp,k);
  }
  for (std::size_t k=0; k < numDims; ++k) {
    avgCoord[k] /= numQPs;
    dblAvgCoords[k] = QCAD::EvaluatorTools<EvalT,Traits>::getDoubleValue(avgCoord[k]);
  }
}
