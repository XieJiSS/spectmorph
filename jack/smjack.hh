// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#ifndef SPECTMORPH_JACK_HH
#define SPECTMORPH_JACK_HH

#include "smmorphplanwindow.hh"
#include "smmorphplancontrol.hh"
#include "smmorphplansynth.hh"
#include "smmorphplanvoice.hh"
#include "smmidisynth.hh"
#include "smled.hh"

#include <jack/jack.h>

namespace SpectMorph
{

class JackSynth : public SynthInterface
{
protected:
  double                        jack_mix_freq;
  jack_port_t                  *input_port;
  std::vector<jack_port_t *>    output_ports;
  std::vector<jack_port_t *>    control_ports;

  double                        m_volume;

  MidiSynth                    *midi_synth;
  Project                      *m_project;

  MorphPlanPtr                  m_new_plan;
  double                        m_new_volume;
  bool                          m_voices_active;

  std::vector<std::string>      m_out_events;

public:
  JackSynth (jack_client_t *client, Project *project);
  ~JackSynth();

  void preinit_plan (MorphPlanPtr plan);
  void change_plan (MorphPlanPtr plan);
  void change_volume (double new_volume);
  bool voices_active();
  int  process (jack_nframes_t nframes);

  Project *get_project() override;
  std::vector<std::string> notify_take_events();
};

class JackControl : public SignalReceiver
{
  MorphPlanControl  *m_control_widget;
  JackSynth         *synth;
  MorphPlanPtr       morph_plan;

public:
  JackControl (MorphPlanPtr plan, MorphPlanWindow& window, MorphPlanControl *control_widget, JackSynth *synth);

  void update_led();

/* slots: */
  void on_plan_changed();
  void on_volume_changed (double d);
};

}

#endif
