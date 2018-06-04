#!/bin/sh

# ============================================================================
# File:
#   version.sh
#   - get version of repository and generate the file version.h
# Author:
#   Falei LUO <falei.luo@gmail.com>
# ============================================================================

# setting API version
api=13

# get version of remote and local repository
VER_R=`git rev-list --count origin/master`
VER_SHA=`git rev-parse HEAD | cut -c -16`

# generate version numbers
len=`expr length $api`
end1=`expr $len - 1`
VER_MAJOR=`echo $api | cut -c -$end1`
VER_MINOR=`echo $api | cut -c $len-`

# date and time information
BUILD_TIME=`date "+%Y-%m-%d %H:%M:%S"`

# generate the file version.h
echo "// ==========================================================================="  > version.h
echo "// version.h"                                                                   >> version.h
echo "// - collection of version numbers"                                             >> version.h
echo "//"                                                                             >> version.h
echo "// Author:  Falei LUO <falei.luo@gmail.com>"                                    >> version.h
echo "//"                                                                             >> version.h
echo "// ===========================================================================" >> version.h
echo ""                                                                               >> version.h
echo "#ifndef DAVS2_VERSION_H"                                                        >> version.h
echo "#define DAVS2_VERSION_H"                                                        >> version.h
echo ""                                                                               >> version.h
echo "// version number"                                                              >> version.h
echo "#define VER_MAJOR         $VER_MAJOR     // major version number"               >> version.h
echo "#define VER_MINOR         $VER_MINOR     // minor version number"               >> version.h
echo "#define VER_BUILD         $VER_R    // build number"                            >> version.h
echo "#define VER_SHA_STR       \"$VER_SHA\"  // commit id"                           >> version.h
echo ""                                                                               >> version.h
echo "// stringify"                                                                   >> version.h
echo "#define _TOSTR(x)       #x            // stringify x"                           >> version.h
echo "#define TOSTR(x)        _TOSTR(x)     // stringify x, perform macro expansion"  >> version.h
echo ""                                                                               >> version.h
echo "// define XVERSION string"                                                      >> version.h
echo "#define XVERSION        VER_MAJOR, VER_MINOR, VER_BUILD"                        >> version.h
echo "#define XVERSION_STR    TOSTR(VER_MAJOR) \".\" TOSTR(VER_MINOR) \".\" TOSTR(VER_BUILD) \" \" VER_SHA_STR" >> version.h
echo "#define XBUILD_TIME     \"$BUILD_TIME\""                                        >> version.h
echo ""                                                                               >> version.h
echo "#endif // __VERSION_H__"                                                        >> version.h

mv version.h source/version.h

# show version informations
echo "#define DAVS2_BUILD      $api"
echo "#define DAVS2_POINTVER \"$VER_MAJOR.$VER_MINOR.$VER_R\""
