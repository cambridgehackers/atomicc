# atomicc
Generate Verilog from Atomicc IR files (which are generated from cambridgehackers/llvm)

Note that the repo cambridgehackers/llvm must be checked out at the same level as this repo
(outside this directory) so that the file ../llvm/lib/Target/Atomicc/AtomiccIR.h can be used
during build.


To build

0) get sources
    sudo apt-get install libffi-dev libblocksruntime-dev clang
    git clone git://github.com/cambridgehackers/atomicc
    git clone git://github.com/cambridgehackers/atomicc-examples
    git clone git://github.com/cambridgehackers/llvm
    git clone git://github.com/cambridgehackers/clang
    cd clang; git checkout remotes/origin/release_50atomicc1 -b release_50atomicc1
    cd ../llvm; git checkout remotes/origin/release_50atomicc1 -b release_50atomicc1

1) build source
    cd llvm 
    mkdir build
    cd build
    bash ../configure_atomicc
    make -j10
    cd ../../atomicc
    make

2) cd atomicc-examples/examples/rulec

    make 
    make build
    make run

3) examine output in:

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

