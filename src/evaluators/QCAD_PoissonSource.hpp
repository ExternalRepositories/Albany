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


#ifndef QCAD_POISSONSOURCE_HPP
#define QCAD_POISSONSOURCE_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Teuchos_ParameterList.hpp"
#include "Epetra_Vector.h"
#include "Sacado_ParameterAccessor.hpp"
#include "Stokhos_KL_ExponentialRandomField.hpp"
#include "Teuchos_Array.hpp"

#include "QCAD_MaterialDatabase.hpp"

/** 
 * \brief Evaluates Poisson Source Term 
 */
namespace QCAD 
{
  template<typename EvalT, typename Traits>
  class PoissonSource : 
  public PHX::EvaluatorWithBaseImpl<Traits>,
  public PHX::EvaluatorDerived<EvalT, Traits>,
  public Sacado::ParameterAccessor<EvalT, SPL_Traits> 
  {
  public:
    typedef typename EvalT::ScalarT ScalarT;
    typedef typename EvalT::MeshScalarT MeshScalarT;

    PoissonSource(Teuchos::ParameterList& p);
  
    void postRegistrationSetup(typename Traits::SetupData d,
         PHX::FieldManager<Traits>& vm);
  
    void evaluateFields(typename Traits::EvalData d);
  
    //! Function to allow parameters to be exposed for embedded analysis
    ScalarT& getValue(const std::string &n);

    //! Public Universal Constants
    /***** define universal constants as double constants *****/
    static const double kbBoltz; // Boltzmann constant in [eV/K]
    static const double eps0; // vacuum permittivity in [C/(V.cm)]
    static const double eleQ; // electron elemental charge in [C]
    static const double m0;   // vacuum electron mass in [kg]
    static const double hbar; // reduced planck constant in [J.s]
    static const double pi;   // pi constant (unitless)

  private:

    //! Reference parameter list generator to check xml input file
    Teuchos::RCP<const Teuchos::ParameterList>
        getValidPoissonSourceParameters() const;

    //! evaluateFields functions for different device types (device specified in xml input)
    void evaluateFields_elementblocks(typename Traits::EvalData workset);
    void evaluateFields_default(typename Traits::EvalData workset);
        
    //! compute the Maxwell-Boltzmann statistics
    inline ScalarT computeMBStat(const ScalarT x);
        
    //! compute the Fermi-Dirac integral of 1/2 order
    inline ScalarT computeFDIntOneHalf(const ScalarT x);
        
    //! compute the 0-K Fermi-Dirac integral
    inline ScalarT computeZeroKFDInt(const ScalarT x);
        
    //! return the doping value when incompIonization = False
    inline ScalarT fullDopants(const std::string dopType, const ScalarT &x);
        
    //! compute the ionized dopants when incompIonization = True
    ScalarT ionizedDopants(const std::string dopType, const ScalarT &x);

    //! compute the Fermi-Dirac integral of -1/2 order for calculating electron 
    //! density in the 2D Poisson-Schrondinger loop
    ScalarT computeFDIntMinusOneHalf(const ScalarT x);
    
    //! compute the electron density for Poisson-Schrodinger iteration
    ScalarT eDensityForPoissonSchrond(typename Traits::EvalData workset, std::size_t cell, std::size_t qp, const std::vector<double> &eigenvals);
    
    //! read eigenvalues from a text file (temporary, until we have a better way of passing them)
    std::vector<double> ReadEigenvaluesFromFile(int numberToRead);

  	//! input
  	std::size_t numQPs;
  	std::size_t numDims;
  	PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim> coordVec;
  	PHX::MDField<ScalarT,Cell,QuadPoint> potential;	// scaled potential (no unit)
    PHX::MDField<ScalarT,Dim> temperatureField; // lattice temperature [K]

  	//! output
        PHX::MDField<ScalarT,Cell,QuadPoint> poissonSource; // scaled RHS (unitless)
        PHX::MDField<ScalarT,Cell,QuadPoint> chargeDensity; // space charge density in [cm-3]
        PHX::MDField<ScalarT,Cell,QuadPoint> electronDensity; // electron density in [cm-3]
        PHX::MDField<ScalarT,Cell,QuadPoint> holeDensity;   // electron density in [cm-3]
        PHX::MDField<ScalarT,Cell,QuadPoint> electricPotential;	// phi in [V]
        PHX::MDField<ScalarT,Cell,QuadPoint> ionizedDopant;    // ionized dopants in [cm-3]
        PHX::MDField<ScalarT,Cell,QuadPoint> conductionBand; // conduction band in [eV]
        PHX::MDField<ScalarT,Cell,QuadPoint> valenceBand;   // valence band in [eV]

  	//! constant prefactor parameter in source function
  	ScalarT factor;

  	//! temperature parameter in source function
  	ScalarT temperatureName; //name of temperature field
    
    //! string variable to differ the various devices implementation
    std::string device;
    
    //! specify carrier statistics and incomplete ionization
    std::string carrierStatistics;
    std::string incompIonization;
        
    //! donor and acceptor concentrations (for element blocks nsilicon & psilicon)
    double dopingDonor;   // in [cm-3]
    double dopingAcceptor;
        
    //! donor and acceptor activation energy in [eV]
    double donorActE;     // (Ec-Ed) where Ed = donor energy level
    double acceptorActE;  // (Ea-Ev) where Ea = acceptor energy level
        
        //! scaling parameters
        double length_unit_in_m; // length unit for input and output mesh
        //ScalarT C0;  // scaling for conc. [cm^-3]
        //ScalarT Lambda2;  // derived scaling factor (unitless) that appears in the scaled Poisson equation
        
        //! Schrodinger coupling
        bool bSchrodingerInQuantumRegions;
        int  nEigenvectors;
        std::string eigenValueFilename;
        std::string evecStateRoot;

	//! Material database
        Teuchos::RCP<QCAD::MaterialDatabase> materialDB;
	};
}

#endif
