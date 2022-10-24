// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"

namespace gba::timer {

struct Timer
{
	s32 event_time;
	u16 cycles;
	u16 counter; // timer, but timer.timer looks strange
	u16 reload;
	u16 freq;
	bool cascade;
	bool irq;
	bool enable;

};

STATIC auto on_timer_event(void* user, s32 id, s32 late) -> void;
STATIC auto on_cnt_write(Gba& gba, u8 num) -> void;
STATIC auto read_timer(Gba& gba, u8 num) -> u16;

} // namespace gba::timer
