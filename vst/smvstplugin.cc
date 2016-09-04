/*
 *  VST Plugin is under GPL 2 or later, since it is based on Vestige (GPL) and amsynth_vst (GPL)
 *
 *  Copyright (c) 2008-2015 Nick Dowell
 *  Copyright (c) 2016 Stefan Westerfeld
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "vestige/aeffectx.h"
#include "smutils.hh"
#include "smmorphplan.hh"
#include "smmorphplansynth.hh"
#include "smmidisynth.hh"
#include "smmain.hh"
#include "smvstui.hh"
#include "smvstplugin.hh"
#include "smmorphoutputmodule.hh"

#include <QMutex>
#include <QApplication>

// from http://www.asseca.org/vst-24-specs/index.html
#define effGetParamLabel        6
#define effGetParamDisplay      7
#define effGetChunk             23
#define effSetChunk             24
#define effCanBeAutomated       26
#define effGetOutputProperties  34
#define effGetTailSize          52
#define effGetMidiKeyName       66
#define effBeginLoadBank        75
#define effFlagsProgramChunks   (1 << 5)

#define DEBUG 1

using namespace SpectMorph;

using std::string;

static FILE *debug_file = NULL;
QMutex       debug_mutex;

static void
debug (const char *fmt, ...)
{
  if (DEBUG)
    {
      QMutexLocker locker (&debug_mutex);

      if (!debug_file)
        debug_file = fopen ("/tmp/smvstplugin.log", "w");

      va_list ap;

      va_start (ap, fmt);
      fprintf (debug_file, "%s", string_vprintf (fmt, ap).c_str());
      va_end (ap);
      fflush (debug_file);
    }
}

void
VstPlugin::preinit_plan (MorphPlanPtr plan)
{
  // this might take a while, and cannot be used in RT callback
  MorphPlanSynth mp_synth (mix_freq);
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
}

VstPlugin::VstPlugin (audioMasterCallback master, AEffect *aeffect) :
  audioMaster (master),
  aeffect (aeffect),
  plan (new MorphPlan()),
  morph_plan_synth (nullptr),
  midi_synth (nullptr),
  ui (new VstUI ("/home/stefan/lv2.smplan", this))
{
  audioMaster = master;

  string filename = "/home/stefan/lv2.smplan";
  GenericIn *in = StdioIn::open (filename);
  if (!in)
    {
      g_printerr ("Error opening '%s'.\n", filename.c_str());
      exit (1);
    }
  plan->load (in);
  delete in;

  parameters.push_back (Parameter ("Control #1", 0, -1, 1));
  parameters.push_back (Parameter ("Control #2", 0, -1, 1));
  parameters.push_back (Parameter ("Volume", -6, -48, 12, "dB"));

  // initialize mix_freq with something, so that the plugin doesn't crash if the host never calls SetSampleRate
  set_mix_freq (48000);
}

VstPlugin::~VstPlugin()
{
  delete ui;
  ui = nullptr;

  if (morph_plan_synth)
    {
      delete morph_plan_synth;
      morph_plan_synth = nullptr;
    }
  if (midi_synth)
    {
      delete midi_synth;
      midi_synth = nullptr;
    }
}

void
VstPlugin::change_plan (MorphPlanPtr plan)
{
  preinit_plan (plan);

  QMutexLocker locker (&m_new_plan_mutex);
  m_new_plan = plan;
}

void
VstPlugin::get_parameter_name (Param param, char *out, size_t len) const
{
  if (param >= 0 && param < parameters.size())
    strncpy (out, parameters[param].name.c_str(), len);
}

void
VstPlugin::get_parameter_label (Param param, char *out, size_t len) const
{
  if (param >= 0 && param < parameters.size())
    strncpy (out, parameters[param].label.c_str(), len);
}

void
VstPlugin::get_parameter_display (Param param, char *out, size_t len) const
{
  if (param >= 0 && param < parameters.size())
    strncpy (out, string_printf ("%.5f", parameters[param].value).c_str(), len);
}

void
VstPlugin::set_parameter_scale (Param param, float value)
{
  if (param >= 0 && param < parameters.size())
    parameters[param].value = parameters[param].min_value + (parameters[param].max_value - parameters[param].min_value) * value;
}

float
VstPlugin::get_parameter_scale (Param param) const
{
  if (param >= 0 && param < parameters.size())
    return (parameters[param].value - parameters[param].min_value) / (parameters[param].max_value - parameters[param].min_value);

  return 0;
}

float
VstPlugin::get_parameter_value (Param param) const
{
  if (param >= 0 && param < parameters.size())
    return parameters[param].value;

  return 0;
}

void
VstPlugin::set_parameter_value (Param param, float value)
{
  if (param >= 0 && param < parameters.size())
    parameters[param].value = value;
}

void
VstPlugin::set_mix_freq (double new_mix_freq)
{
  /* this should only be called by the host if the plugin is suspended, so
   * we can alter variables that are used by process|processReplacing in the real time thread
   */
  if (midi_synth)
    delete midi_synth;
  if (morph_plan_synth)
    delete morph_plan_synth;

  mix_freq = new_mix_freq;

  morph_plan_synth = new MorphPlanSynth (mix_freq);
  morph_plan_synth->update_plan (plan);

  midi_synth = new MidiSynth (*morph_plan_synth, mix_freq, 64);
}


static char hostProductString[64] = "";

static intptr_t dispatcher(AEffect *effect, int opcode, int index, intptr_t val, void *ptr, float f)
{
  VstPlugin *plugin = (VstPlugin *)effect->ptr3;

  switch (opcode) {
    case effOpen:
      return 0;

    case effClose:
      delete plugin;
      memset(effect, 0, sizeof(AEffect));
      free(effect);
      return 0;

    case effSetProgram:
    case effGetProgram:
    case effGetProgramName:
      return 0;

    case effGetParamLabel:
      plugin->get_parameter_label ((VstPlugin::Param)index, (char *)ptr, 32);
      return 0;

    case effGetParamDisplay:
      plugin->get_parameter_display ((VstPlugin::Param)index, (char *)ptr, 32);
      return 0;

    case effGetParamName:
      plugin->get_parameter_name ((VstPlugin::Param)index, (char *)ptr, 32);
      return 0;

    case effSetSampleRate:
      plugin->set_mix_freq (f);
      return 0;

    case effSetBlockSize:
    case effMainsChanged:
      return 0;

    case effEditGetRect:
      plugin->ui->getRect ((ERect **) ptr);
      return 1;

    case effEditOpen:
      plugin->ui->open((WId)(uintptr_t)ptr);
      return 1;

    case effEditClose:
      plugin->ui->close();
      return 0;

    case effEditIdle:
      plugin->ui->idle();
      return 0;

    case effGetChunk:
      {
        int result = plugin->ui->save_state((char **)ptr);
        debug ("get chunk returned: %s\n", *(char **)ptr);
        return result;
      }

    case effSetChunk:
      debug ("set chunk: %s\n", (char *)ptr);
      plugin->ui->load_state((char *)ptr);
      return 0;

    case effProcessEvents:
      {
        VstEvents *events = (VstEvents *)ptr;

        for (int32_t i = 0; i < events->numEvents; i++)
          {
            VstMidiEvent *event = (VstMidiEvent *)events->events[i];
            if (event->type != kVstMidiType)
              continue;

            plugin->midi_synth->add_midi_event (event->deltaFrames, reinterpret_cast <unsigned char *> (&event->midiData[0]));
          }
        return 1;
      }

    case effCanBeAutomated:
    case effGetOutputProperties:
      return 0;

    case effGetPlugCategory:
      return kPlugCategSynth;
    case effGetEffectName:
      strcpy((char *)ptr, "SpectMorph");
      return 1;
    case effGetVendorString:
      strcpy((char *)ptr, "Stefan Westerfeld");
      return 1;
#if 0
		case effGetProductString:
			strcpy((char *)ptr, "amsynth");
			return 1;
#endif
     case effGetVendorVersion:
       return 0;

		case effCanDo:
			if (strcmp("receiveVstMidiEvent", (char *)ptr) == 0 ||
				false) return 1;
			if (strcmp("midiKeyBasedInstrumentControl", (char *)ptr) == 0 ||
				strcmp("midiSingleNoteTuningChange", (char *)ptr) == 0 ||
				strcmp("receiveVstSysexEvent", (char *)ptr) == 0 ||
				strcmp("sendVstMidiEvent", (char *)ptr) == 0 ||
				false) return 0;
			debug("unhandled canDo: %s\n", (char *)ptr);
			return 0;

		case effGetTailSize:
		case effIdle:
		case effGetParameterProperties:
			return 0;

		case effGetVstVersion:
			return 2400;

		case effGetMidiKeyName:
		case effStartProcess:
		case effStopProcess:
		case effBeginSetProgram:
		case effEndSetProgram:
		case effBeginLoadBank:
			return 0;

		default:
			debug ("[smvstplugin] unhandled VST opcode: %d\n", opcode);
			return 0;
	}
}

static void process(AEffect *effect, float **inputs, float **outputs, int numSampleFrames)
{
  VstPlugin *plugin = (VstPlugin *)effect->ptr3;
  debug ("!missing process\n");
#if 0 // FIXME
  std::vector<amsynth_midi_cc_t> midi_out;
  plugin->synthesizer->process(numSampleFrames, plugin->midiEvents, midi_out, outputs[0], outputs[1]);
  plugin->midiEvents.clear();
#endif
}

static void processReplacing(AEffect *effect, float **inputs, float **outputs, int numSampleFrames)
{
  VstPlugin *plugin = (VstPlugin *)effect->ptr3;

  // update plan with new parameters / new modules if necessary
  if (plugin->m_new_plan_mutex.tryLock())
    {
      if (plugin->m_new_plan)
        {
          plugin->morph_plan_synth->update_plan (plugin->m_new_plan);
          plugin->m_new_plan = NULL;
        }
      plugin->m_new_plan_mutex.unlock();
    }
  plugin->midi_synth->set_control_input (0, plugin->parameters[VstPlugin::PARAM_CONTROL_1].value);
  plugin->midi_synth->set_control_input (1, plugin->parameters[VstPlugin::PARAM_CONTROL_2].value);
  plugin->midi_synth->process_audio_midi (outputs[0], numSampleFrames);

  // apply replay volume
  const float volume_factor = bse_db_to_factor (plugin->parameters[VstPlugin::PARAM_VOLUME].value);
  for (int i = 0; i < numSampleFrames; i++)
    outputs[0][i] *= volume_factor;

  // stereo -> left and right output are identical
  std::copy_n (outputs[0], numSampleFrames, outputs[1]);
}

static void setParameter(AEffect *effect, int i, float f)
{
  VstPlugin *plugin = (VstPlugin *)effect->ptr3;

  plugin->set_parameter_scale ((VstPlugin::Param) i, f);
}

static float getParameter(AEffect *effect, int i)
{
  VstPlugin *plugin = (VstPlugin *)effect->ptr3;

  return plugin->get_parameter_scale((VstPlugin::Param) i);
}

extern "C" AEffect * VSTPluginMain(audioMasterCallback audioMaster)
{
  debug ("VSTPluginMain called\n");
  if (audioMaster)
    {
      audioMaster (NULL, audioMasterGetProductString, 0, 0, hostProductString, 0.0f);
    }

  if (!sm_init_done())
    sm_init_plugin();

  if (qApp)
    {
      debug ("... (have qapp) ...\n");
    }
  else
    {
      printf ("...  (creating qapp) ...\n");
      static int argc = 0;
      new QApplication(argc, NULL, true);
    }

  AEffect *effect = (AEffect *)calloc(1, sizeof(AEffect));
  effect->magic = kEffectMagic;
  effect->dispatcher = dispatcher;
  effect->process = process;
  effect->setParameter = setParameter;
  effect->getParameter = getParameter;
  effect->numPrograms = 0;
  effect->numParams = VstPlugin::PARAM_COUNT;
  effect->numInputs = 0;
  effect->numOutputs = 2;
  effect->flags = effFlagsCanReplacing | effFlagsIsSynth | effFlagsProgramChunks | effFlagsHasEditor;

  // Do no use the ->user pointer because ardour clobbers it
  effect->ptr3 = new VstPlugin (audioMaster, effect);
  effect->uniqueID = CCONST ('s', 'm', 'r', 'p');
  effect->processReplacing = processReplacing;

  debug ("VSTPluginMain done => return %p\n", effect);
  return effect;
}

#if 0
// this is required because GCC throws an error if we declare a non-standard function named 'main'
extern "C" __attribute__ ((visibility("default"))) AEffect * main_plugin(audioMasterCallback audioMaster) asm ("main");

extern "C" __attribute__ ((visibility("default"))) AEffect * main_plugin(audioMasterCallback audioMaster)
{
	return VSTPluginMain (audioMaster);
}
#endif
