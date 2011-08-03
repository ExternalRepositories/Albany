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


#include "PHAL_AlbanyTraits.hpp"

const std::string PHAL::TypeString<PHAL::AlbanyTraits::Residual>::value = 
  "<Residual>";

const std::string PHAL::TypeString<PHAL::AlbanyTraits::Jacobian>::value = 
  "<Jacobian>";

const std::string PHAL::TypeString<PHAL::AlbanyTraits::Tangent>::value = 
  "<Tangent>";

const std::string PHAL::TypeString<PHAL::AlbanyTraits::SGResidual>::value = 
  "<SGResidual>";

const std::string PHAL::TypeString<PHAL::AlbanyTraits::SGJacobian>::value = 
  "<SGJacobian>";

const std::string PHAL::TypeString<PHAL::AlbanyTraits::MPResidual>::value = 
  "<MPResidual>";

const std::string PHAL::TypeString<PHAL::AlbanyTraits::MPJacobian>::value = 
  "<MPJacobian>";

const std::string PHAL::TypeString<RealType>::value = 
  "double";

const std::string PHAL::TypeString<FadType>::
  value = "Sacado::ELRFad::DFad<double>";

const std::string PHAL::TypeString<SGType>::
  value = "Sacado::PCE::OrthogPoly<double>";

const std::string PHAL::TypeString<SGFadType>::
  value = "Sacado::ELRCacheFad::DFad< Sacado::PCE::OrthogPoly<double> >";

const std::string PHAL::TypeString<MPType>::
  value = "Sacado::ETV::Vector<double>";

const std::string PHAL::TypeString<MPFadType>::
  value = "Sacado::ELRCacheFad::DFad< Sacado::ETV::Vector<double> >";
