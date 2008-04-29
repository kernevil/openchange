#!/bin/sh

#
# VARS
#
GITREV="7fccd85"

#
# Error check
#
error_check() {
    error=$1
    step=$2

    if [ $error -ne 0 ]; then
	echo "Error in $2 (error code $1)"
	exit 1
    fi
}

cleanup_talloc() {
    # cleanup existing talloc installation
    if test -f samba4/source/lib/talloc/Makefile; then
	echo "Step0: cleaning up talloc directory"
	OLD_PWD=$PWD
	cd samba4/source/lib/talloc
	make realdistclean
	rm -rf ../replace/*.o ../replace/*.ho
	cd $OLD_PWD
    fi
}

clean_tdb() {
    # cleanup existing tdb installation
    if test -f samba/source/lib/tdb/Makefile; then
	echo "Step0: cleaning up tdb directory"
	OLD_PWD=$PWD
	cd samba4/source/lib/tdb
	make realdistclean
	rm -rf ../replace/*.o ../replace/*.ho
	cd $OLD_PWD
    fi
}

delete_install() {

    # cleanup existing existing samba4 installation
    if test -d /usr/local/samba; then
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo "A previous samba4 installation has been detected"
	echo "It is highly recommended to delete it prior compiling Samba4"
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo ""
	echo -n "Proceed? [Yn]: "
	read answer
	case "$answer" in
	    Y|y|yes)
		echo "Step0: Removing previous samba4 installation"
		sudo rm -rf /usr/local/samba
		;;
	    N|n|no)
		echo "Step0: Keep previous samba4 installation"
		;;
	esac
    fi

	cleanup_talloc

	cleanup_tdb
}

#
# Checkout Samba4
#
checkout() {
    OLD_PWD=$PWD

    GITPATH=`whereis -b git`

    if test x"$GITPATH" = x"git:"; then
	echo "git was not found in your path!"
	echo "Please install git"
	exit 1
    fi

    echo "Step1: Fetching Samba4 latest GIT revision"
    git-clone git://git.samba.org/samba.git samba4
    error_check $? "Step1"

    echo "Step2: Creating openchange local copy"
    cd samba4
    git checkout -b openchange origin/v4-0-test
    error_check $? "Step2"

    echo "Step3: Revert to commit $GITREV"
    git reset --hard $GITREV
    error_check $? "Step3"

    cd $OLD_PWD
    return $?
}

#
# Compile and Install samba4 packages:
# talloc, tdb
#
packages() {
    OLD_PWD=$PWD

    delete_install

    echo "Step1: Installing talloc library"
    cd samba4/source/lib/talloc
    error_check $? "Step1"

    rm -rf ../replace/*.o ../replace/*.ho
    error_check $? "Step1"

    ./autogen.sh
    error_check $? "Step1"

    ./configure --prefix=/usr/local/samba
    error_check $? "Step1"

    make
    error_check $? "Step1"

    sudo make install
    error_check $? "Step1"

    make realdistclean
    error_check $? "Step1"

    rm -rf ../replace/*.o ../replace/*.ho
    error_check $? "Step1"

    cd $OLD_PWD

    echo "Step2: Installing tdb library"

    cd samba4/source/lib/tdb
    error_check $? "Step2"

    ./autogen.sh
    error_check $? "Step2"

    ./configure --prefix=/usr/local/samba
    error_check $? "Step2"

    make
    error_check $? "Step2"

    sudo make install
    error_check $? "Step2"

    make realdistclean
    error_check $? "Step2"

    rm -rf ../replace/*.o ../replace/*.ho
    error_check $? "Step2"

    cd $OLD_PWD
}

#
# Compile Samba4
#
compile() {

    OLD_PWD=$PWD

    PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/samba/lib/pkgconfig

	cleanup_talloc
	cleanup_tdb

	# Cleanup tdb and talloc directories

    echo "Step1: Preparing Samba4 system"
    cd samba4/source
    error_check $? "Step1"

    ./autogen.sh
    error_check $? "Step1"

    ./configure.developer
    error_check $? "Step1"

    echo "Step2: Compile Samba4"
    make
    error_check $? "Step2"

    cd $OLD_PWD
}


#
# Install Samba4
#
install() {

    OLD_PWD=$PWD

    echo "Step1: Installing Samba"
    echo "===> we are in $PWD"
    cd samba4/source
    error_check $? "Step1"

    sudo make install
    error_check $? "Step1"

    cd $OLD_PWD
}


#
# main program
#
case $1 in
    checkout)
	checkout
	;;
    packages)
	packages
	;;
    compile)
	compile
	;;
    install)
	install
	;;
    all)
	checkout
	packages
	compile
	install
	;;
    *)
	echo $"Usage: $0 {checkout|packages|compile|install|all}"
	;;
esac

exit 0