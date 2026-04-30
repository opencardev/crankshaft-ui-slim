/*
 * Project: Crankshaft
 * This file is part of Crankshaft project.
 * Copyright (C) 2025 OpenCarDev Team
 *
 *  Crankshaft is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Crankshaft is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Crankshaft. If not, see <http://www.gnu.org/licenses/>.
 */

#include "SlimUiApplicationRunner.h"

#if defined(__has_include)
#if __has_include("build_info.h")
#include "build_info.h"
#elif __has_include("../../build_info.h")
#include "../../build_info.h"
#else
#define CRANKSHAFT_VERSION "unknown"
#endif
#else
#define CRANKSHAFT_VERSION "unknown"
#endif

int main(int argc, char* argv[]) {
    return runSlimUiApplication(argc, argv, QString::fromUtf8(CRANKSHAFT_VERSION));
}