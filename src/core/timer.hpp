// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"

namespace gba::timer {

struct Timer
{
	u32 event_time;
	u16 cycles;
	u16 counter; // timer, but timer.timer looks strange
	u16 reload;
	u16 freq;
	bool cascade;
	bool irq;
	bool enable;

};

STATIC auto on_timer0_event(Gba& gba) -> void;
STATIC auto on_timer1_event(Gba& gba) -> void;
STATIC auto on_timer2_event(Gba& gba) -> void;
STATIC auto on_timer3_event(Gba& gba) -> void;

STATIC auto on_cnt_write(Gba& gba, u8 num) -> void;

STATIC auto update_timer(Gba& gba, Timer& timer) -> void;
STATIC auto read_timer(Gba& gba, u8 num) -> u16;

STATIC auto run(Gba& gba, u8 cycles) -> void;

} // namespace gba::timer
