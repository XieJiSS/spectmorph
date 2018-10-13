// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#include "smmorphwavsourcemodule.hh"
#include "smmorphwavsource.hh"
#include "smmorphplan.hh"
#include "smleakdebugger.hh"
#include "../instedit/sminstrument.hh"
#include "../instedit/smwavsetbuilder.hh"
#include <glib.h>

using namespace SpectMorph;

using std::string;
using std::vector;
using std::max;

static LeakDebugger leak_debugger ("SpectMorph::MorphWavSourceModule");

static float
freq_to_note (float freq)
{
  return 69 + 12 * log (freq / 440) / log (2);
}

void
InstrumentSource::retrigger (int channel, float freq, int midi_velocity, float mix_freq)
{
  Audio  *best_audio = nullptr;
  float   best_diff  = 1e10;

  // we can not delete the old wav_set alive between retrigger() invocations
  //  - LiveDecoder may keep a pointer to contained Audio* entries (which die if the WavSet is freed)
  //
  if (next_wav_set)
    wav_set = next_wav_set;

  if (wav_set)
    {
      float note = freq_to_note (freq);
      for (vector<WavSetWave>::iterator wi = wav_set->waves.begin(); wi != wav_set->waves.end(); wi++)
        {
          Audio *audio = wi->audio;
          if (audio && wi->channel == channel &&
                       wi->velocity_range_min <= midi_velocity &&
                       wi->velocity_range_max >= midi_velocity)
            {
              float audio_note = freq_to_note (audio->fundamental_freq);
              if (fabs (audio_note - note) < best_diff)
                {
                  best_diff = fabs (audio_note - note);
                  best_audio = audio;
                }
            }
        }
    }
  active_audio = best_audio;
}

Audio*
InstrumentSource::audio()
{
  return active_audio;
}

AudioBlock *
InstrumentSource::audio_block (size_t index)
{
  if (active_audio && index < active_audio->contents.size())
    return &active_audio->contents[index];
  else
    return nullptr;
}

void
InstrumentSource::update_wav_set (std::shared_ptr<WavSet> next)
{
  next_wav_set = next;
}


MorphWavSourceModule::MorphWavSourceModule (MorphPlanVoice *voice) :
  MorphOperatorModule (voice)
{
  leak_debugger.add (this);
}

MorphWavSourceModule::~MorphWavSourceModule()
{
  leak_debugger.del (this);
}

LiveDecoderSource *
MorphWavSourceModule::source()
{
  return &my_source;
}

void
MorphWavSourceModule::set_config (MorphOperator *op)
{
  MorphWavSource *source = dynamic_cast<MorphWavSource *> (op);

  static int64 iid = 0;
  static std::shared_ptr<WavSet> builder_wav_set;

  if (iid != source->instrument_id())
    {
      printf ("MorphWavSourceModule::set_config: using instrument=%s source=%ld\n", source->instrument().c_str(), source->instrument_id());

      Instrument inst;
      inst.load (source->instrument());

      WavSetBuilder builder (&inst);
      builder.run();
      builder_wav_set = std::make_shared<WavSet>();
      builder.get_result (*builder_wav_set);

      iid = source->instrument_id();
    }
  my_source.update_wav_set (builder_wav_set);
}

#include "../instedit/smwavsetbuilder.cc"
