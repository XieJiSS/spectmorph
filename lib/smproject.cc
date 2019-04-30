// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#include "smproject.hh"
#include "smmidisynth.hh"
#include "smsynthinterface.hh"
#include "smmorphoutputmodule.hh"
#include "smzip.hh"
#include "smmemout.hh"

using namespace SpectMorph;

using std::string;
using std::vector;

void
ControlEventVector::take (SynthControlEvent *ev)
{
  // we'd rather run destructors in non-rt part of the code
  if (clear)
    {
      events.clear();
      clear = false;
    }

  events.emplace_back (ev);
}

void
ControlEventVector::run_rt (Project *project)
{
  if (!clear)
    {
      for (const auto& ev : events)
        ev->run_rt (project);

      clear = true;
    }
}

void
Project::try_update_synth()
{
  // handle synth updates (if locking is possible without blocking)
  //  - apply new parameters
  //  - process events
  if (m_synth_mutex.try_lock())
    {
      m_control_events.run_rt (this);
      m_out_events = m_midi_synth->inst_edit_synth()->take_out_events();
      m_voices_active = m_midi_synth->active_voice_count() > 0;

      m_synth_mutex.unlock();
    }
}

void
Project::synth_take_control_event (SynthControlEvent *event)
{
  std::lock_guard<std::mutex> lg (m_synth_mutex);
  m_control_events.take (event);
}

void
Project::rebuild (int inst_id)
{
  Instrument *instrument = instrument_map[inst_id].get();

  if (!instrument)
    return;

  WavSetBuilder *builder = new WavSetBuilder (instrument, /* keep_samples */ false);
  m_builder_thread.add_job (builder,
    [this, inst_id] (WavSet *wav_set)
      {
        synth_interface()->emit_add_rebuild_result (inst_id, wav_set);
      });
}

void
Project::add_rebuild_result (int inst_id, WavSet *wav_set)
{
  size_t s = inst_id + 1;
  if (s > wav_sets.size())
    wav_sets.resize (s);

  wav_sets[inst_id] = std::shared_ptr<WavSet> (wav_set);
}

int
Project::add_instrument()
{
  int inst_id = 1;

  while (instrument_map[inst_id].get()) /* find first free slot */
    inst_id++;

  instrument_map[inst_id].reset (new Instrument());
  return inst_id;
}

Instrument *
Project::get_instrument (int inst_id)
{
  return instrument_map[inst_id].get();
}

std::shared_ptr<WavSet>
Project::get_wav_set (int inst_id)
{
  if (size_t (inst_id) < wav_sets.size())
    return wav_sets[inst_id];
  else
    return nullptr;
}

vector<string>
Project::notify_take_events()
{
  std::lock_guard<std::mutex> lg (m_synth_mutex);
  return std::move (m_out_events);
}

SynthInterface *
Project::synth_interface() const
{
  return m_synth_interface.get();
}

MidiSynth *
Project::midi_synth() const
{
  return m_midi_synth.get();
}

Project::Project()
{
  m_morph_plan = new MorphPlan (*this);
  m_morph_plan->load_default();

  connect (m_morph_plan->signal_plan_changed, this, &Project::on_plan_changed);

  m_synth_interface.reset (new SynthInterface (this));
}

void
Project::set_mix_freq (double mix_freq)
{
  // not rt safe, needs to be called when synthesis thread is not running
  m_midi_synth.reset (new MidiSynth (mix_freq, 64));
  m_mix_freq = mix_freq;

  // FIXME: can this cause problems if an old plan change control event remained
  m_midi_synth->update_plan (m_morph_plan->clone());
  m_midi_synth->set_gain (db_to_factor (m_volume));
}

void
Project::on_plan_changed()
{
  MorphPlanPtr plan = m_morph_plan->clone();

  // this might take a while, and cannot be done in synthesis thread
  MorphPlanSynth mp_synth (m_mix_freq);
  MorphPlanVoice *mp_voice = mp_synth.add_voice();
  mp_synth.update_plan (plan);

  MorphOutputModule *om = mp_voice->output();
  if (om)
    {
      om->retrigger (0, 440, 1);
      float s;
      float *values[1] = { &s };
      om->process (1, values, 1);
    }

  // FIXME: refptr is locking (which is not too good)
  m_synth_interface->emit_update_plan (plan);
}

bool
Project::voices_active()
{
  std::lock_guard<std::mutex> lg (m_synth_mutex);
  return m_voices_active;
}

MorphPlanPtr
Project::morph_plan() const
{
  return m_morph_plan;
}

double
Project::volume() const
{
  return m_volume;
}

void
Project::set_volume (double volume)
{
  m_volume = volume;
  m_synth_interface->emit_update_gain (db_to_factor (m_volume));

  signal_volume_changed (m_volume);
}

Error
Project::save (const std::string& filename)
{
  ZipWriter zip (filename); // FIXME: handle I/O errors

  vector<unsigned char> data;
  MemOut mo (&data);
  m_morph_plan->save (&mo);

  zip.add ("plan.smplan", data);
  // FIXME: check if needed by current morph plan
  for (const auto& inst : instrument_map)
    {
      ZipWriter mem_zip;

      string inst_file = string_printf ("instrument%d.sminst", inst.first);
      inst.second->save (mem_zip);
      zip.add (inst_file, mem_zip.data());
    }

  return Error::NONE;
}
