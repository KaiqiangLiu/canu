
/******************************************************************************
 *
 *  This file is part of canu, a software program that assembles whole-genome
 *  sequencing reads into contigs.
 *
 *  This software is based on:
 *    'Celera Assembler' (http://wgs-assembler.sourceforge.net)
 *    the 'kmer package' (http://kmer.sourceforge.net)
 *  both originally distributed by Applera Corporation under the GNU General
 *  Public License, version 2.
 *
 *  Canu branched from Celera Assembler at its revision 4587.
 *  Canu branched from the kmer project at its revision 1994.
 *
 *  This file is derived from:
 *
 *    src/AS_OVS/overlapStoreBuild.C
 *    src/stores/ovStoreBuild.C
 *
 *  Modifications by:
 *
 *    Brian P. Walenz from 2012-APR-02 to 2013-AUG-01
 *      are Copyright 2012-2013 J. Craig Venter Institute, and
 *      are subject to the GNU General Public License version 2
 *
 *    Brian P. Walenz from 2014-JUL-31 to 2015-SEP-21
 *      are Copyright 2014-2015 Battelle National Biodefense Institute, and
 *      are subject to the BSD 3-Clause License
 *
 *    Brian P. Walenz beginning on 2015-NOV-03
 *      are a 'United States Government Work', and
 *      are released in the public domain
 *
 *    Sergey Koren beginning on 2016-FEB-20
 *      are a 'United States Government Work', and
 *      are released in the public domain
 *
 *  File 'README.licenses' in the root directory of this distribution contains
 *  full conditions and disclaimers for each license.
 */

#include "runtime.H"

#include "sqStore.H"
#include "ovStore.H"
#include "ovStoreConfig.H"

#include <vector>
#include <algorithm>

using namespace std;



class evalueFileMap {
public:
  evalueFileMap() {
    _name  = NULL;
    _bgnID = UINT32_MAX;
    _endID = 0;
    _Nolap = 0;
  }

  char     *_name;

  uint32    _bgnID;
  uint32    _endID;
  uint64    _Nolap;
};

bool
operator<(evalueFileMap const &a, evalueFileMap const &b) {
  return(a._bgnID < b._bgnID);
};



void
ovStore::addEvalues(vector<char *> &fileList) {
  char  evalueName[FILENAME_MAX+1];
  char  evalueTemp[FILENAME_MAX+1];

  //  Handy to have the name of the file we're working with.

  snprintf(evalueTemp, FILENAME_MAX, "%s/evalues.WORKING", _storePath);
  snprintf(evalueName, FILENAME_MAX, "%s/evalues",         _storePath);

  //  If we have an opened memory mapped file, close it.  There _shouldn't_ be one,
  //  as it would exist only if evalues were already added, but it might.  And if it
  //  does exist, nuke it from disk too (well, not quite yet).

  if (_evaluesMap) {
    delete _evaluesMap;

    _evaluesMap = NULL;
    _evalues    = NULL;
  }

  if (fileExists(evalueName) == true) {
    fprintf(stderr, "WARNING:\n");
    fprintf(stderr, "WARNING: existing evalue file will be overwritten!\n");
    fprintf(stderr, "WARNING:\n");
  }

  //  Scan each file, finding the first overlap in it.

  fprintf(stderr, "\n");
  fprintf(stderr, "Scanning.\n");

  evalueFileMap   *emap = new evalueFileMap [fileList.size()];

  for (uint32 ii=0; ii<fileList.size(); ii++) {
    emap[ii]._name  = fileList[ii];
    emap[ii]._bgnID = 0;
    emap[ii]._endID = 0;
    emap[ii]._Nolap = 0;

    FILE *E = AS_UTL_openInputFile(fileList[ii]);

    loadFromFile(emap[ii]._bgnID, "bgnID", E);
    loadFromFile(emap[ii]._endID, "endID", E);
    loadFromFile(emap[ii]._Nolap, "Nolap", E);

    AS_UTL_closeFile(E);

    fprintf(stderr, "  '%s' covers reads %7" F_U32P "-%-7" F_U32P " with %10" F_U64P " overlaps.\n",
            emap[ii]._name, emap[ii]._bgnID, emap[ii]._endID, emap[ii]._Nolap);
  }

  //  Sort the emap by starting read.

  sort(emap, emap + fileList.size());

  //  Check that all IDs are present.

  for (uint32 ii=1; ii<fileList.size(); ii++) {
    if (emap[ii-1]._endID < emap[ii]._bgnID)
      fprintf(stderr, "Discontinuity between files '%s' and '%s'.\n", emap[ii-1]._name, emap[ii]._name);
  }

  //  Now just copy the new evalues to the real evalues file.  The inputs two 32-bit words and
  //  a 64-bit word at the start we need to ignore.  That's 8 16-bit words.

  fprintf(stderr, "\n");
  fprintf(stderr, "Merging.\n");

  FILE *EO = AS_UTL_openOutputFile(evalueTemp);

  for (uint32 ii=0; ii<fileList.size(); ii++) {
    uint16 *ev = new uint16 [emap[ii]._Nolap + 8];

    AS_UTL_loadFile(emap[ii]._name, ev, emap[ii]._Nolap + 8);

    writeToFile(ev + 8, "evalues", emap[ii]._Nolap, EO);

    delete [] ev;

    fprintf(stderr, "  '%s' covers reads %7" F_U32P "-%-7" F_U32P "; %10" F_U64P " with overlaps.\n",
            emap[ii]._name, emap[ii]._bgnID, emap[ii]._endID, emap[ii]._Nolap);
  }

  AS_UTL_closeFile(EO, evalueTemp);

  fprintf(stderr, "\n");
  fprintf(stderr, "Renaming.\n");

  AS_UTL_unlink(evalueName);
  AS_UTL_rename(evalueTemp, evalueName);

  fprintf(stderr, "\n");
  fprintf(stderr, "Success!\n");
  fprintf(stderr, "\n");
}





int
main(int argc, char **argv) {
  char           *ovlName        = NULL;
  char           *seqName        = NULL;
  vector<char *>  fileList;

  argc = AS_configure(argc, argv);

  vector<char *>  err;
  int             arg=1;
  while (arg < argc) {
    if        (strcmp(argv[arg], "-O") == 0) {
      ovlName = argv[++arg];

    } else if (strcmp(argv[arg], "-S") == 0) {
      seqName = argv[++arg];

    } else if (strcmp(argv[arg], "-L") == 0) {
      AS_UTL_loadFileList(argv[++arg], fileList);

    } else if (((argv[arg][0] == '-') && (argv[arg][1] == 0)) ||
               (fileExists(argv[arg]))) {
      fileList.push_back(argv[arg]);        //  Assume it's an input file

    } else {
      char *s = new char [1024];
      snprintf(s, 1024, "%s: unknown option '%s'.\n", argv[0], argv[arg]);
      err.push_back(s);
    }

    arg++;
  }

  if (ovlName == NULL)
    err.push_back("ERROR: No overlap store (-O) supplied.\n");

  if (seqName == NULL)
    err.push_back("ERROR: No sequence store (-S) supplied.\n");

  if (fileList.size() == 0)
    err.push_back("ERROR: No input erate files (-L or last on the command line) supplied.\n");

  if (err.size() > 0) {
    fprintf(stderr, "usage: %s -O asm.ovlStore -S asm.seqStore [-L evalueFileList] [evalueFile ...]\n", argv[0]);
    fprintf(stderr, "  -O asm.ovlStore       path to the overlap store to create\n");
    fprintf(stderr, "  -S asm.seqStore       path to a sequence store\n");
    fprintf(stderr, "  -L fileList           a list of evalue files in 'fileList'\n");
    fprintf(stderr, "\n");

    for (uint32 ii=0; ii<err.size(); ii++)
      if (err[ii])
        fputs(err[ii], stderr);

    exit(1);
  }


  ovStore  *ovs = new ovStore(ovlName, NULL);

  ovs->addEvalues(fileList);

  delete    ovs;

  exit(0);
}
