// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include <filesystem>

// this should probably be a generic impl for all
// systems that don't have os browser
// for now, switch specific
namespace nx::fs {

// returns true if a file is selected
auto render() -> bool;

} // namespace nx::fs
