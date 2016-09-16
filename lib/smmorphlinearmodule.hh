// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#ifndef SPECTMORPH_MORPH_LINEAR_MODULE_HH
#define SPECTMORPH_MORPH_LINEAR_MODULE_HH

#include "smmorphplan.hh"
#include "smmorphoutput.hh"
#include "smmorphoperatormodule.hh"
#include "smmorphlinear.hh"

namespace SpectMorph
{

class MorphLinearModule : public MorphOperatorModule
{
  MorphOperatorModule *left_mod;
  MorphOperatorModule *right_mod;
  MorphOperatorModule *control_mod;
  float                morphing;
  bool                 db_linear;
  bool                 use_lpc;

  MorphLinear::ControlType control_type;

  Audio                audio;
  AudioBlock           audio_block;

  struct MySource : public LiveDecoderSource
  {
    MorphLinearModule *module;

    void interp_mag_one (double interp, uint16_t *left, uint16_t *right);
    void retrigger (int channel, float freq, int midi_velocity, float mix_freq);
    Audio* audio();
    AudioBlock *audio_block (size_t index);
  } my_source;

public:
  MorphLinearModule (MorphPlanVoice *voice);
  ~MorphLinearModule();

  void set_config (MorphOperator *op);
  LiveDecoderSource *source();
};

}

#endif
