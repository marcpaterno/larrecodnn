////////////////////////////////////////////////////////////////////
// Implementation of the EMShower algorithm
//
// Forms EM showers from clusters and associated tracks.
// Also provides methods for finding the vertex and further
// properties of the shower.
//
// Mike Wallbank (m.wallbank@sheffield.ac.uk), September 2015
////////////////////////////////////////////////////////////////////

#include "RecoAlg/EMShowerAlg.h"

shower::EMShowerAlg::EMShowerAlg(fhicl::ParameterSet const& pset) : fShowerEnergyAlg(pset.get<fhicl::ParameterSet>("ShowerEnergyAlg")),
                                                                    fCalorimetryAlg(pset.get<fhicl::ParameterSet>("CalorimetryAlg")),
								    fProjectionMatchingAlg(pset.get<fhicl::ParameterSet>("ProjectionMatchingAlg")) {
  fMinTrackLength  = pset.get<double>("MinTrackLength");
  fdEdxTrackLength = pset.get<double>("dEdxTrackLength");
}

void shower::EMShowerAlg::AssociateClustersAndTracks(std::vector<art::Ptr<recob::Cluster> > const& clusters,
						     art::FindManyP<recob::Hit> const& fmh,
						     art::FindManyP<recob::Track> const& fmt,
						     std::map<int,std::vector<int> >& clusterToTracks,
						     std::map<int,std::vector<int> >& trackToClusters) {

  /// Map associated tracks and clusters together given their associated hits

  std::vector<int> clustersToIgnore = {-999};
  this->AssociateClustersAndTracks(clusters, fmh, fmt, clustersToIgnore, clusterToTracks, trackToClusters);

  return;

}

void shower::EMShowerAlg::AssociateClustersAndTracks(std::vector<art::Ptr<recob::Cluster> > const& clusters,
						     art::FindManyP<recob::Hit> const& fmh,
						     art::FindManyP<recob::Track> const& fmt,
						     std::vector<int> const& clustersToIgnore,
						     std::map<int,std::vector<int> >& clusterToTracks,
						     std::map<int,std::vector<int> >& trackToClusters) {

  /// Map associated tracks and clusters together given their associated hits, whilst ignoring certain clusters

  // Look through all the clusters
  for (std::vector<art::Ptr<recob::Cluster> >::const_iterator clusterIt = clusters.begin(); clusterIt != clusters.end(); ++clusterIt) {

    // Get the hits in this cluster
    std::vector<art::Ptr<recob::Hit> > clusterHits = fmh.at(clusterIt->key());

    // Look at all these hits and find the associated tracks
    for (std::vector<art::Ptr<recob::Hit> >::iterator clusterHitIt = clusterHits.begin(); clusterHitIt != clusterHits.end(); ++clusterHitIt) {

      // Get the tracks associated with this hit
      std::vector<art::Ptr<recob::Track> > clusterHitTracks = fmt.at(clusterHitIt->key());
      if (clusterHitTracks.size() > 1) { std::cout << "More than one track associated with this hit!" << std::endl; continue; }
      if (clusterHitTracks.size() < 1) continue;
      if (clusterHitTracks.at(0)->Length() < fMinTrackLength) continue;

      // Add this cluster to the track map
      int track = clusterHitTracks.at(0).key();
      int cluster = (*clusterIt).key();
      if (std::find(clustersToIgnore.begin(), clustersToIgnore.end(), cluster) != clustersToIgnore.end())
	continue;
      if (std::find(trackToClusters[track].begin(), trackToClusters[track].end(), cluster) == trackToClusters[track].end())
	trackToClusters[track].push_back(cluster);
      if (std::find(clusterToTracks[cluster].begin(), clusterToTracks[cluster].end(), track) == clusterToTracks[cluster].end())
	clusterToTracks[cluster].push_back(track);

    }

  }

  return;

}

void shower::EMShowerAlg::CheckShowerPlanes(std::vector<std::vector<int> > const& initialShowers,
					    std::vector<int>& clustersToIgnore,
					    std::vector<art::Ptr<recob::Cluster> > const& clusters,
					    art::FindManyP<recob::Hit> const& fmh) {

  /// Takes the initial showers found and tries to resolve issues where one bad view ruins the event

  for (std::vector<std::vector<int> >::const_iterator initialShowerIt = initialShowers.begin(); initialShowerIt != initialShowers.end(); ++initialShowerIt) {

    // Make maps of all clusters and cluster hits in each view
    std::map<int,std::vector<art::Ptr<recob::Cluster> > > planeClusters;
    std::map<int,std::vector<art::Ptr<recob::Hit> > > planeHits;

    // Loop over the clusters comprising this shower
    for (std::vector<int>::const_iterator clusterIt = initialShowerIt->begin(); clusterIt != initialShowerIt->end(); ++clusterIt) {
      art::Ptr<recob::Cluster> cluster = clusters.at(*clusterIt);
      std::vector<art::Ptr<recob::Hit> > hits = fmh.at(cluster.key());
      planeClusters[cluster->Plane().Plane].push_back(cluster);
      for (std::vector<art::Ptr<recob::Hit> >::iterator hitIt = hits.begin(); hitIt != hits.end(); ++hitIt)
	planeHits[(*hitIt)->WireID().Plane].push_back(*hitIt);
    }

    // Look at how many clusters each plane has, and the proportion of hits each of uses
    std::map<int,std::vector<double> > planeClusterSizes;
    for (std::map<int,std::vector<art::Ptr<recob::Cluster> > >::iterator planeClustersIt = planeClusters.begin(); planeClustersIt != planeClusters.end(); ++planeClustersIt) {
      for (std::vector<art::Ptr<recob::Cluster> >::iterator planeClusterIt = planeClustersIt->second.begin(); planeClusterIt != planeClustersIt->second.end(); ++planeClusterIt) {
	std::vector<art::Ptr<recob::Hit> > hits = fmh.at(planeClusterIt->key());
        planeClusterSizes[planeClustersIt->first].push_back((double)hits.size()/(double)planeHits.at(planeClustersIt->first).size());
      }
    }

    // Find the average hit fraction across all clusters in the plane
    std::map<int,double> planeClusterAverageSizes;
    for (std::map<int,std::vector<double> >::iterator planeClusterSizesIt = planeClusterSizes.begin(); planeClusterSizesIt != planeClusterSizes.end(); ++planeClusterSizesIt) {
      double average = 0;
      for (std::vector<double>::iterator planeClusterSizeIt = planeClusterSizesIt->second.begin(); planeClusterSizeIt != planeClusterSizesIt->second.end(); ++planeClusterSizeIt)
	average += *planeClusterSizeIt;
      average /= planeClusterSizesIt->second.size();
      planeClusterAverageSizes[planeClusterSizesIt->first] = average;
    }

    // Now, decide if there is one plane which is ruining the reconstruction
    // If there are two planes with a low average cluster fraction and one with a high one, this plane likely merges two particle deposits together
    std::vector<int> highAverage, lowAverage;
    for (std::map<int,double>::iterator planeClusterAverageSizeIt = planeClusterAverageSizes.begin(); planeClusterAverageSizeIt != planeClusterAverageSizes.end(); ++planeClusterAverageSizeIt) {
      if (planeClusterAverageSizeIt->second > 0.9) highAverage.push_back(planeClusterAverageSizeIt->first);
      else lowAverage.push_back(planeClusterAverageSizeIt->first);
    }
    int badPlane = -1;
    if (highAverage.size() == 1 and highAverage.size() < lowAverage.size())
      badPlane = highAverage.at(0);

    if (badPlane != -1) 
      for (std::vector<art::Ptr<recob::Cluster> >::iterator clusterIt = planeClusters.at(badPlane).begin(); clusterIt != planeClusters.at(badPlane).end(); ++clusterIt)
	clustersToIgnore.push_back(clusterIt->key());

  }

  return;

}

// std::map<int,std::vector<art::Ptr<recob::Hit> > > shower::EMShowerAlg::CheckTrackHits(std::map<int,std::vector<art::Ptr<recob::Hit> > > const& trackHitsMap) {

//   /// Checks the initial track-like part of the shower between all the views and tries to resolve conflicting tracks

//   std::map<int,std::vector<art::Ptr<recob::Hit> > > newTrackHitsMap;

//   // If there is just one view then there isn't much we can do!
//   if (trackHitsMap.size() < 2)
//     return trackHitsMap;

//   // If there are two views then there are a few things we can do
//   if (trackHitsMap.size() == 2) {

//     bool tracksOk = true;

//     std::vector<double> averageHitTimes;
//     std::vector<std::vector<art::Ptr<recob::Hit> > > planeTrackHits;

//     // Look at the track hits
//     for (std::map<int,std::vector<art::Ptr<recob::Hit> > >::const_iterator trackHits = trackHitsMap.begin(); trackHits != trackHitsMap.end(); ++trackHits) {
//       averageHitTimes[std::distance(trackHitsMap.begin(),trackHits)] = 0;
//       planeTrackHits.push_back(trackHits->second);
//       for (std::vector<art::Ptr<recob::Hit> >::const_iterator hitIt = trackHits->second.begin(); hitIt != trackHits->second.end(); ++hitIt)
// 	averageHitTimes[std::distance(trackHitsMap.begin(),trackHits)] += (*hitIt)->PeakTime();
//       averageHitTimes[std::distance(trackHitsMap.begin(),trackHits)] /= trackHits->second.size();
//     }

//     // Look at the average hit times
//     if (TMath::Abs(averageHitTimes.at(0) - averageHitTimes.at(1)) > 100)
//       tracksOk = false;

//     // Now make a track object and project onto third plane (if it exists)
//     if (fGeom->MaxPlanes() > 2) {
//       pma::Track3D* pmatrack = fProjectionMatchingAlg.buildSegment(planeTrackHits.at(0), planeTrackHits.at(1));
//     }

//   }

// }

void shower::EMShowerAlg::FindShowers(std::map<int,std::vector<int> > const& trackToClusters,
				      std::vector<std::vector<int> >& showers) {

  /// Makes showers given a map between tracks and all clusters associated with them

  // Loop over all tracks 
  for (std::map<int,std::vector<int> >::const_iterator trackToClusterIt = trackToClusters.begin(); trackToClusterIt != trackToClusters.end(); ++ trackToClusterIt) {

    // Find which showers already made are associated with this track
    std::vector<int> matchingShowers;
    for (unsigned int shower = 0; shower < showers.size(); ++shower)
      for (std::vector<int>::const_iterator cluster = trackToClusterIt->second.begin(); cluster != trackToClusterIt->second.end(); ++cluster)
	if ( (std::find(showers.at(shower).begin(), showers.at(shower).end(), *cluster) != showers.at(shower).end()) and
	     (std::find(matchingShowers.begin(), matchingShowers.end(), shower)) == matchingShowers.end() )
	  matchingShowers.push_back(shower);

    // Shouldn't be more than one
    if (matchingShowers.size() > 1)
      std::cout << "WARNING! Number of showers this track matches is " << matchingShowers.size() << std::endl;

    // New shower
    if (matchingShowers.size() < 1)
      showers.push_back(trackToClusterIt->second);

    // Add to existing shower
    else {
      for (std::vector<int>::const_iterator cluster = trackToClusterIt->second.begin(); cluster != trackToClusterIt->second.end(); ++cluster)
	if (std::find(showers.at(matchingShowers.at(0)).begin(), showers.at(matchingShowers.at(0)).end(), *cluster) == showers.at(matchingShowers.at(0)).end())
	  showers.at(matchingShowers.at(0)).push_back(*cluster);
    }
  }

  return;

}


void shower::EMShowerAlg::FindInitialTrack(art::PtrVector<recob::Hit> const& hits, art::Ptr<recob::Track>& initialTrack, std::vector<art::Ptr<recob::Hit> >& initialTrackHits) {

  /// Finds the initial track-like part of the shower and the hits in all views associated with it

  // Find the shower hits on each plane
  std::map<int,std::vector<art::Ptr<recob::Hit> > > planeHitsMap;
  for (art::PtrVector<recob::Hit>::const_iterator hit = hits.begin(); hit != hits.end(); ++hit)
    planeHitsMap[(*hit)->View()].push_back(*hit);

  std::map<int,std::pair<double,std::vector<art::Ptr<recob::Hit> > > > orderedShowerMap;

  for (std::map<int,std::vector<art::Ptr<recob::Hit> > >::iterator planeHits = planeHitsMap.begin(); planeHits != planeHitsMap.end(); ++planeHits) {

    // Find the charge-weighted centre (in cm) of this shower
    TVector2 pos, chargePoint = TVector2(0,0);
    double totalCharge = 0;
    for (std::vector<art::Ptr<recob::Hit> >::const_iterator hit = planeHits->second.begin(); hit != planeHits->second.end(); ++hit) {
      pos = HitPosition(*hit);
      chargePoint += (*hit)->Integral() * pos;
      totalCharge += (*hit)->Integral();
    }
    TVector2 centre = chargePoint / totalCharge;

    // Find a rough shower 'direction'
    int nhits = 0;
    double sumx=0., sumy=0., sumx2=0., sumxy=0.;
    for (std::vector<art::Ptr<recob::Hit> >::const_iterator hit = planeHits->second.begin(); hit != planeHits->second.end(); ++hit) {
      ++nhits;
      pos = HitPosition(*hit);
      sumx += pos.X();
      sumy += pos.Y();
      sumx2 += pos.X() * pos.X();
      sumxy += pos.X() * pos.Y();
    }
    double gradient = (nhits * sumxy - sumx * sumy) / (nhits * sumx2 - sumx * sumx);
    TVector2 direction = TVector2(1,gradient).Unit();

    // Find how far each hit (projected onto this axis) is from the centre
    std::map<double,art::Ptr<recob::Hit> > hitProjection;
    for (std::vector<art::Ptr<recob::Hit> >::const_iterator hit = planeHits->second.begin(); hit != planeHits->second.end(); ++hit) {
      pos = HitPosition(*hit) - centre;
      hitProjection[direction*pos] = *hit;
    }

    // Construct a line from the initial part of the shower

    // forwards
    nhits = 0;
    sumx=0., sumy=0., sumx2=0., sumxy=0.;
    for (std::map<double,art::Ptr<recob::Hit> >::iterator hitIt = hitProjection.begin(); hitIt != hitProjection.end(); ++hitIt) {
      if (std::distance(hitProjection.begin(),hitIt) > 5) break;
      ++nhits;
      pos = HitPosition(hitIt->second);
      sumx += pos.X();
      sumy += pos.Y();
      sumx2 += pos.X() * pos.X();
      sumxy += pos.X() * pos.Y();
    }
    gradient = (nhits * sumxy - sumx * sumy) / (nhits * sumx2 - sumx * sumx);
    TVector2 directionFromStart = TVector2(1,gradient).Unit();

    // backwards
    nhits = 0;
    sumx=0., sumy=0., sumx2=0., sumxy=0.;
    for (std::map<double,art::Ptr<recob::Hit> >::reverse_iterator hitIt = hitProjection.rbegin(); hitIt != hitProjection.rend(); ++hitIt) {
      if (std::distance(hitProjection.rbegin(),hitIt) > 5) break;
      ++nhits;
      pos = HitPosition(hitIt->second);
      sumx += pos.X();
      sumy += pos.Y();
      sumx2 += pos.X() * pos.X();
      sumxy += pos.X() * pos.Y();
    }
    gradient = (nhits * sumxy - sumx * sumy) / (nhits * sumx2 - sumx * sumx);
    TVector2 directionFromEnd = TVector2(1,gradient).Unit();

    // Count how often the distance increases
    int fromBeginning = 0, fromEnd = 0;
    double previousDistanceFromAxis = 0;
    int hitsAfterOffshoot = 0;
    TVector2 start = HitPosition(hitProjection.begin()->second);
    for (std::map<double,art::Ptr<recob::Hit> >::iterator hitIt = hitProjection.begin(); hitIt != hitProjection.end(); ++hitIt) {
      TVector2 pos = HitPosition(hitIt->second);
      TVector2 projPos = pos.Proj(directionFromStart);
      double distanceFromAxis = TMath::Sqrt(TMath::Power((projPos-start).Mod(),2) + TMath::Power(pos*directionFromStart,2));
      if (distanceFromAxis > previousDistanceFromAxis) {
	++fromBeginning;
	hitsAfterOffshoot = 0;
	previousDistanceFromAxis = distanceFromAxis;
      }
      else
	++hitsAfterOffshoot;
      if (hitsAfterOffshoot > 5) {
	hitsAfterOffshoot = 0;
	previousDistanceFromAxis = 0;
      }
    }

    previousDistanceFromAxis = 0;
    hitsAfterOffshoot = 0;
    TVector2 end = HitPosition(hitProjection.rbegin()->second);
    for (std::map<double,art::Ptr<recob::Hit> >::reverse_iterator hitIt = hitProjection.rbegin(); hitIt != hitProjection.rend(); ++hitIt) {
      TVector2 pos = HitPosition(hitIt->second);
      TVector2 projPos = pos.Proj(directionFromEnd);
      double distanceFromAxis = TMath::Sqrt(TMath::Power((projPos-start).Mod(),2) + TMath::Power(pos*directionFromStart,2));
      if (distanceFromAxis > previousDistanceFromAxis) {
	++fromEnd;
	hitsAfterOffshoot = 0;
	previousDistanceFromAxis = distanceFromAxis;
      }
      else
	++hitsAfterOffshoot;
      if (hitsAfterOffshoot > 5) {
	hitsAfterOffshoot = 0;
	previousDistanceFromAxis = 0;
      }
    }

    std::vector<art::Ptr<recob::Hit> > orderedHits;
    if (fromBeginning > fromEnd) {
      for (std::map<double,art::Ptr<recob::Hit> >::iterator hitIt = hitProjection.begin(); hitIt != hitProjection.end(); ++hitIt)
	orderedHits.push_back(hitIt->second);
      orderedShowerMap[planeHits->first] = std::make_pair(fromBeginning/(double)fromEnd, orderedHits);
    }
    else {
      for (std::map<double,art::Ptr<recob::Hit> >::reverse_iterator hitIt = hitProjection.rbegin(); hitIt != hitProjection.rend(); ++hitIt)
	orderedHits.push_back(hitIt->second);
      orderedShowerMap[planeHits->first] = std::make_pair(fromEnd/(double)fromBeginning, orderedHits);
    }

    

  }

}


//     // Find the track-like part of the shower
//     std::vector<art::Ptr<recob::Hit> > trackHits = FindShowerStart(planeHits->second);
//     //trackMap[planeHits->first] = track;
//     trackHitsMap[planeHits->first] = trackHits;

//     // Find a track object
//     art::Ptr<recob::Track> track;
//     for (std::vector<art::Ptr<recob::Hit> >::iterator trackHitIt = trackHits.begin(); trackHitIt != trackHits.end(); ++trackHitIt) {
//       std::vector<art::Ptr<recob::Track> > tracks = fmt.at(trackHitIt->key());
//       if (tracks.size() == 0)
//     	continue;
//       track = tracks.at(0);
//       break;
//     }
//     trackMap[planeHits->first] = track;

//     // Vertex
//     art::Ptr<recob::Hit> planevertex = *trackHits.begin();
//     vertexMap[planeHits->first] = planevertex;
//     std::cout << "Vertex is (" << HitCoordinates(planevertex).X() << ", " << HitCoordinates(planevertex).Y() << ")" << std::endl;

//   }

//   // Made a 3D track object for the start of the shower
//   trackHitsMap = CheckTrackHits(trackHitsMap);
//   art::Ptr<recob::Track> vertexTrack = MakeVertexTrack(trackHitsMap);

//   // // Find 3D vertex and direction
//   // art::Ptr<recob::Track> vertexTrack = FindVertexTrack(vertexMap, trackMap, trackHitsMap);
//   // if (vertexTrack.isNull()) return;
//   FindShowerStartDirection(vertexTrack, showerCentreMap, vertex, direction);

//   // Use the 3D vertex to resolve any problems in 2D
//   ProjectVertexIn2D(vertex, trackHitsMap, trackHitsBothEndsMap);

//   return;

// }

// art::Ptr<recob::Track> shower::EMShowerAlg::MakeVertexTrack(std::map<int,std::vector<art::Ptr<recob::Hit> > > const& trackHitsMap) {

//   /// Makes a 3D track to represent the start of the shower

//   art::Ptr<recob::Track> vertexTrack;

//   // First we need to find the two views which we will use to make this track object
//   // Don't want to make any assumptions on the number of planes in the detector, or the numbers of planes with hits on

//   // If there is only one plane then we can't do much
//   if (trackHitsMap.size() < 2)
//     return vertexTrack;

//   // Need to find the two planes to form the track using
//   std::vector<std::vector<art::Ptr<recob::Hit> > > showerPlanes;

//   // If there are only two planes then we can just make the shower using these
//   if (trackHitsMap.size() == 2)
//     for (std::map<int,std::vector<art::Ptr<recob::Hit> > >::const_iterator trackHits = trackHitsMap.begin(); trackHits != trackHitsMap.end(); ++trackHits)
//       showerPlanes.push_back(trackHits->second)

//   // If there are more, need to selec

//   // Make a map of the size of the tracks in each plane
//   std::map<int,std::vector<art::Ptr<recob::Hit> > > showerSizeMap;
//   for (std::map<int,std::vector<art::Ptr<recob::Hit> > >::const_iterator trackHits = trackHitsMap.begin(); trackHits != trackHitsMap.end(); ++trackHits)
//     showerSizeMap[trackHits->second.size()] = trackHits->second;

  

// }

// std::vector<art::Ptr<recob::Hit> > shower::EMShowerAlg::FindInitialTrack(std::vector<art::Ptr<recob::Hit> > const& shower) {

//   /// Finds the initial track like part of the shower in a particular view

//   // Find the charge-weighted centre (in cm) of this shower
//   TVector2 pos, chargePoint = TVector2(0,0);
//   double totalCharge = 0;
//   for (std::vector<art::Ptr<recob::Hit> >::const_iterator hit = shower.begin(); hit != shower.end(); ++hit) {
//     pos = HitPosition(*hit);
//     chargePoint += (*hit)->Integral() * pos;
//     totalCharge += (*hit)->Integral();
//   }
//   TVector2 centre = chargePoint / totalCharge;

//   // Find a rough shower 'direction'
//   int nhits = 0;
//   double sumx=0., sumy=0., sumx2=0., sumxy=0.;
//   for (std::vector<art::Ptr<recob::Hit> >::const_iterator hit = shower.begin(); hit != shower.end(); ++hit) {
//     ++nhits;
//     pos = HitPosition(*hit);
//     sumx += pos.X();
//     sumy += pos.Y();
//     sumx2 += pos.X() * pos.X();
//     sumxy += pos.X() * pos.Y();
//   }
//   double gradient = (nhits * sumxy - sumx * sumy) / (nhits * sumx2 - sumx * sumx);
//   TVector2 direction = TVector2(1,gradient).Unit();

//   // Find how far each hit (projected onto this axis) is from the centre
//   std::map<double,art::Ptr<recob::Hit> > hitProjection;
//   for (std::vector<art::Ptr<recob::Hit> >::const_iterator hit = shower.begin(); hit != shower.end(); ++hit) {
//     pos = HitPosition(*hit) - centre;
//     hitProjection[direction*pos] = *hit;
//   }

//   // Construct a line from the initial part of the shower

//   // forwards
//   nhits = 0;
//   sumx=0., sumy=0., sumx2=0., sumxy=0.;
//   for (std::map<double,art::Ptr<recob::Hit> >::iterator hitIt = hitProjection.begin(); hitIt != hitProjection.end(); ++hitIt) {
//     if (std::distance(hitProjection.begin(),hitIt) > 5) break;
//     ++nhits;
//     pos = HitPosition(hitIt->second);
//     sumx += pos.X();
//     sumy += pos.Y();
//     sumx2 += pos.X() * pos.X();
//     sumxy += pos.X() * pos.Y();
//   }
//   gradient = (nhits * sumxy - sumx * sumy) / (nhits * sumx2 - sumx * sumx);
//   TVector2 directionFromStart = TVector2(1,gradient).Unit();

//   // backwards
//   nhits = 0;
//   sumx=0., sumy=0., sumx2=0., sumxy=0.;
//   for (std::map<double,art::Ptr<recob::Hit> >::reverse_iterator hitIt = hitProjection.rbegin(); hitIt != hitProjection.rend(); ++hitIt) {
//     if (std::distance(hitProjection.rbegin(),hitIt) > 5) break;
//     ++nhits;
//     pos = HitPosition(hitIt->second);
//     sumx += pos.X();
//     sumy += pos.Y();
//     sumx2 += pos.X() * pos.X();
//     sumxy += pos.X() * pos.Y();
//   }
//   gradient = (nhits * sumxy - sumx * sumy) / (nhits * sumx2 - sumx * sumx);
//   TVector2 directionFromEnd = TVector2(1,gradient).Unit();

//   // Count how often the distance increases
//   int fromBeginning = 0, fromEnd = 0;
//   double previousDistanceFromAxis = 0;
//   int hitsAfterOffshoot = 0;
//   TVector2 start = HitPosition(hitProjection.begin()->second);
//   for (std::map<double,art::Ptr<recob::Hit> >::iterator hitIt = hitProjection.begin(); hitIt != hitProjection.end(); ++hitIt) {
//     TVector2 pos = HitPosition(hitIt->second);
//     TVector2 projPos = pos.Proj(directionFromStart);
//     double distanceFromAxis = TMath::Sqrt(TMath::Power((projPos-start).Mod(),2) + TMath::Power(pos*directionFromStart,2));
//     if (distanceFromAxis > previousDistanceFromAxis) {
//       ++fromBeginning;
//       hitsAfterOffshoot = 0;
//       previousDistanceFromAxis = distanceFromAxis;
//     }
//     else
//       ++hitsAfterOffshoot;
//     if (hitsAfterOffshoot > 5) {
//       hitsAfterOffshoot = 0;
//       previousDistanceFromAxis = 0;
//     }
//   }

//   previousDistanceFromAxis = 0;
//   hitsAfterOffshoot = 0;
//   TVector2 end = HitPosition(hitProjection.rbegin()->second);
//   for (std::map<double,art::Ptr<recob::Hit> >::reverse_iterator hitIt = hitProjection.rbegin(); hitIt != hitProjection.rend(); ++hitIt) {
//     TVector2 pos = HitPosition(hitIt->second);
//     TVector2 projPos = pos.Proj(directionFromEnd);
//     double distanceFromAxis = TMath::Sqrt(TMath::Power((projPos-start).Mod(),2) + TMath::Power(pos*directionFromStart,2));
//     if (distanceFromAxis > previousDistanceFromAxis) {
//       ++fromEnd;
//       hitsAfterOffshoot = 0;
//       previousDistanceFromAxis = distanceFromAxis;
//     }
//     else
//       ++hitsAfterOffshoot;
//     if (hitsAfterOffshoot > 5) {
//       hitsAfterOffshoot = 0;
//       previousDistanceFromAxis = 0;
//     }
//   }

//   int startWire = fromBeginning > fromEnd ? HitCoordinates(hitProjection.begin()->second).X() : HitCoordinates(hitProjection.rbegin()->second).X();

//   std::vector<art::Ptr<recob::Hit> > trackHits = Construct2DShowerStart(shower, startWire);

//   return trackHits;

// }

// void shower::EMShowerAlg::ProjectVertexIn2D(TVector3 const& vertex,
// 					    std::map<int,std::vector<art::Ptr<recob::Hit> > >& trackHitsMap,
// 					    std::map<int,std::pair<std::vector<art::Ptr<recob::Hit> >,std::vector<art::Ptr<recob::Hit> > > > const& trackHitsBothEndsMap) {

//   /// Projects the 3D direction into all the 2D views to make sure the correct end of the track is selected in each view

//   for (std::map<int,std::pair<std::vector<art::Ptr<recob::Hit> >, std::vector<art::Ptr<recob::Hit> > > >::const_iterator trackHitsIt = trackHitsBothEndsMap.begin();
//        trackHitsIt != trackHitsBothEndsMap.end();
//        ++trackHitsIt) {

//     TVector2 end1 = HitCoordinates(*trackHitsIt->second.first.begin());
//     TVector2 end2 = HitCoordinates(*trackHitsIt->second.second.begin());

//     TVector2 vertexProj = Project3DPointOntoPlane(vertex, trackHitsIt->first);

//     if ( (vertexProj - end1).Mod() < (vertexProj - end2).Mod() )
//       trackHitsMap[trackHitsIt->first] = trackHitsIt->second.first;
//     else
//       trackHitsMap[trackHitsIt->first] = trackHitsIt->second.second;

//   }

// }

// void shower::EMShowerAlg::FindShowerStartDirection(art::Ptr<recob::Track> const& vertexTrack,
// 						   std::map<int,TVector2> const& showerCentreMap,
// 						   TVector3& showerVertex,
// 						   TVector3& showerDirection) {

//   /// Finds the start of the shower, and its direction

//   TVector3 vertex = vertexTrack->Vertex();
//   TVector3 end = vertexTrack->End();

//   std::map<int,double> distanceToVertex;
//   std::map<int,double> distanceToEnd;

//   // Loop over all the planes and find the distance from the vertex and end projections to the centre in each plane
//   for (std::map<int,TVector2>::const_iterator showerCentreIt = showerCentreMap.begin(); showerCentreIt != showerCentreMap.end(); ++showerCentreIt) {

//     // Project the vertex and the end point onto this plane
//     TVector2 vertexProj = Project3DPointOntoPlane(vertex, showerCentreIt->first);
//     TVector2 endProj    = Project3DPointOntoPlane(end, showerCentreIt->first);

//     // Find the distance of each to the centre of the cluster
//     distanceToVertex[showerCentreIt->first] = (vertexProj - showerCentreIt->second).Mod();
//     distanceToEnd[showerCentreIt->first] = (endProj - showerCentreIt->second).Mod();

//   }

//   // Find the average distance to the vertex and the end across the planes
//   double avDistanceToVertex = 0, avDistanceToEnd = 0;
//   for (std::map<int,double>::iterator distanceToVertexIt = distanceToVertex.begin(); distanceToVertexIt != distanceToVertex.end(); ++distanceToVertexIt)
//     avDistanceToVertex += distanceToVertexIt->second;
//   avDistanceToVertex /= distanceToVertex.size();

//   for (std::map<int,double>::iterator distanceToEndIt = distanceToEnd.begin(); distanceToEndIt != distanceToEnd.end(); ++distanceToEndIt)
//     avDistanceToEnd += distanceToEndIt->second;
//   avDistanceToEnd /= distanceToEnd.size();

//   // Set the vertex and directions for this shower
//   if (avDistanceToVertex > avDistanceToEnd) {
//     showerVertex = vertex;
//     showerDirection = vertexTrack->VertexDirection();
//   }
//   else {
//     showerVertex = end;
//     showerDirection = (-1) * vertexTrack->VertexDirection();
//   }

//   return;

// }

// art::Ptr<recob::Track> shower::EMShowerAlg::FindVertexTrack(std::map<int,art::Ptr<recob::Hit> > const& vertexMap,
// 							    std::map<int,art::Ptr<recob::Track> > const& trackMap,
// 							    std::map<int,std::vector<art::Ptr<recob::Hit> > > const& trackHitsMap) {

//   /// Finds the 3D vertex and direction given the tracks associated with the 2D vertex

//   art::Ptr<recob::Track> vertexTrack;

//   // First, find out if two views agree on a track (normally they will)
//   std::map<int,std::vector<int> > trackIDToPlanes;
//   std::vector<int> planesWithTrack;
//   for (std::map<int,art::Ptr<recob::Track> >::const_iterator trackIt = trackMap.begin(); trackIt != trackMap.end(); ++trackIt)
//     if (!trackIt->second.isNull()) {
//       trackIDToPlanes[trackIt->second->ID()].push_back(trackIt->first);
//       planesWithTrack.push_back(trackIt->first);
//     }
//   for (std::map<int,std::vector<int> >::iterator trackVertexIt = trackIDToPlanes.begin(); trackVertexIt != trackIDToPlanes.end(); ++trackVertexIt)
//     if (trackVertexIt->second.size() > 1)
//       vertexTrack = trackMap.at(*trackVertexIt->second.begin());

//   // If there are no planes with a track reconstructed, don't carry on right now
//   if (planesWithTrack.size() == 0)
//     return vertexTrack;

//   // If they don't, try to use the third view (it exsits) to pick the correct track
//   if (vertexTrack.isNull() and trackMap.size() > 2) {
//     std::map<int,double> distanceOfVertexFromVertex;
//     for (std::map<int,art::Ptr<recob::Hit> >::const_iterator vertexIt = vertexMap.begin(); vertexIt != vertexMap.end(); ++vertexIt) {
//       if (std::find(planesWithTrack.begin(), planesWithTrack.end(), vertexIt->first) != planesWithTrack.end())
// 	continue;
//       for (std::vector<int>::iterator planesWithTrackIt = planesWithTrack.begin(); planesWithTrackIt != planesWithTrack.end(); ++planesWithTrackIt)
// 	distanceOfVertexFromVertex[*planesWithTrackIt] = ( TMath::Abs( HitCoordinates(vertexIt->second).Y() - HitCoordinates(vertexMap.at(*planesWithTrackIt)).Y() ) ) / HitCoordinates(vertexMap.at(*planesWithTrackIt)).Y();
//     }
//     std::vector<int> thirdViewClose;
//     for (std::map<int,double>::iterator distanceOfVertexFromVertexIt = distanceOfVertexFromVertex.begin(); distanceOfVertexFromVertexIt != distanceOfVertexFromVertex.end(); ++distanceOfVertexFromVertexIt)
//       if (distanceOfVertexFromVertexIt->second < 0.1)
// 	thirdViewClose.push_back(distanceOfVertexFromVertexIt->first);
//     if (thirdViewClose.size() == 1)
//       vertexTrack = trackMap.at(thirdViewClose.at(0));
//   }

//   // Finally, if all else fails, just pick the view with the longest initial track reconstructed in 2D
//   if (vertexTrack.isNull()) {
//     std::map<int,int> lengthOfTrackToPlane;
//     for (std::vector<int>::iterator planesWithTrackIt = planesWithTrack.begin(); planesWithTrackIt != planesWithTrack.end(); ++planesWithTrackIt)
//       lengthOfTrackToPlane[trackHitsMap.at(*planesWithTrackIt).size()] = *planesWithTrackIt;
//     vertexTrack = trackMap.at(lengthOfTrackToPlane.rbegin()->second);
//   }

//   return vertexTrack;

// }

// std::vector<art::Ptr<recob::Hit> > shower::EMShowerAlg::FindTrack(std::vector<art::Ptr<recob::Hit> > const& shower, TVector2 const& start, TVector2 const& end) {

//   /// Finds the track from the start of the shower

//   std::vector<art::Ptr<recob::Hit> > trackHits;

//   // Map of hit on each wire
//   std::map<int,std::vector<art::Ptr<recob::Hit> > > hitWires;
//   for (std::vector<art::Ptr<recob::Hit> >::const_iterator hit = shower.begin(); hit != shower.end(); ++hit)
//     hitWires[(int)HitCoordinates(*hit).X()].push_back(*hit);

//   // Call the track ended when there is more than one hit on a wire twice in three wires
//   std::vector<int> lastTwoWires = {0, 0};

//   // Find out which way to look!
//   int startWire = (int)start.X(), endWire = (int)end.X();
//   bool increasing;
//   if (startWire < endWire) increasing = true;
//   else increasing = false;

//   // Look through the hits from the start
//   if (increasing) {
//     for (int wire = startWire; wire < endWire; ++wire) {
//       int numberOfHitsOnThisWire;
//       if (hitWires.find(wire) != hitWires.end()) numberOfHitsOnThisWire = hitWires.at(wire).size();
//       else numberOfHitsOnThisWire = 0;
//       if (numberOfHitsOnThisWire >= 2 and (lastTwoWires.at(0) >= 2 or lastTwoWires.at(1) >= 2))
// 	break;
//       else {
// 	if (numberOfHitsOnThisWire)
// 	  for (std::vector<art::Ptr<recob::Hit> >::const_iterator hitWireIt = hitWires.at(wire).begin(); hitWireIt != hitWires.at(wire).end(); ++hitWireIt)
// 	    trackHits.push_back(*hitWireIt);
// 	lastTwoWires[0] = lastTwoWires[1];
// 	lastTwoWires[1] = numberOfHitsOnThisWire;
//       }
//     }
//   }

//   else {
//     for (int wire = startWire; wire > endWire; --wire) {
//       int numberOfHitsOnThisWire;
//       if (hitWires.find(wire) != hitWires.end()) numberOfHitsOnThisWire = hitWires.at(wire).size();
//       else numberOfHitsOnThisWire = 0;
//       if (numberOfHitsOnThisWire >= 2 and (lastTwoWires.at(0) >= 2 or lastTwoWires.at(1) >= 2))
// 	break;
//       else {
// 	if (numberOfHitsOnThisWire)
// 	  for (std::vector<art::Ptr<recob::Hit> >::const_iterator hitWireIt = hitWires.at(wire).begin(); hitWireIt != hitWires.at(wire).end(); ++hitWireIt)
// 	    trackHits.push_back(*hitWireIt);
// 	lastTwoWires[0] = lastTwoWires[1];
// 	lastTwoWires[1] = numberOfHitsOnThisWire;
//       }
//     }
//   }

//   if (trackHits.size() == 0)
//     trackHits.push_back(hitWires.begin()->second.at(0));

//   return trackHits;

// }

// std::vector<art::Ptr<recob::Hit> > shower::EMShowerAlg::Construct2DShowerStart(std::vector<art::Ptr<recob::Hit> > const& shower, int wire) {

//   /// Constructs a track to represent the initial part of a shower in a particular view, given the hits and the start wire

//   std::vector<art::Ptr<recob::Hit> > trackHits;

//   // Map of hit on each wire
//   std::map<int,std::vector<art::Ptr<recob::Hit> > > hitWires;
//   for (std::vector<art::Ptr<recob::Hit> >::const_iterator hit = shower.begin(); hit != shower.end(); ++hit)
//     hitWires[(int)HitCoordinates(*hit).X()].push_back(*hit);

//   // Call the track ended when there is more than one hit on a wire twice in three wires
//   std::vector<int> lastTwoWires = {0, 0};

//   // Find out which way to look!
//   int startWire = *hitWires.begin(), endWire = *hitWires.rbegin();
//   bool increasing = TMath::Abs(*hitWires.begin() - wire) < TMath::Abs(*hitWires.rbegin() - wire);

//   // Look through the hits from the start
//   if (increasing) {
//     for (int wire = startWire; wire < endWire; ++wire) {
//       int numberOfHitsOnThisWire;
//       if (hitWires.find(wire) != hitWires.end()) numberOfHitsOnThisWire = hitWires.at(wire).size();
//       else numberOfHitsOnThisWire = 0;
//       if (numberOfHitsOnThisWire >= 2 and (lastTwoWires.at(0) >= 2 or lastTwoWires.at(1) >= 2))
// 	break;
//       else {
// 	if (numberOfHitsOnThisWire)
// 	  for (std::vector<art::Ptr<recob::Hit> >::const_iterator hitWireIt = hitWires.at(wire).begin(); hitWireIt != hitWires.at(wire).end(); ++hitWireIt)
// 	    trackHits.push_back(*hitWireIt);
// 	lastTwoWires[0] = lastTwoWires[1];
// 	lastTwoWires[1] = numberOfHitsOnThisWire;
//       }
//     }
//   }

//   // And from the end
//   else {
//     for (int wire = startWire; wire > endWire; --wire) {
//       int numberOfHitsOnThisWire;
//       if (hitWires.find(wire) != hitWires.end()) numberOfHitsOnThisWire = hitWires.at(wire).size();
//       else numberOfHitsOnThisWire = 0;
//       if (numberOfHitsOnThisWire >= 2 and (lastTwoWires.at(0) >= 2 or lastTwoWires.at(1) >= 2))
// 	break;
//       else {
// 	if (numberOfHitsOnThisWire)
// 	  for (std::vector<art::Ptr<recob::Hit> >::const_iterator hitWireIt = hitWires.at(wire).begin(); hitWireIt != hitWires.at(wire).end(); ++hitWireIt)
// 	    trackHits.push_back(*hitWireIt);
// 	lastTwoWires[0] = lastTwoWires[1];
// 	lastTwoWires[1] = numberOfHitsOnThisWire;
//       }
//     }
//   }

//   if (trackHits.size() == 0)
//     trackHits.push_back(hitWires.begin()->second.at(0));

//   return trackHits;

// }

TVector2 shower::EMShowerAlg::HitCoordinates(art::Ptr<recob::Hit> const& hit) {

  /// Return the coordinates of this hit in global wire/tick space

  return TVector2(GlobalWire(hit->WireID()), hit->PeakTime());

}

TVector2 shower::EMShowerAlg::HitPosition(art::Ptr<recob::Hit> const& hit) {

  /// Return the coordinates of this hit in units of cm

  geo::PlaneID planeID = hit->WireID().planeID();

  return HitPosition(HitCoordinates(hit), planeID);

}

TVector2 shower::EMShowerAlg::HitPosition(TVector2 const& pos, geo::PlaneID planeID) {

  /// Return the coordinates of this hit in units of cm

  return TVector2(pos.X() * fGeom->WirePitch(planeID),
		  fDetProp->ConvertTicksToX(pos.Y(), planeID));

}

double shower::EMShowerAlg::GlobalWire(geo::WireID wireID) {

  /// Find the global wire position

  double wireCentre[3];
  fGeom->WireIDToWireGeo(wireID).GetCenter(wireCentre);

  double globalWire;
  if (fGeom->SignalType(wireID) == geo::kInduction) {
    if (wireID.TPC % 2 == 0) globalWire = fGeom->WireCoordinate(wireCentre[1], wireCentre[2], wireID.Plane, 0, wireID.Cryostat);
    else globalWire = fGeom->WireCoordinate(wireCentre[1], wireCentre[2], wireID.Plane, 1, wireID.Cryostat);
  }
  else {
    if (wireID.TPC % 2 == 0) globalWire = wireID.Wire + ((wireID.TPC/2) * fGeom->Nwires(wireID.Plane, 0, wireID.Cryostat));
    else globalWire = wireID.Wire + ((int)(wireID.TPC/2) * fGeom->Nwires(wireID.Plane, 1, wireID.Cryostat));
  }

  return globalWire;

}

TVector2 shower::EMShowerAlg::Project3DPointOntoPlane(TVector3 const& point, unsigned int plane) {

  /// Projects a 3D point onto a 2D plane

  double pointPosition[3] = {point.X(), point.Y(), point.Z()};

  return TVector2(fGeom->WireCoordinate(point.Y(), point.Z(), plane, fGeom->FindTPCAtPosition(pointPosition).TPC % 2, 0),
		  fDetProp->ConvertXToTicks(point.X(), plane, fGeom->FindTPCAtPosition(pointPosition).TPC % 2, 0));

}

recob::Shower shower::EMShowerAlg::MakeShower(art::PtrVector<recob::Hit> const& hits,
					      art::Ptr<recob::Track> const& initialTrack,
					      std::vector<art::Ptr<recob::Hit> > const& initialTrackHits) {

  /// Makes a recob::Shower object given the hits in the shower and the initial track-like part

  // Find the shower hits on each plane
  std::map<int,std::vector<art::Ptr<recob::Hit> > > planeHitsMap;
  for (art::PtrVector<recob::Hit>::const_iterator hit = hits.begin(); hit != hits.end(); ++hit)
    planeHitsMap[(*hit)->View()].push_back(*hit);

  // Find the initial track hits on each plane
  std::map<int,std::vector<art::Ptr<recob::Hit> > > initialHitsMap;
  for (std::vector<art::Ptr<recob::Hit> >::const_iterator initialHitIt = initialTrackHits.begin(); initialHitIt != initialTrackHits.end(); ++initialHitIt)
    initialHitsMap[(*initialHitIt)->View()].push_back(*initialHitIt);

  int bestPlane = -1;
  unsigned int highestNumberOfHits = 0;
  std::vector<double> totalEnergy(3), totalEnergyError(3), dEdx(3), dEdxError(3);

  // Look at each of the planes
  for (unsigned int plane = 0; plane < fGeom->MaxPlanes(); ++plane) {

    // If hits exist on this plane...
    if (planeHitsMap.count(plane) != 0) {
      dEdx.push_back(FinddEdx(initialHitsMap.at(plane), initialTrack));
      totalEnergy.push_back(fShowerEnergyAlg.ShowerEnergy(planeHitsMap.at(plane), plane));
      if (planeHitsMap.at(plane).size() > highestNumberOfHits) {
	bestPlane = plane;
	highestNumberOfHits = planeHitsMap.at(plane).size();
      }
    }

    // If there are no hits on this plane
    else {
      dEdx.push_back(0);
      totalEnergy.push_back(0);
    }
  }

  TVector3 direction = initialTrack->VertexDirection(), directionError;
  TVector3 showerStart = initialTrack->Vertex(), showerStartError;

  std::cout << "Best plane is " << bestPlane << std::endl;
  std::cout << "dE/dx for each plane is: " << dEdx[0] << ", " << dEdx[1] << " and " << dEdx[2] << std::endl;
  std::cout << "Total energy for each plane is: " << totalEnergy[0] << ", " << totalEnergy[1] << " and " << totalEnergy[2] << std::endl;
  std::cout << "The shower start is " << std::endl;
  showerStart.Print();

  return recob::Shower(direction, directionError, showerStart, showerStartError, totalEnergy, totalEnergyError, dEdx, dEdxError, bestPlane);

}

double shower::EMShowerAlg::FinddEdx(std::vector<art::Ptr<recob::Hit> > const& trackHits, art::Ptr<recob::Track> const& track) {

  /// Finds dE/dx for the track given a set of hits

  double totalCharge = 0, totalDistance = 0, avHitTime = 0;
  unsigned int nHits = 0;

  // Get the pitch
  double pitch = 0;
  try { pitch = track->PitchInView(trackHits.at(0)->View()); }
  catch(...) { pitch = 0; }

  // Deal with large pitches
  if (pitch > fdEdxTrackLength) {
    double dEdx = fCalorimetryAlg.dEdx_AREA(*trackHits.begin(), pitch);
    return dEdx;
  }

  for (std::vector<art::Ptr<recob::Hit> >::const_iterator trackHitIt = trackHits.begin(); trackHitIt != trackHits.end(); ++trackHitIt) {
    if (totalDistance + pitch < fdEdxTrackLength) {
      totalDistance += pitch;
      totalCharge += (*trackHitIt)->Integral();
      avHitTime += (*trackHitIt)->PeakTime();
      ++nHits;
      // double dEdx = fCalorimetryAlg.dEdx_AREA(shower.at(*trackHitIt), pitch);
      // std::cout << "After " << totalDistance << ", the dE/dx is " << dEdx << std::endl;
    }
  }

  avHitTime /= (double)nHits;

  double dQdx = totalCharge / totalDistance;
  double dEdx = fCalorimetryAlg.dEdx_AREA(dQdx, avHitTime, trackHits.at(0)->WireID().Plane);

  return dEdx;

}
