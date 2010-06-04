/* 
 * Copyright (C) 2010 Stefan Westerfeld
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "config.h"
#include <smwavset.hh>
#include <smaudio.hh>
#include <bse/bsemain.h>
#include <bse/bseloader.h>

#include <string>
#include <map>

using std::string;
using std::vector;
using std::map;

using SpectMorph::WavSet;
using SpectMorph::WavSetWave;

/// @cond
struct Options
{
  string	program_name; /* FIXME: what to do with that */
  string        data_dir;
  string        args;
  enum { NONE, INIT, ADD, LIST, ENCODE, DECODE, DELTA } mode;

  Options ();
  void parse (int *argc_p, char **argv_p[]);
  static void print_usage ();
} options;
/// @endcond

Options::Options ()
{
  program_name = "smwavset";
  data_dir = "/tmp";
  args = "";
  mode = NONE;
}

#include "stwutils.hh"

void
Options::parse (int   *argc_p,
                char **argv_p[])
{
  guint argc = *argc_p;
  gchar **argv = *argv_p;
  unsigned int i, e;

  g_return_if_fail (argc >= 0);

  /*  I am tired of seeing .libs/lt-gst123 all the time,
   *  but basically this should be done (to allow renaming the binary):
   *
  if (argc && argv[0])
    program_name = argv[0];
  */

  for (i = 1; i < argc; i++)
    {
      const char *opt_arg;
      if (strcmp (argv[i], "--help") == 0 ||
          strcmp (argv[i], "-h") == 0)
	{
	  print_usage();
	  exit (0);
	}
      else if (strcmp (argv[i], "--version") == 0 || strcmp (argv[i], "-v") == 0)
	{
	  printf ("%s %s\n", program_name.c_str(), VERSION);
	  exit (0);
	}
      else if (check_arg (argc, argv, &i, "-i") || check_arg (argc, argv, &i, "--init"))
        {
          mode = INIT;
        }
      else if (check_arg (argc, argv, &i, "-a") || check_arg (argc, argv, &i, "--add"))
        {
          mode = ADD;
        }
      else if (check_arg (argc, argv, &i, "-l") || check_arg (argc, argv, &i, "--list"))
        {
          mode = LIST;
        }
      else if (check_arg (argc, argv, &i, "-e") || check_arg (argc, argv, &i, "--encode"))
        {
          mode = ENCODE;
        }
      else if (check_arg (argc, argv, &i, "-d") || check_arg (argc, argv, &i, "--decode"))
        {
          mode = DECODE;
        }
      else if (check_arg (argc, argv, &i, "--delta"))
        {
          mode = DELTA;
        }
      else if (check_arg (argc, argv, &i, "--args", &opt_arg))
        {
          args = opt_arg;
        }
      else if (check_arg (argc, argv, &i, "-D", &opt_arg))
	{
	  data_dir = opt_arg;
        }
#if 0
      else if (check_arg (argc, argv, &i, "-d"))
	{
          debug = fopen ("/tmp/stwenc.log", "w");
	}
      else if (check_arg (argc, argv, &i, "-f", &opt_arg))
	{
	  fundamental_freq = atof (opt_arg);
        }
      else if (check_arg (argc, argv, &i, "-m", &opt_arg))
        {
          fundamental_freq = freqFromNote (atoi (opt_arg));
        }
      else if (check_arg (argc, argv, &i, "-O0"))
        {
          optimization_level = 0;
        }
      else if (check_arg (argc, argv, &i, "-O1"))
        {
          optimization_level = 1;
        }
      else if (check_arg (argc, argv, &i, "-O2"))
        {
          optimization_level = 2;
        }
      else if (check_arg (argc, argv, &i, "-O", &opt_arg))
        {
          optimization_level = atoi (opt_arg);
        }
      else if (check_arg (argc, argv, &i, "-s"))
        {
          strip_models = true;
        }
#endif
    }

  /* resort argc/argv */
  e = 1;
  for (i = 1; i < argc; i++)
    if (argv[i])
      {
        argv[e++] = argv[i];
        if (i >= e)
          argv[i] = NULL;
      }
  *argc_p = e;
}

void
Options::print_usage ()
{
  g_printerr ("usage: %s [ <options> ] <src_audio_file> <dest_sm_file>\n", options.program_name.c_str());
  g_printerr ("\n");
  g_printerr ("options:\n");
  g_printerr (" -h, --help                  help for %s\n", options.program_name.c_str());
  g_printerr (" --version                   print version\n");
  g_printerr (" -f <freq>                   specify fundamental frequency in Hz\n");
  g_printerr (" -m <note>                   specify midi note for fundamental frequency\n");
  g_printerr (" -O <level>                  set optimization level\n");
  g_printerr (" -s                          produced stripped models\n");
  g_printerr ("\n");
}

string
int2str (int i)
{
  char buffer[512];
  sprintf (buffer, "%d", i);
  return buffer;
}

vector<WavSetWave>::iterator
find_wave (WavSet& wset, int midi_note)
{
  vector<WavSetWave>::iterator wi = wset.waves.begin(); 

  while (wi != wset.waves.end())
    {
      if (wi->midi_note == midi_note)
        return wi;
      wi++;
    }

  return wi;
}

bool
load_wav_file (const string& filename, vector<float>& data_out)
{
  /* open input */
  BseErrorType error;

  BseWaveFileInfo *wave_file_info = bse_wave_file_info_load (filename.c_str(), &error);
  if (!wave_file_info)
    {
      fprintf (stderr, "%s: can't open the input file %s: %s\n", options.program_name.c_str(), filename.c_str(), bse_error_blurb (error));
      return false;
    }

  BseWaveDsc *waveDsc = bse_wave_dsc_load (wave_file_info, 0, FALSE, &error);
  if (!waveDsc)
    {
      fprintf (stderr, "%s: can't open the input file %s: %s\n", options.program_name.c_str(), filename.c_str(), bse_error_blurb (error));
      return false;
    }

  GslDataHandle *dhandle = bse_wave_handle_create (waveDsc, 0, &error);
  if (!dhandle)
    {
      fprintf (stderr, "%s: can't open the input file %s: %s\n", options.program_name.c_str(), filename.c_str(), bse_error_blurb (error));
      return false;
    }

  error = gsl_data_handle_open (dhandle);
  if (error)
    {
      fprintf (stderr, "%s: can't open the input file %s: %s\n", options.program_name.c_str(), filename.c_str(), bse_error_blurb (error));
      return false;
    }

  if (gsl_data_handle_n_channels (dhandle) != 1)
    {
      fprintf (stderr, "Currently, only mono files are supported.\n");
      return false;
    }

  data_out.clear();

  vector<float> block (1024);

  const uint64 n_values = gsl_data_handle_length (dhandle);
  for (uint64 pos = 0; pos < n_values; pos += block.size())
    {
      /* read data from file */
      uint64 r = gsl_data_handle_read (dhandle, pos, block.size(), &block[0]);

      for (uint64 t = 0; t < r; t++)
        data_out.push_back (block[t]);
    }
  return true;
}

double
delta (vector<float>& d0, vector<float>& d1)
{
  double error = 0;
  for (size_t t = 0; t < MAX (d0.size(), d1.size()); t++)
    {
      double a0 = 0, a1 = 0;
      if (t < d0.size())
        a0 = d0[t];
      if (t < d1.size())
        a1 = d1[t];
      error += (a0 - a1) * (a0 - a1);
    }
  return error / MAX (d0.size(), d1.size());
}

int
main (int argc, char **argv)
{
  bse_init_inprocess (&argc, &argv, NULL, NULL);

  options.parse (&argc, &argv);

  if (options.mode == Options::INIT)
    {
      for (int i = 1; i < argc; i++)
        {
          SpectMorph::WavSet wset;
          wset.save (argv[i]);
        }
    }
  else if (options.mode == Options::ADD)
    {
      assert (argc == 4);

      SpectMorph::WavSet wset;
      wset.load (argv[1]);
      SpectMorph::WavSetWave new_wave;
      new_wave.midi_note = atoi (argv[2]);
      new_wave.path = argv[3];
      wset.waves.push_back (new_wave);
      wset.save (argv[1]);
    }
  else if (options.mode == Options::LIST)
    {
      SpectMorph::WavSet wset;
      wset.load (argv[1]);

      for (vector<WavSetWave>::iterator wi = wset.waves.begin(); wi != wset.waves.end(); wi++)
        {
          printf ("%d %s\n", wi->midi_note, wi->path.c_str());
        }
    }
  else if (options.mode == Options::ENCODE)
    {
      assert (argc == 3);

      SpectMorph::WavSet wset, smset;
      wset.load (argv[1]);

      for (vector<WavSetWave>::iterator wi = wset.waves.begin(); wi != wset.waves.end(); wi++)
        {
          string smpath = options.data_dir + "/" + int2str (wi->midi_note) + ".sm";
          string cmd = "smenc -m " + int2str (wi->midi_note) + " " + wi->path.c_str() + " " + smpath + " " + options.args;
          printf ("## %s\n", cmd.c_str());
          system (cmd.c_str());

          SpectMorph::WavSetWave new_wave = *wi;
          new_wave.path = smpath;
          smset.waves.push_back (new_wave);
        }
      smset.save (argv[2]);
    }
  else if (options.mode == Options::DECODE)
    {
      assert (argc == 3);

      SpectMorph::WavSet smset, wset;
      smset.load (argv[1]);

      for (vector<WavSetWave>::const_iterator si = smset.waves.begin(); si != smset.waves.end(); si++)
        {
          SpectMorph::Audio audio;
          if (audio.load (si->path) != BSE_ERROR_NONE)
            {
              printf ("can't load %s\n", si->path.c_str());
              exit (1);
            }

          string wavpath = options.data_dir + "/" + int2str (si->midi_note) + ".wav";
          string cmd = "smplay --rate=" + int2str (audio.mix_freq) + " " +si->path.c_str() + " --export " + wavpath + " " + options.args;
          printf ("## %s\n", cmd.c_str());
          system (cmd.c_str());

          SpectMorph::WavSetWave new_wave = *si;
          new_wave.path = wavpath;
          wset.waves.push_back (new_wave);
        }
      wset.save (argv[2]);
    }
  else if (options.mode == Options::DELTA)
    {
      assert (argc >= 2);

      vector<SpectMorph::WavSet> wsets;
      map<int,bool>              midi_note_used;

      for (int i = 1; i < argc; i++)
        {
          WavSet wset;
          wset.load (argv[i]);
          wsets.push_back (wset);
          for (vector<WavSetWave>::const_iterator wi = wset.waves.begin(); wi != wset.waves.end(); wi++)
            midi_note_used[wi->midi_note] = true;              
        }
      for (int i = 0; i < 128; i++)
        {
          if (midi_note_used[i])
            {
              printf ("%3d: ", i);
              vector<WavSetWave>::iterator w0 = find_wave (wsets[0], i);
              assert (w0 != wsets[0].waves.end());
              for (size_t w = 1; w < wsets.size(); w++)
                {
                  vector<WavSetWave>::iterator w1 = find_wave (wsets[w], i);
                  assert (w1 != wsets[w].waves.end());

                  vector<float> data0, data1;
                  if (load_wav_file (w0->path, data0) && load_wav_file (w1->path, data1))
                    {
                      printf ("%.8e ", delta (data0, data1));
                    }
                  else
                    {
                      printf ("an error occured during loading the files.\n");
                      exit (1);
                    }
                }
              printf ("\n");
            }
        }
    }
  else
    {
      g_printerr ("You need to specify a mode (-i)\n");
      Options::print_usage();
      exit (1);
    }
}
