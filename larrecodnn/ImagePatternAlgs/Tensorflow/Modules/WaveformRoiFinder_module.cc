////////////////////////////////////////////////////////////////////////
// Class:       WaveformRoiFinder
// Plugin Type: producer (art v3_05_00)
// File:        WaveformRoiFinder_module.cc
//
// Authors:     Mike Wang (mwang@fnal.gov)
//              Lorenzo Uboldi (uboldi.lorenzo@gmail.com)
//              Tingjun Yang (tjyang@fnal.gov)
//
// Generated at Fri Apr 10 23:30:12 2020 by Tingjun Yang using cetskelgen
// from cetlib version v3_10_00.
// Based on the Analyzer module written by Mike Wang.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Utilities/make_tool.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "canvas/Utilities/InputTag.h"

#include "lardataobj/RecoBase/Wire.h"
#include "lardataobj/RawData/RawDigit.h"
#include "lardataobj/RawData/raw.h"
#include "larcore/Geometry/Geometry.h"
#include "larrecodnn/ImagePatternAlgs/Tensorflow/WaveformRecogTools/IWaveformRecog.h"

#include <memory>

namespace nnet {
  class WaveformRoiFinder;
}


class nnet::WaveformRoiFinder : public art::EDProducer {
public:
  explicit WaveformRoiFinder(fhicl::ParameterSet const& p);
  // The compiler-generated destructor is fine for non-base
  // classes without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  WaveformRoiFinder(WaveformRoiFinder const&) = delete;
  WaveformRoiFinder(WaveformRoiFinder&&) = delete;
  WaveformRoiFinder& operator=(WaveformRoiFinder const&) = delete;
  WaveformRoiFinder& operator=(WaveformRoiFinder&&) = delete;

  // Required functions.
  void produce(art::Event& e) override;

private:

  unsigned int fWaveformSize;		// Full waveform size

  art::InputTag fRawProducerLabel;
  art::InputTag fWireProducerLabel;
  std::unique_ptr<wavrec_tool::IWaveformRecog> fWaveformRecogTool;

};


nnet::WaveformRoiFinder::WaveformRoiFinder(fhicl::ParameterSet const& p)
  : EDProducer{p}  ,
  fWaveformSize(p.get<unsigned int>("WaveformSize", 6000)),
  fRawProducerLabel(p.get<art::InputTag>("RawProducerLabel","")),
  fWireProducerLabel(p.get<art::InputTag>("WireProducerLabel",""))
{

  if (fRawProducerLabel.empty() && fWireProducerLabel.empty()){
    throw cet::exception("WaveformRoiFinder") << "Both RawProducerLabel and WireProducerLabel are empty";
  }

  if ((!fRawProducerLabel.empty()) && (!fWireProducerLabel.empty())){
    throw cet::exception("WaveformRoiFinder") << "Only one of RawProducerLabel and WireProducerLabel should be set";
  }

  // Signal/Noise waveform recognition tool
  fWaveformRecogTool = art::make_tool<wavrec_tool::IWaveformRecog>(p.get<fhicl::ParameterSet>("WaveformRecog"));

  produces< std::vector<recob::Wire> >();

}

void nnet::WaveformRoiFinder::produce(art::Event& e)
{

  std::cout<<fRawProducerLabel<<std::endl;

  art::Handle< std::vector<raw::RawDigit> > rawListHandle;
  std::vector<art::Ptr<raw::RawDigit> > rawlist;
  if (e.getByLabel(fRawProducerLabel, rawListHandle))
    art::fill_ptr_vector(rawlist, rawListHandle);

  //std::cout<<rawlist.size()<<std::endl;

  art::Handle< std::vector<recob::Wire> > wireListHandle;
  std::vector<art::Ptr<recob::Wire> > wirelist;
  if (e.getByLabel(fWireProducerLabel, wireListHandle))
    art::fill_ptr_vector(wirelist, wireListHandle);

  //auto wireHandle = e.getValidHandle< std::vector<recob::Wire> >(fWireProducerLabel);

  std::unique_ptr<std::vector<recob::Wire> > outwires(new std::vector<recob::Wire>);

  auto const* geo = lar::providerFrom<geo::Geometry>();

  //##############################
  //### Looping over the wires ###
  //##############################
  //for(size_t wireIter = 0; wireIter < wireHandle->size(); wireIter++){
  //std::cout<<"size = "<<(rawlist.empty()?wirelist.size():rawlist.size())<<std::endl;
  for (unsigned int ich = 0; ich < (rawlist.empty()?wirelist.size():rawlist.size()); ++ich){
    
    std::vector<float> inputsignal(fWaveformSize);

    if (!wirelist.empty()){
      const auto & wire = wirelist[ich];
      //art::Ptr<recob::Wire>   wire(wireHandle, wireIter);
      const auto & signal = wire->Signal();

      for (size_t itck = 0; itck < inputsignal.size(); ++itck){
        inputsignal[itck] = signal[itck];
      }
    }
    else if (!rawlist.empty()){
      //std::cout<<ich<<std::endl;
      const auto & digitVec = rawlist[ich];
      std::vector<short> rawadc(fWaveformSize);
      raw::Uncompress(digitVec->ADCs(), rawadc, digitVec->GetPedestal(), digitVec->Compression());
      for (size_t itck = 0; itck < rawadc.size(); ++itck){
        inputsignal[itck] = rawadc[itck] - digitVec->GetPedestal();
      }
    }

    // ... use waveform recognition CNN to perform inference on each window
    std::vector<bool>inroi(fWaveformSize,false);
    inroi = fWaveformRecogTool->findROI(inputsignal);

    std::vector<float> sigs;
    int lastsignaltick = -1;
    int roistart = -1;

    recob::Wire::RegionsOfInterest_t rois(fWaveformSize);

    for (size_t i = 0; i<fWaveformSize; ++i){
      if (inroi[i]){
        if (sigs.empty()){
          sigs.push_back(inputsignal[i]);
          lastsignaltick = i;
          roistart = i;
        }
        else{
          if (int(i)!=lastsignaltick+1){
            rois.add_range(roistart, std::move(sigs));
            sigs.clear();
            sigs.push_back(inputsignal[i]);
            lastsignaltick = i;
            roistart = i;
          }
          else {
            sigs.push_back(inputsignal[i]);
            lastsignaltick = i;
          }
        }
      }
    }
    if (!sigs.empty()){
      rois.add_range(roistart, std::move(sigs));
    }
    if (!wirelist.empty()){
      outwires->emplace_back(recob::Wire(rois, wirelist[ich]->Channel(), wirelist[ich]->View()));
    }
    else if (!rawlist.empty()){
      outwires->emplace_back(recob::Wire(rois, rawlist[ich]->Channel(), geo->View(rawlist[ich]->Channel())));
    }
  }

  e.put(std::move(outwires));

}

DEFINE_ART_MODULE(nnet::WaveformRoiFinder)