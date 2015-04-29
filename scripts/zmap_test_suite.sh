#!/bin/bash

SCRIPT_NAME=$(basename $0)
INITIAL_DIR=$(pwd)
 SCRIPT_DIR=$(dirname $0)
if ! echo $SCRIPT_DIR | egrep -q "(^)/" ; then
   BASE_DIR=$INITIAL_DIR/$SCRIPT_DIR
else
   BASE_DIR=$SCRIPT_DIR
fi

. $BASE_DIR/zmap_functions.sh || { echo "Failed to load zmap_functions.sh"; exit 1; }
set -o history
. $BASE_DIR/build_config.sh   || { echo "Failed to load build_config.sh";   exit 1; }


while getopts ":r:" opt ; do
    case $opt in
#	t  ) TAR_FILE=$OPTARG            ;;
	r  ) RELEASE_LOCATION=$OPTARG    ;;
	\? ) zmap_message_exit "$usage"
    esac
done

shift $(($OPTIND - 1))


# including VARIABLE=VALUE settings from command line
if [ $# -gt 0 ]; then
    eval "$*"
fi

[ -n "$RELEASE_LOCATION" ] || { 
    zmap_message_err "RELEASE_LOCATION not set. Setting to '.'"
    RELEASE_LOCATION=. 
}

zmap_message_out "Easier if we cd to $BASE_DIR"

zmap_cd $BASE_DIR

# If you're in the scripts directory this file is in and would like to
# test this script you'll need to run the following commands.

# mkdir -p Linux_i686
# cd Linux_i686
# ln -s ../../src/build/linux bin
# cd bin
# ln -s /nfs/team71/acedb/zmap/BUILDS/ZMap.0-1-55.BUILD/Linux_i686/bin/sgifaceserver sgifaceserver

# Load the x11_functions
. $BASE_DIR/zmap_x11_functions.sh

#unset DISPLAY

zmap_x11_save_enviroment

zmap_x11_examine_environment

zmap_x11_get_xserver

X_APP=x-window-manager
X_APP=xlogo
# uncomment the next line while testing...
#X_APP=xlogo

# Need an exit function that kills $X_APP first
# Usage: zmap_message_kill_exit "message"
function zmap_message_kill_exit
{
    zmap_message_err "Running 'killall -9 $X_APP'"
    killall -9 $X_APP
    zmap_message_exit "$@"
}

# print out lots of information here.
# DISPLAY=$DISPLAY
# XAUTHORITY=$XAUTHORITY
zmap_message_out "waiting for the server to start..."

sleep 5

zmap_x11_check_xserver


zmap_message_out "...running $X_APP"

$X_APP &

zmap_message_out "waiting for $X_APP to start..."

sleep 5

zmap_message_out "...attempting to get pid of $X_APP"

X_APP_PID=`pidof $X_APP`

zmap_message_out "got pid of '$X_APP_PID'"

[ "x$X_APP_PID" == "x" ] && { 
    # We need to protect against pidof not working here as this is the
    # only handle we have on removing the xserver (we use -terminate)
    zmap_message_err "Failed running $X_APP..."
#    zmap_message_out "killall -9 $X_APP anyway"
#    killall -9 $X_APP
    zmap_message_exit "exiting..."
}

# Set the file names

if [ "x$RELEASE_LOCATION" == "x." ]; then
    OUTPUT_DIR=$BASE_DIR
else
    OUTPUT_DIR=$RELEASE_LOCATION/ZMap
fi

ZMAP_CONFIG_DIR=$OUTPUT_DIR
ZMAP_CONFIG=$ZMAP_CONFIG_DIR/$USER.ZMap
ZMAP_LOG=${USER}.zmap.log

COMMAND_FILE=$OUTPUT_DIR/${USER}-xremote_gui.cmd
CONFIG_FILE=$OUTPUT_DIR/${USER}-xremote.ini

# Set variables for the files

config_set_ZMAP_ARCH `hostname -s`
PROGRAM_PATH=$RELEASE_LOCATION/$ZMAP_ARCH/bin

SEQUENCE=20.2748056-2977904
DATABASE_LOCATION=~/acedb_sessions/$SEQUENCE
PORT=24321

NEW_FEATURE=__new_feature_name__
OLD_FEATURE=__old_feature_name__

CONTEXT_DUMP=context.dump
CONTEXT_FORMAT=context


# Actually write the files

# The zmap config file that zmap reads

# N.B. Substitution _will_ occur in this HERE doc.
cat <<EOF > $ZMAP_CONFIG || zmap_message_kill_exit "Failed to write '$ZMAP_CONFIG'"
# Generated by $0
[ZMap]
show_mainwindow=true
[logging]
filename=$ZMAP_LOG
EOF

# The config file the xremote_gui reads

# N.B. Substitution _will_ occur in this HERE doc.
cat <<EOF > $CONFIG_FILE || zmap_message_kill_exit "Failed to write '$CONFIG_FILE'"
# Generated by $0
[programs]
zmap-exe=$PROGRAM_PATH/zmap
zmap-options=--conf_dir $ZMAP_CONFIG_DIR --conf_file $USER.ZMap
sgifaceserver-exe=$PROGRAM_PATH/sgifaceserver
sgifaceserver-options=-readonly $DATABASE_LOCATION $PORT 0:0

EOF


# The command file that xremote_gui reads

# N.B. Substitution _will_ occur in this HERE doc.
cat <<EOF > $COMMAND_FILE || zmap_message_kill_exit "Failed to write '$COMMAND_FILE'"
<zmap action="new_zmap" />
__EOC__
<zmap action="new_view">
  <segment sequence="$SEQUENCE" start="1" end="0">
[ZMap]
sources=source
[source]
url=acedb://any:any@localhost:24321?use_methods=true
featuresets=coding;trf;transcript;est_human
</segment>
</zmap>
__EOC__
<zmap action="export_context">
  <export filename="$OUTPUT_DIR/$SEQUENCE.gff" format="gff" />
</zmap>
__EOC__
<zmap action="export_context">
  <export filename="$OUTPUT_DIR/$SEQUENCE.context" format="context" />
</zmap>
__EOC__

EOF


# Now run the xremote_gui command, which should look like
# xremote_gui --config-file ../../../scripts/xremote.ini --command-file ../../../scripts/xremote_gui.cmd

zmap_message_out "Running $PROGRAM_PATH/xremote_gui --config-file $CONFIG_FILE --command-file $COMMAND_FILE"

$PROGRAM_PATH/xremote_gui \
--config-file $CONFIG_FILE   \
--command-file $COMMAND_FILE || zmap_message_kill_exit "Failed runing xremote_gui"

zmap_x11_restore_enviroment

# We've finished with X now.
kill -9 $X_APP_PID

# Now there should be 
# a) dumpfiles [$SEQUENCE.gff & $SEQUENCE.context]
# b) no xremote_gui process
# c) no zmap process
# d) no sgifaceserver process


# Now we need to test stuff.

# grep CRITICAL zmap.log 

CRITICAL_MESSAGES=`grep CRITICAL $ZMAP_CONFIG_DIR/$ZMAP_LOG 2>&1`

if [ "x$CRITICAL_MESSAGES" != "x" ]; then
    zmap_message_err "Testing zmap resulted in CRITICAL message(s) in $ZMAP_CONFIG_DIR/$ZMAP_LOG"
    zmap_message_err $CRITICAL_MESSAGES
    zmap_message_exit "$SCRIPT_NAME failed."
fi

REPORT_IDENTICAL_FILES=-s
REPORT_IDENTICAL_FILES=""

# diff $SEQUENCE.context gold.context

# this should be from the build_config.sh, not the same file!!!
ZMAP_GOLD_CONTEXT=$OUTPUT_DIR/$SEQUENCE.context

CONTEXT_DIFF=`diff $REPORT_IDENTICAL_FILES $OUTPUT_DIR/$SEQUENCE.context $ZMAP_GOLD_CONTEXT 2>&1`

if [ "x$CONTEXT_DIFF" != "x" ]; then
    zmap_message_err "Testing zmap resulted in differences between dumped context and gold context"
    # echo $CONTEXT_DIFF
    zmap_message_exit "$SCRIPT_NAME failed."
fi


# diff $SEQUENCE.gff gold.gff

# this should be from the build_config.sh, not the same file!!!
ZMAP_GOLD_GFF=$OUTPUT_DIR/$SEQUENCE.gff

GFF_DIFF=`diff $REPORT_IDENTICAL_FILES $OUTPUT_DIR/$SEQUENCE.gff $ZMAP_GOLD_GFF 2>&1`

if [ "x$GFF_DIFF" != "x" ]; then
    zmap_message_err "Testing zmap resulted in differences between dumped gff and gold gff"
    # echo $GFF_DIFF
    zmap_message_exit "$SCRIPT_NAME failed."
fi



zmap_message_out "Reached end of '$SCRIPT_NAME'"

exit 0;
