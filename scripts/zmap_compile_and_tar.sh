#!/bin/bash

########################
# Find out where BASE is
# _must_ be first part of script
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

zmap_message_out "Start of script."

zmap_message_out "Running in $INITIAL_DIR on $(hostname)"

zmap_message_out "Parsing cmd line options: '$*'"

# Get the options the user may have requested
BUILD_PREFIX=""

while getopts ":dip:" opt ; do
    case $opt in
	i  ) ZMAP_MAKE_INSTALL=$ZMAP_TRUE     ;;
	p  ) BUILD_PREFIX=$OPTARG             ;;
	\? ) zmap_message_exit "usage notes"
    esac
done

shift $(($OPTIND - 1))

# including VARIABLE=VALUE settings from command line
if [ $# -gt 0 ]; then
    eval "$*"
fi

INSTALL_PREFIX=prefix_root
SERVER_SCRIPTS_DIR=server

# We know that this is in cvs
zmap_cd $BASE_DIR

# Hack this for now...we know we are just above ZMap anyway....
# We can then go to the correct place
#zmap_goto_cvs_module_root
#cd ..
#CVS_MODULE=ZMap
#CVS_MODULE_LOCAL=ZMap
zmap_goto_git_root


mkdir $INSTALL_PREFIX

zmap_message_out "CVS_MODULE = '$CVS_MODULE', Locally lives in (CVS_MODULE_LOCAL) '$CVS_MODULE_LOCAL'"

zmap_cd ..

# we want this for later so we can tar up the whole directory and copy
# back to where we were called from.
CVS_CHECKOUT_DIR=$(pwd)

zmap_message_out "Aiming for the src directory to do the build"
zmap_cd $CVS_MODULE_LOCAL/src

zmap_message_out "PATH =" $PATH

zmap_message_out "Running $ZMAP_BOOTSTRAP_SCRIPT"
./$ZMAP_BOOTSTRAP_SCRIPT  || zmap_message_exit $(hostname) "Failed Running $ZMAP_BOOTSTRAP_SCRIPT"
zmap_message_out "Finished $ZMAP_BOOTSTRAP_SCRIPT"


zmap_message_out "Running $ZMAP_RUNCONFIG_SCRIPT"
./$ZMAP_RUNCONFIG_SCRIPT --prefix=$CVS_CHECKOUT_DIR/$CVS_MODULE_LOCAL/$INSTALL_PREFIX || \
    zmap_message_exit $(hostname) "Failed Running $ZMAP_RUNCONFIG_SCRIPT"
zmap_message_out "Finished $ZMAP_RUNCONFIG_SCRIPT"


if [ "x$ZMAP_MAKE" == "x$ZMAP_TRUE" ]; then
    zmap_message_out "Running make"
    make         || zmap_message_exit $(hostname) "Failed Running make"
fi


if [ "x$ZMAP_MAKE_CHECK" == "x$ZMAP_TRUE" ]; then
    zmap_message_out "Running make check"

    CHECK_LOG_FILE=$(pwd)/$SCRIPT_NAME.check.log

    MAKE_CHECK_FAILED=no

    make check check_zmap_LOG_FILE=$CHECK_LOG_FILE || MAKE_CHECK_FAILED=$ZMAP_TRUE

    if [ "x$MAKE_CHECK_FAILED" == "x$ZMAP_TRUE" ]; then

	TMP_LOG=/var/tmp/zmap_fail.$$.log

	echo "ZMap Tests Failure"                            > $TMP_LOG
	echo ""                                             >> $TMP_LOG
	echo "Tail of make check log:"                      >> $TMP_LOG
	echo ""                                             >> $TMP_LOG
	tail $CHECK_LOG_FILE                                >> $TMP_LOG
	echo ""                                             >> $TMP_LOG
	echo "Full log is here $(hostname):$CHECK_LOG_FILE" >> $TMP_LOG

	ERROR_RECIPIENT=$ZMAP_MASTER_NOTIFY_MAIL

	if [ "x$ERROR_RECIPIENT" != "x" ]; then
	    zmap_message_out "Mailing log to $ERROR_RECIPIENT"

	    cat $TMP_LOG | mailx -s "ZMap Tests Failure on $(hostname)" $ERROR_RECIPIENT
	fi
	rm -f $TMP_LOG

	zmap_message_err $(hostname) "Failed Running make check"
    fi
fi


if [ "x$ZMAP_MAKE_INSTALL" == "x$ZMAP_TRUE" ]; then
    zmap_message_out "Running make install"
    make install || zmap_message_exit $(hostname) "Failed running make install"
fi

zmap_cd $CVS_CHECKOUT_DIR

zmap_cd $CVS_MODULE_LOCAL



if [ "x$TAR_TARGET" != "x" ]; then

    zmap_scp_path_to_host_path $TAR_TARGET

    zmap_message_out $TAR_TARGET_HOST $TAR_TARGET_PATH
    TAR_TARGET_CVS=$CVS_MODULE.$(hostname -s)
    config_set_ZMAP_ARCH `hostname -s`
    UNAME_DIR=$ZMAP_ARCH

    [ "x$TAR_TARGET_CVS"  != "x" ] || zmap_message_exit "No CVS set."

    tar -zcf - * | ssh $TAR_TARGET_HOST "/bin/bash -c '\
cd $TAR_TARGET_PATH    || exit 1; \
rm -rf $TAR_TARGET_CVS || exit 1; \
mkdir $TAR_TARGET_CVS  || exit 1; \
cd $TAR_TARGET_CVS     || exit 1; \
tar -zxf -             || exit 1; \
cd ..                  || exit 1; \
ln -s $TAR_TARGET_CVS/$INSTALL_PREFIX $UNAME_DIR'"

    if [ $? != 0 ]; then
	zmap_message_exit "Failed to copy!"
    else
	zmap_message_out "Sucessfully copied to $TAR_TARGET_HOST:$TAR_TARGET_PATH/$TAR_TARGET_CVS"
    fi
fi


# Copy the builds to /software for certain types of build:
#   production builds get installed in /software/annotools/
#   release builds get installed in /software/annotools/test
#   develop builds get installed in /software/annotools/dev
#
zmap_message_out "Checking whether to install on /software for $BUILD_PREFIX build"

if [[ $BUILD_PREFIX == "PRODUCTION" || $BUILD_PREFIX == "RELEASE" || $BUILD_PREFIX == "DEVELOPMENT" ]]
then
  zmap_message_out "Installing on /software"
  source_dir=$TAR_TARGET_PATH/$TAR_TARGET_CVS/$INSTALL_PREFIX      # where to copy the installed files from
  server_dir=$TAR_TARGET_PATH/$TAR_TARGET_CVS/$SERVER_SCRIPTS_DIR  # where to copy the server scripts from

  dev_machine=$TAR_TARGET_HOST          # must be a machine with write access to the project software area
  software_root_dir="/software/noarch"  # root directory for project software
  arch_subdir=""                        # the subdirectory for the current machine architecture
  annotools_subdir="annotools"          # the subdirectory for annotools

  config_set_PROJECT_ARCH `hostname -s`
  arch_subdir=$PROJECT_ARCH
  
  if [[ $arch_subdir != "" ]]
  then
    # production builds are installed in the project area directly
    # release and develop builds are stored in 'test' and 'dev' subdirectories
    build_subdir=""
    if [[ $BUILD_PREFIX == "DEVELOPMENT" ]]
    then
        build_subdir="/dev"
    elif [[ $BUILD_PREFIX == "RELEASE" ]]
    then
      build_subdir="/test"
    fi

    # Set the destination for the current host machine type
    project_area=$software_root_dir/$arch_subdir/$annotools_subdir$build_subdir

    # Do the copy. (Hack to hard-code path to copy script - can't find it in the
    # local checkout for some reason.)
    copy_script=~zmap/BUILD_CHECKOUT/ZMap_develop/scripts/copy_directory.sh

    # Copy the source directory to the project area (the source includes the bin and share 
    # subdirectories so we don't need to explicitly specify those). 
    zmap_message_out "Running: ssh $dev_machine "$copy_script $source_dir $project_area""
    ssh $dev_machine "$copy_script $source_dir $project_area"
    wait

    # Copy the server scripts to the project area. Note we need to specify 
    # the bin subdirectory in the destination here.
    zmap_message_out "Running: ssh $dev_machine "$copy_script $server_dir $project_area/bin""
    ssh $dev_machine "$copy_script $server_dir $project_area/bin"
    wait
  
    # We don't currently build on trusty 64-bit ubuntu because the precise build works there.
    # This means we need to copy the precise build to the trusty destination on /software.
    if [ $arch_subdir == "precise-x86_64" ]
    then
      copy_subdirs="trusty-x86_64"

      for dest_subdir in $copy_subdirs; do
        dest_project_area=$software_root_dir/$dest_subdir/$annotools_subdir$build_subdir

        zmap_message_out "Running: ssh $dev_machine $copy_script $source_dir $dest_project_area"
        ssh $dev_machine "$copy_script $source_dir $dest_project_area"
        wait  

        zmap_message_out "Running: ssh $dev_machine $copy_script $server_dir $dest_project_area/bin"
        ssh $dev_machine "$copy_script $server_dir $dest_project_area/bin"
        wait  
      done
    fi    
  fi
else
  zmap_message_out "NOT installing on /software"
fi
