#!/bin/bash
#
# Build zmap for all architectures specified in $ZMAP_BUILD_MACHINES.
#
# Builds/logs are in FEATURE_XXX.BUILD file/directory.
#
# Main Build Parameters:
#
# Version incremented        no
#        Docs created        no
#     Docs checked in        no
#  
# Error reporting gets done by the build_run.
#

RC=0


BUILD_PREFIX='FEATURE'
#ERROR_ID='edgrif@sanger.ac.uk'
ERROR_ID=''


# Script takes 2 args, first is the directory to copy the source
# code from, second is name to give directory under ~zmap...
if (( $# != 2 )) ; then
  echo "script needs two args:  $0 <src_dir> <dest_dir>"
  exit 1
else
  SRC_DIR=$1
  DEST_DIR=$2
fi


./build_run.sh $ERROR_ID -d -i $SRC_DIR -o $DEST_DIR -g $BUILD_PREFIX || RC=1


exit $RC
