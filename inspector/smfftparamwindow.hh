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


#ifndef SPECTMORPH_FFTPARAMWINDOW_HH
#define SPECTMORPH_FFTPARAMWINDOW_HH

#include "smspectrumview.hh"
#include "smtimefreqview.hh"
#include "smzoomcontroller.hh"

#include <QComboBox>

namespace SpectMorph {

class FFTParamWindow : public QWidget
{
  Q_OBJECT

  QComboBox *transform_combobox;
  QSlider   *fft_frame_size_slider;
  QLabel    *fft_frame_size_label;

  QSlider   *fft_frame_overlap_slider;
  QLabel    *fft_frame_overlap_label;
public:
  FFTParamWindow();

  AnalysisParams  get_analysis_params();
  double          get_frame_overlap();
  double          get_frame_size();
  double          get_frame_step();


public slots:
  void on_value_changed();

signals:
  void            params_changed();
#if 0
  Gtk::Frame          cwt_frame;
  Gtk::Table          cwt_frame_table;

  Gtk::ComboBoxText   cwt_mode_combobox;
  Gtk::Label          cwt_mode_label;

  Gtk::HScale         cwt_freq_res_scale;
  Gtk::Label          cwt_freq_res_label;
  Gtk::Label          cwt_freq_res_value_label;

  Gtk::HScale         cwt_time_res_scale;
  Gtk::Label          cwt_time_res_label;
  Gtk::Label          cwt_time_res_value_label;

  Gtk::ProgressBar    progress_bar;

  void on_value_changed();

  double get_cwt_time_resolution();

public:
  FFTParamWindow();

  void   set_progress (double progress);
  AnalysisParams get_analysis_params();

#endif
};

}

#endif
