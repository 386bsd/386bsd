/* 
Copyright (C) 1988 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

This file is part of GNU CC.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the GNU CC General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
GNU CC, but only under the conditions described in the
GNU CC General Public License.   A copy of this license is
supposed to have been given to you along with GNU CC so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  
*/

#ifdef __GNUG__
#pragma implementation
#endif
#include <SFile.h>

SFile::SFile() {}
SFile::~SFile() {}

SFile::SFile(const char* filename, int size, io_mode m, access_mode a)
: (filename, m, a) 
{ 
  sz = size; 
}

SFile::SFile(const char* filename, int size, const char* m)
: (filename, m)    
{ 
  sz = size; 
}

SFile::SFile(int filedesc, int size, io_mode m)
: (filedesc, m)
{ 
  sz = size; 
}

SFile::SFile(FILE* fileptr,  int size)
: (fileptr)
{ 
  sz = size; 
}
