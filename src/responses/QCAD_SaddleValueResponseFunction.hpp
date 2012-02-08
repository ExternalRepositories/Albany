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


#ifndef QCAD_SADDLEVALUERESPONSEFUNCTION_HPP
#define QCAD_SADDLEVALUERESPONSEFUNCTION_HPP

#include "Albany_FieldManagerScalarResponseFunction.hpp"

namespace QCAD {

  // Helper class: a vector with math operators
  class mathVector
  {
  public:
    mathVector();
    mathVector(int n);
    mathVector(const mathVector& copy);
    ~mathVector();

    void resize(std::size_t n);
    void fill(double d);
    void fill(const double* vec);
    double dot(const mathVector& v2) const;
    double distanceTo(const mathVector& v2) const;
    double distanceTo(const double* p) const;

    double norm() const;
    double norm2() const;
    void normalize();

    double* data();
    const double* data() const;
    std::size_t size() const;

    mathVector& operator=(const mathVector& rhs);

    mathVector operator+(const mathVector& v2) const;
    mathVector operator-(const mathVector& v2) const;
    mathVector operator*(double scale) const;

    mathVector& operator+=(const mathVector& v2);
    mathVector& operator-=(const mathVector& v2);
    mathVector& operator*=(double scale);
    mathVector& operator/=(double scale);

    double& operator[](int i);
    const double& operator[](int i) const;

  private:
    int dim_;
    std::vector<double> data_;
  };

  // Data Structure for an image point
  struct nebImagePt {
    void init(int nDims) {
      coords.resize(nDims); coords.fill(0.0);
      velocity.resize(nDims); velocity.fill(0.0);
      grad.resize(nDims); grad.fill(0.0);
      value = weight = 0.0;
    }

    void init(const mathVector& coordPt) {
      init(coordPt.size());
      coords = coordPt;
    }
      
    mathVector coords;
    mathVector velocity;
    mathVector grad;
    double value;
    double weight;
  };

  std::ostream& operator<<(std::ostream& os, const mathVector& mv);
  std::ostream& operator<<(std::ostream& os, const nebImagePt& np);
 
 
  /*!
   * \brief Reponse function for finding saddle point values of a field
   */
  class SaddleValueResponseFunction : 
    public Albany::FieldManagerScalarResponseFunction {
  public:
  
    //! Constructor
    SaddleValueResponseFunction(
      const Teuchos::RCP<Albany::Application>& application,
      const Teuchos::RCP<Albany::AbstractProblem>& problem,
      const Teuchos::RCP<Albany::MeshSpecsStruct>&  ms,
      const Teuchos::RCP<Albany::StateManager>& stateMgr,
      Teuchos::ParameterList& responseParams);

    //! Destructor
    virtual ~SaddleValueResponseFunction();

    //! Get the number of responses
    virtual unsigned int numResponses() const;

    //! Evaluate responses
    virtual void 
    evaluateResponse(const double current_time,
		     const Epetra_Vector* xdot,
		     const Epetra_Vector& x,
		     const Teuchos::Array<ParamVec>& p,
		     Epetra_Vector& g);

    //! Evaluate tangent = dg/dx*dx/dp + dg/dxdot*dxdot/dp + dg/dp
    virtual void 
    evaluateTangent(const double alpha, 
		    const double beta,
		    const double current_time,
		    bool sum_derivs,
		    const Epetra_Vector* xdot,
		    const Epetra_Vector& x,
		    const Teuchos::Array<ParamVec>& p,
		    ParamVec* deriv_p,
		    const Epetra_MultiVector* Vxdot,
		    const Epetra_MultiVector* Vx,
		    const Epetra_MultiVector* Vp,
		    Epetra_Vector* g,
		    Epetra_MultiVector* gx,
		    Epetra_MultiVector* gp);

    //! Evaluate gradient = dg/dx, dg/dxdot, dg/dp
    virtual void 
    evaluateGradient(const double current_time,
		     const Epetra_Vector* xdot,
		     const Epetra_Vector& x,
		     const Teuchos::Array<ParamVec>& p,
		     ParamVec* deriv_p,
		     Epetra_Vector* g,
		     Epetra_MultiVector* dg_dx,
		     Epetra_MultiVector* dg_dxdot,
		     Epetra_MultiVector* dg_dp);


    //! Post process responses
    virtual void 
    postProcessResponses(const Epetra_Comm& comm, const Teuchos::RCP<Epetra_Vector>& g);

    //! Post process response derivatives
    virtual void 
    postProcessResponseDerivatives(const Epetra_Comm& comm, const Teuchos::RCP<Epetra_MultiVector>& gt);

    //! Called by evaluator to interface with class data that persists across worksets
    std::string getMode();
    bool pointIsInImagePtRegion(const double* p);
    void addBeginPointData(const std::string& elementBlock, const double* p, double value);
    void addEndPointData(const std::string& elementBlock, const double* p, double value);
    void addImagePointData(const double* p, double value, double* grad);
    double getSaddlePointWeight(const double* p) const;
    double getTotalSaddlePointWeight() const;
    const double* getSaddlePointPosition() const;

    
  private:

    //! Helper functions for Nudged Elastic Band (NEB) algorithm, perfomred in evaluateResponse
    void initializeImagePoints(const double current_time, const Epetra_Vector* xdot,
			       const Epetra_Vector& x, const Teuchos::Array<ParamVec>& p,
			       Epetra_Vector& g, int dbMode);
    void doNudgedElasticBand(const double current_time, const Epetra_Vector* xdot,
			     const Epetra_Vector& x, const Teuchos::Array<ParamVec>& p,
			     Epetra_Vector& g, int dbMode);
    void fillSaddlePointData(const double current_time, const Epetra_Vector* xdot,
			     const Epetra_Vector& x, const Teuchos::Array<ParamVec>& p,
			     Epetra_Vector& g, int dbMode);


    //! Helper functions for doNudgedElasticBand(...)
    void getImagePointValues(const double current_time, const Epetra_Vector* xdot,
			     const Epetra_Vector& x, const Teuchos::Array<ParamVec>& p,
			     Epetra_Vector& g, double* globalPtValues, double* globalPtWeights,
			     double* globalPtGrads, std::vector<mathVector> lastPositions, int dbMode);
    void writeOutput(int nIters);
    void initialIterationSetup(double& gradScale, double& springScale, int dbMode);
    void computeTangent(std::size_t i, mathVector& tangent, int dbMode);
    void computeClimbingForce(std::size_t i, const QCAD::mathVector& tangent, 
			      const double& gradScale, QCAD::mathVector& force, int dbMode);
    void computeForce(std::size_t i, const QCAD::mathVector& tangent, 
		      const std::vector<double>& springConstants,
		      const double& gradScale,  const double& springScale, 
		      QCAD::mathVector& force, double& dt, double& dt2, int dbMode);

    bool matchesCurrentResults(Epetra_Vector& g) const;


    //! Private to prohibit copying
    SaddleValueResponseFunction(const SaddleValueResponseFunction&);
    
    //! Private to prohibit copying
    SaddleValueResponseFunction& operator=(const SaddleValueResponseFunction&);

    //! function giving distribution of weights for "point"
    double pointFn(double d) const;

    //! data used across worksets and processors in saddle point algorithm
    std::size_t numDims;
    std::size_t nImagePts;
    std::vector<nebImagePt> imagePts;
    double imagePtSize;
    bool bClimbing;
    double antiKinkFactor;

    // just store a nebImagePt or index instead?
    mathVector saddlePt; 
    double saddlePtWeight; 
    double saddlePtVal, returnFieldVal;

    double maxTimeStep, minTimeStep;
    double minSpringConstant, maxSpringConstant;
    std::size_t maxIterations;
    double convergeTolerance;

    //! data for beginning and ending regions
    std::string beginRegionType, endRegionType; // "Point", "Element Block", or "Polygon"
    std::string beginElementBlock, endElementBlock;
    std::vector<mathVector> beginPolygon, endPolygon;
    bool saddleGuessGiven;
    mathVector saddlePointGuess;

    double zmin, zmax;  //defines lateral-volume region when numDims == 3
    double xmin, xmax, ymin, ymax; // dynamically adjusted box marking region containing image points

    //! accumulation vectors for evaluator to fill
    mathVector imagePtValues;
    mathVector imagePtWeights;
    mathVector imagePtGradComps;

    //! mode of current evaluator operation (maybe not thread safe?)
    std::string mode;

    int  debugMode;

    std::string outputFilename;
    std::string debugFilename;
    int nEvery;
  };

  


}

#endif // QCAD_SADDLEVALUERESPONSEFUNCTION_HPP
