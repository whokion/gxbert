//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
//
// $Id: GXSigmaZero.cc 79357 2014-02-25 10:06:54Z gcosmo $
//
// 
// ----------------------------------------------------------------------
//      GEANT 4 class implementation file
//
//      History: first implementation, based on object model of
//      4th April 1996, G.Cosmo
// **********************************************************************
//  New impelemenataion as an utility class  M.Asai, 26 July 2004
// ----------------------------------------------------------------------

#include "GXSigmaZero.hh"
#include "GXPhysicalConstants.hh"
#include "GXSystemOfUnits.hh"
#include "GXParticleTable.hh"

#include "GXPhaseSpaceDecayChannel.hh"
#include "GXDecayTable.hh"

// ######################################################################
// ###                           SigmaZero                            ###
// ######################################################################

GXSigmaZero* GXSigmaZero::theInstance = 0;

GXSigmaZero* GXSigmaZero::Definition()
{
  if (theInstance !=0) return theInstance;
  const G4String name = "sigma0";
  // search in particle table]
  GXParticleTable* pTable = GXParticleTable::GetParticleTable();
  GXParticleDefinition* anInstance = pTable->FindParticle(name);
  if (anInstance ==0)
  {
  // create particle
  //
  //    Arguments for constructor are as follows
  //               name             mass          width         charge
  //             2*spin           parity  C-conjugation
  //          2*Isospin       2*Isospin3       G-parity
  //               type    lepton number  baryon number   PDG encoding
  //             stable         lifetime    decay table
  //             shortlived      subType    anti_encoding

    anInstance = new GXParticleDefinition(
                 name,    1.192642*GeV,    8.9e-3*MeV,         0.0,
                    1,              +1,             0,
                    2,               0,             0,
             "baryon",               0,            +1,        3212,
                false,      7.4e-11*ns,          NULL,
                false,       "sigma");
   // Life time is given from width
   anInstance->SetPDGLifeTime( hbar_Planck/(anInstance->GetPDGWidth()) );

    //create Decay Table
    GXDecayTable* table = new GXDecayTable();
    
    // create decay channels
    // sigma0 -> lambda + gamma
    GXVDecayChannel* mode  = new GXPhaseSpaceDecayChannel("sigma0",1.000,2,"lambda","gamma");
    
    table->Insert(mode);
    
    anInstance->SetDecayTable(table);
  }
  theInstance = reinterpret_cast<GXSigmaZero*>(anInstance);
  return theInstance;
}

GXSigmaZero*  GXSigmaZero::SigmaZeroDefinition()
{
  return Definition();
}

GXSigmaZero*  GXSigmaZero::SigmaZero()
{
  return Definition();
}


