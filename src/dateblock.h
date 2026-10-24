/*
 Dateblock blocks (sleeps) until a cron-style moment in time.
 Copyright (C) 2013 Chris Caron <lead2gold@gmail.com>
 
 This file is part of Dateblock.  Dateblock is free software; you can
 redistribute it and/or modify it under the terms of the GNU General Public
 License as published by the Free Software Foundation; either version 2 of
 the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#ifndef _DATEBLOCK_H_
#define _DATEBLOCK_H_

#define  AUTHOR       "Chris Caron <lead2gold@gmail.com>"
#define  ONEMIN       60
#define  ONEHOUR      3600
#define  ONEDAY       86400

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Date.h"

using namespace std;

/// ProgramDetails displays help information
void ProgramDetails();

/// ProgramExamples displays sample executions of the progrm
void ProgramExamples();
#endif //_DATEBLOCK_H_
