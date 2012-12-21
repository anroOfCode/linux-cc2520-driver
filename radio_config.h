#ifndef RADIO_CONFIG_H
#define RADIO_CONFIG_H

#include "cc2520.h"

/////////////////////////////////
// Radio (default) Configuration
/////////////////////////////////

// Some of these settings are recommended by the data sheet for maximum
// performance, others are inferred from the configuration of other
// 802.15.4 radios in TinyOS.

static cc2520_frmctrl0_t cc2520_frmctrl0_default = {.f.autoack = 0, .f.autocrc = 1};
// Set 0dBm output power
static cc2520_txpower_t cc2520_txpower_default = { .f.pa_power = 0x32 };

// Raises CCA threshold from -108dBm to -8 - 76 = -84dBm
//static cc2520_ccactrl0_t cc2520_ccactrl0_default = { .f.cca_thr = 0xF8 };
// FIXME: This might be a problem in the EK devkit. But the threshold has to
// be really high!
// Raises CCA threshold from -108dBm to 10 - 76dBm
static cc2520_ccactrl0_t cc2520_ccactrl0_default = { .f.cca_thr = 0x1A };

// Makes sync word detection less likely by requiring two zero symbols before
// the sync word
static cc2520_mdmctrl0_t cc2520_mdmctrl0_default = {.f.tx_filter = 1, .f.preamble_length = 2, .f.demod_avg_mode = 0, .f.dem_num_zeros = 2};

// Only one SFD symbol must be above threshold, and raise correlation
// threshold
static cc2520_mdmctrl1_t cc2520_mdmctrl1_default = {.f.corr_thr = 0x14, .f.corr_thr_sfd = 0};

static cc2520_freqctrl_t cc2520_freqctrl_default = {.f.freq = 0x0B };

static cc2520_fifopctrl_t cc2520_fifopctrl_default =  {.f.fifop_thr = 127};

static cc2520_frmfilt0_t cc2520_frmfilt0_default = {.f.max_frame_version = 2, .f.frame_filter_en = 1};

static cc2520_frmfilt1_t cc2520_frmfilt1_default = {.f.accept_ft_0_beacon = 1, .f.accept_ft_1_data = 1, .f.accept_ft_2_ack = 1, .f.accept_ft_3_mac_cmd = 1, .f.accept_ft_4to7_reserved = 1};

static cc2520_srcmatch_t cc2520_srcmatch_default = {.f.src_match_en = 0, .f.autopend = 1, .f.pend_datareq_only = 1};

// Updates from the Datasheet.
static cc2520_rxctrl_t cc2520_rxctrl_default = {.value = 0x3F};
static cc2520_fsctrl_t cc2520_fsctrl_default = {.value = 0x5A};
static cc2520_fscal1_t cc2520_fscal1_default = {.value = 0x2B};
static cc2520_agcctrl1_t cc2520_agcctrl1_default = {.value = 0x11};
static cc2520_adctest0_t cc2520_adctest0_default = {.value = 0x10};
static cc2520_adctest1_t cc2520_adctest1_default = {.value = 0x0E};
static cc2520_adctest2_t cc2520_adctest2_default = {.value = 0x03};

#endif