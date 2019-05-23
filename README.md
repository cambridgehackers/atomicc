# atomicc
Generate Verilog from Atomicc IR files (which are generated from cambridgehackers/llvm)

Note that the repo cambridgehackers/llvm must be checked out at the same level as this repo
(outside this directory) so that the file ../llvm/lib/Target/Atomicc/AtomiccIR.h can be used
during build.


To build

0) prepare machine
    sudo apt-get install libffi-dev libblocksruntime-dev clang
    sudo apt-get install gcc-multilib g++-multilib
0) On OSX:
    git clone git://github.com/mackyle/blocksruntime.git
    cd blocksruntime
    make
    make install

1) get sources
    git clone git://github.com/cambridgehackers/atomicc
    git clone git://github.com/cambridgehackers/atomicc-examples
    git clone git://github.com/cambridgehackers/cudd
    git clone git://github.com/cambridgehackers/llvm
    git clone git://github.com/cambridgehackers/clang
    git clone git://github.com/cambridgehackers/verilator
    git clone git://github.com/cambridgehackers/connectal
    cd clang; git checkout remotes/origin/release_50atomicc1 -b release_50atomicc1
    cd ../llvm; git checkout remotes/origin/release_50atomicc1 -b release_50atomicc1
    cd ../verilator; git checkout remotes/origin/atomicc1 -b atomicc1

2) build source
    cd llvm 
    mkdir build
    cd build
    bash ../configure_atomicc
    make -j10
    cd ../../cudd
    ./configure
    make
    cd ../atomicc
    make

3) cd atomicc-examples/examples/rulec

    To run on verilator:
        make 
        make verilator
        make run
    To build for zybo:
        make
        make zybo
    To clean up afterward:
        make clean

4) examine output in:

    ls *.generated.*

###############################################################
############ Other maintenence information ###########
To update to newer llvm releases:

    git clone git@github.com:cambridgehackers/llvm

    From https://help.github.com/articles/syncing-a-fork/  :

    git remote add upstream git@github.com:llvm-mirror/llvm.git
    git fetch upstream
    git checkout master
    git pull upstream master
    git remote -v
    git push origin master
    git checkout remotes/upstream/release_38 -b release_38
    git push origin release_38
    git checkout remotes/upstream/release_39 -b release_39
    git push origin release_39
    git checkout remotes/upstream/release_40 -b release_40
    git push origin release_40
    git checkout remotes/upstream/release_50 -b release_50
    git push origin release_50

----------------

8) Klee

    git clone https://github.com/niklasso/minisat
    cmake -DBUILD_SHARED_LIBS:BOOL=OFF ..

    git clone https://github.com/stp/stp.git
    cmake -DBUILD_SHARED_LIBS:BOOL=OFF -DENABLE_PYTHON_INTERFACE:BOOL=OFF -DNO_BOOST:BOOL=ON ..

