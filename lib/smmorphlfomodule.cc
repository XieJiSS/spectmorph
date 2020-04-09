// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#include "smmorphlfomodule.hh"
#include "smmorphlfo.hh"
#include "smmorphplan.hh"
#include "smwavsetrepo.hh"
#include "smleakdebugger.hh"
#include "smmorphplanvoice.hh"
#include "smmorphplansynth.hh"
#include "smmath.hh"

using namespace SpectMorph;

using std::string;
using std::vector;
using std::max;

static LeakDebugger leak_debugger ("SpectMorph::MorphLFOModule");

MorphLFOModule::MorphLFOModule (MorphPlanVoice *voice) :
  MorphOperatorModule (voice)
{
  leak_debugger.add (this);

  shared_state = NULL;
}

MorphLFOModule::~MorphLFOModule()
{
  leak_debugger.del (this);
}

static double
normalize_phase (double phase)
{
  return fmod (phase + 1, 1);
}

void
MorphLFOModule::set_config (MorphOperator *op)
{
  MorphLFO *lfo = dynamic_cast<MorphLFO *> (op);

  frequency = lfo->frequency();
  depth = lfo->depth();
  center = lfo->center();
  start_phase = lfo->start_phase();
  sync_voices = lfo->sync_voices();
  wave_type = lfo->wave_type();
  beat_sync = lfo->beat_sync();

  MorphPlanSynth *synth = morph_plan_voice->morph_plan_synth();
  if (synth)
    {
      shared_state = dynamic_cast<SharedState *> (synth->shared_state (op));
      if (!shared_state)
        {
          shared_state = new SharedState();
          restart_lfo (shared_state->global_lfo_state, /* start from zero time */ TimeInfo());
          synth->set_shared_state (op, shared_state);
        }
    }
}

float
MorphLFOModule::value()
{
  TimeInfo time = time_info();

  if (sync_voices)
    {
      auto lfo_state = shared_state->global_lfo_state;
      if (time.time_ms > lfo_state.last_time_ms)
        update_lfo_value (lfo_state, time.time_ms - lfo_state.last_time_ms, time.ppq_pos);

      return lfo_state.value;
    }
  else
    {
      if (time.time_ms > local_lfo_state.last_time_ms)
        {
          update_lfo_value (local_lfo_state, time.time_ms - local_lfo_state.last_time_ms, time.ppq_pos);
          local_lfo_state.last_time_ms = time.time_ms;
        }
      return local_lfo_state.value;
    }
}

void
MorphLFOModule::reset_value (const TimeInfo& time_info)
{
  restart_lfo (local_lfo_state, time_info);
  local_lfo_state.last_time_ms = time_info.time_ms;
}

void
MorphLFOModule::restart_lfo (LFOState& state, const TimeInfo& time_info)
{
  state.phase = normalize_phase (start_phase / 360);
  state.last_random_value = random_gen()->random_double_range (-1, 1);
  state.random_value = random_gen()->random_double_range (-1, 1);
  /* compute initial value */
  update_lfo_value (state, 0, 0);
  state.last_ppq_pos = time_info.ppq_pos;
}

void
MorphLFOModule::update_lfo_value (LFOState& state, double time_ms, double ppq_pos)
{
  if (beat_sync == MorphLFO::BEAT_SYNC_OFF)
    {
      state.phase += time_ms / 1000 * frequency;
    }
  else
    {
      double factor = 1;
      switch (beat_sync)
        {
          case MorphLFO::BEAT_SYNC_1_1: factor = 1.0 / 1.0;
                                        break;
          case MorphLFO::BEAT_SYNC_1_2: factor = 1.0 / 2.0;
                                        break;
          case MorphLFO::BEAT_SYNC_1_4: factor = 1.0 / 4.0;
                                        break;
          case MorphLFO::BEAT_SYNC_OFF: g_assert_not_reached();
        }
      factor *= 4; // <- tempo is relative to quarter notes
      if (state.last_ppq_pos > ppq_pos)
        {
          state.phase = ppq_pos / factor;
          state.phase += 1; /* force random retrigger */
        }
      else
        state.phase += (ppq_pos - state.last_ppq_pos) / factor;
      state.last_ppq_pos = ppq_pos;
    }
  if (state.phase > 1)
    {
      state.last_random_value = state.random_value;
      state.random_value = random_gen()->random_double_range (-1, 1);
    }
  state.phase = normalize_phase (state.phase);

  if (wave_type == MorphLFO::WAVE_SINE)
    {
      state.value = sin (state.phase * M_PI * 2);
    }
  else if (wave_type == MorphLFO::WAVE_TRIANGLE)
    {
      if (state.phase < 0.25)
        {
          state.value = 4 * state.phase;
        }
      else if (state.phase < 0.75)
        {
          state.value = (state.phase - 0.5) * -4;
        }
      else
        {
          state.value = 4 * (state.phase - 1);
        }
    }
  else if (wave_type == MorphLFO::WAVE_SAW_UP)
    {
      state.value = -1 + 2 * state.phase;
    }
  else if (wave_type == MorphLFO::WAVE_SAW_DOWN)
    {
      state.value = 1 - 2 * state.phase;
    }
  else if (wave_type == MorphLFO::WAVE_SQUARE)
    {
      if (state.phase < 0.5)
        state.value = -1;
      else
        state.value = 1;
    }
  else if (wave_type == MorphLFO::WAVE_RANDOM_SH)
    {
      state.value = state.random_value;
    }
  else if (wave_type == MorphLFO::WAVE_RANDOM_LINEAR)
    {
      state.value = state.last_random_value * (1 - state.phase) + state.random_value * state.phase;
    }
  else
    {
      g_assert_not_reached();
    }

  state.value = state.value * depth + center;
  state.value = CLAMP (state.value, -1.0, 1.0);
}

void
MorphLFOModule::update_shared_state (const TimeInfo& time_info)
{
  double time_ms = time_info.time_ms;
  if (time_ms > shared_state->global_lfo_state.last_time_ms)
    {
      update_lfo_value (shared_state->global_lfo_state, time_ms - shared_state->global_lfo_state.last_time_ms, time_info.ppq_pos);
      shared_state->global_lfo_state.last_time_ms = time_ms;
    }
}
