# Relies on settings in environment. These can be set by modules or in make.inc.
# Set compiler by $CXX; usually want CXX=mpicxx.
# Add include directories to $CPATH or $CXXFLAGS for MPI, CUDA, MKL, etc.
# Add lib directories to $LIBRARY_PATH or $LDFLAGS for MPI, CUDA, MKL, etc.
# At runtime, these lib directories need to be in $LD_LIBRARY_PATH,
# or on MacOS, $DYLD_LIBRARY_PATH, or set as rpaths in $LDFLAGS.
#
# Set options on command line or in make.inc file:
# CXX=mpicxx      or
# CXX=mpic++      for MPI using compiler wrapper.
# mpi=1           for MPI (-lmpi).
# spectrum=1      for IBM Spectrum MPI (-lmpi_ibm).
#
# mkl=1           for Intel MKL. Additional sub-options:
#     mkl_intel=1       	for Intel MKL with Intel Fortran conventions;
#	  				    	otherwise uses GNU gfortran conventions.
#	  				    	Automatically set if CXX=icpc or on macOS.
#     mkl_threaded=1    	for multi-threaded Intel MKL.
#     mkl_blacs=openmpi		for OpenMPI BLACS in SLATE's testers.
#     mkl_blacs=intelmpi    for Intel MPI BLACS in SLATE's testers (default).
#     ilp64=1           	for ILP64. Currently only with Intel MKL.
# essl=1          for IBM ESSL.
# openblas=1      for OpenBLAS.
#
# openmp=1        for OpenMP.
# static=1        for static library (libslate.a);
#                 otherwise shared library (libslate.so).
#
# cuda_arch="ARCH" for CUDA architectures, where ARCH is one or more of:
#                     kepler maxwell pascal volta turing sm_XX
#                  and sm_XX is a CUDA architecture (see nvcc -h).
# Setting cuda=1 will set cuda_arch="kepler pascal" by default.

-include make.inc

# Strip whitespace from variables, in case make.inc had trailing spaces.
mpi             := $(strip $(mpi))
spectrum        := $(strip $(spectrum))
mkl             := $(strip $(mkl))
mkl_intel       := $(strip $(mkl_intel))
mkl_threaded    := $(strip $(mkl_threaded))
mkl_blacs       := $(strip $(mkl_blacs))
ilp64           := $(strip $(ilp64))
essl            := $(strip $(essl))
openblas        := $(strip $(openblas))
openmp          := $(strip $(openmp))
static          := $(strip $(static))
cuda_arch       := $(strip $(cuda_arch))
cuda            := $(strip $(cuda))

# Warn about obsolete settings.
ifneq ($(openmpi),)
    $(error Variable `openmpi` is obsolete; use mkl_blacs=openmpi)
endif
ifneq ($(intelmpi),)
    $(error Variable `intelmpi` is obsolete; use mkl_blacs=intelmpi)
endif

# Export variables to sub-make for testsweeper, BLAS++, LAPACK++.
export CXX mkl ilp64 essl openblas openmp static

NVCC ?= nvcc

CXXFLAGS  += -O3 -std=c++11 -Wall -pedantic -MMD
NVCCFLAGS += -O3 -std=c++11 --compiler-options '-Wall -Wno-unused-function'

# auto-detect OS
# $OSTYPE may not be exported from the shell, so echo it
ostype = $(shell echo $${OSTYPE})
ifneq ($(findstring darwin, $(ostype)),)
    # MacOS is darwin
    macos = 1
endif

#-------------------------------------------------------------------------------
# if shared
ifneq ($(static),1)
    CXXFLAGS += -fPIC
    LDFLAGS  += -fPIC
    NVCCFLAGS += --compiler-options '-fPIC'
    lib_ext = so
else
    lib_ext = a
endif

#-------------------------------------------------------------------------------
# if OpenMP
ifeq ($(openmp),1)
    CXXFLAGS += -fopenmp
    LDFLAGS  += -fopenmp
else
    libslate_src += src/stubs/openmp_stubs.cc
endif

#-------------------------------------------------------------------------------
# if MPI
ifneq (,$(filter $(CXX),mpicxx mpic++))
    # CXX = mpicxx or mpic++
    # Generic MPI via compiler wrapper. No flags to set.
else ifeq ($(mpi),1)
    # Generic MPI.
    LIBS  += -lmpi
else ifeq ($(spectrum),1)
    # IBM Spectrum MPI
    LIBS  += -lmpi_ibm
else
    FLAGS += -DSLATE_NO_MPI
    libslate_src += src/stubs/mpi_stubs.cc
endif

#-------------------------------------------------------------------------------
# ScaLAPACK, by default
scalapack = -lscalapack

# BLAS and LAPACK
# if MKL
ifeq ($(mkl_threaded),1)
    mkl = 1
endif
ifeq ($(mkl_intel),1)
    mkl = 1
endif
ifeq ($(mkl),1)
    FLAGS += -DSLATE_WITH_MKL
    # Auto-detect whether to use Intel or GNU conventions.
    # Won't detect if CXX = mpicxx.
    ifeq ($(CXX),icpc)
        mkl_intel = 1
    endif
    ifeq ($(macos),1)
        # MKL on MacOS (version 20180001) has only Intel Fortran version
        mkl_intel = 1
    endif
    ifeq ($(mkl_intel),1)
        # use Intel Fortran conventions
        ifeq ($(ilp64),1)
            LIBS += -lmkl_intel_ilp64
        else
            LIBS += -lmkl_intel_lp64
        endif

        # if threaded, use Intel OpenMP (iomp5)
        ifeq ($(mkl_threaded),1)
            LIBS += -lmkl_intel_thread
        else
            LIBS += -lmkl_sequential
        endif
    else
        # use GNU Fortran conventions
        ifeq ($(ilp64),1)
            LIBS += -lmkl_gf_ilp64
        else
            LIBS += -lmkl_gf_lp64
        endif

        # if threaded, use GNU OpenMP (gomp)
        ifeq ($(mkl_threaded),1)
            LIBS += -lmkl_gnu_thread
        else
            LIBS += -lmkl_sequential
        endif
    endif

    LIBS += -lmkl_core -lpthread -lm -ldl

    # MKL on MacOS doesn't include ScaLAPACK; use default.
    # For others, link with appropriate version of ScaLAPACK and BLACS.
    ifneq ($(macos),1)
        ifeq ($(mkl_blacs),openmpi)
            ifeq ($(ilp64),1)
                scalapack = -lmkl_scalapack_ilp64 -lmkl_blacs_openmpi_ilp64
            else
                scalapack = -lmkl_scalapack_lp64 -lmkl_blacs_openmpi_lp64
            endif
        else
            ifeq ($(ilp64),1)
                scalapack = -lmkl_scalapack_ilp64 -lmkl_blacs_intelmpi_ilp64
            else
                scalapack = -lmkl_scalapack_lp64 -lmkl_blacs_intelmpi_lp64
            endif
        endif
    endif
# if ESSL
else ifeq ($(essl),1)
    FLAGS += -DSLATE_WITH_ESSL
    LIBS += -lessl -llapack
# if OpenBLAS
else ifeq ($(openblas),1)
    FLAGS += -DSLATE_WITH_OPENBLAS
    LIBS += -lopenblas
endif

#-------------------------------------------------------------------------------
# cuda_arch implies cuda, if $cuda not already set.
ifneq ($(cuda_arch),)
    ifeq ($(cuda),)
        cuda = 1
    endif
endif

# if CUDA
ifeq ($(cuda),1)
    # Set default cuda_arch if not already set.
    ifeq ($(cuda_arch),)
        cuda_arch = kepler pascal
    endif

    # Generate flags for which CUDA architectures to build.
    # cuda_arch_ is a local copy to modify.
    cuda_arch_ = $(cuda_arch)
    ifneq ($(findstring kepler, $(cuda_arch_)),)
        cuda_arch_ += sm_30
    endif
    ifneq ($(findstring maxwell, $(cuda_arch_)),)
        cuda_arch_ += sm_50
    endif
    ifneq ($(findstring pascal, $(cuda_arch_)),)
        cuda_arch_ += sm_60
    endif
    ifneq ($(findstring volta, $(cuda_arch_)),)
        cuda_arch_ += sm_70
    endif
    ifneq ($(findstring turing, $(cuda_arch_)),)
        cuda_arch_ += sm_75
    endif

    # CUDA architectures that nvcc supports
    sms = 30 32 35 37 50 52 53 60 61 62 70 72 75

    # code=sm_XX is binary, code=compute_XX is PTX
    gencode_sm      = -gencode arch=compute_$(sm),code=sm_$(sm)
    gencode_compute = -gencode arch=compute_$(sm),code=compute_$(sm)

    # Get gencode options for all sm_XX in cuda_arch_.
    nv_sm      = $(filter %, $(foreach sm, $(sms),$(if $(findstring sm_$(sm), $(cuda_arch_)),$(gencode_sm))))
    nv_compute = $(filter %, $(foreach sm, $(sms),$(if $(findstring sm_$(sm), $(cuda_arch_)),$(gencode_compute))))

    ifeq ($(nv_sm),)
        $(error ERROR: set cuda_arch, currently '$(cuda_arch)', to one of kepler, maxwell, pascal, volta, turing, or valid sm_XX from nvcc -h)
    else
        # Get last option (last 2 words) of nv_compute.
        nwords  = $(words $(nv_compute))
        nwords_1 = $(shell expr $(nwords) - 1)
        nv_compute_last = $(wordlist $(nwords_1), $(nwords), $(nv_compute))
    endif

    # Use all sm_XX (binary), and the last compute_XX (PTX) for forward compatibility.
    NVCCFLAGS += $(nv_sm) $(nv_compute_last)
    LIBS += -lcublas -lcudart
else
    FLAGS += -DSLATE_NO_CUDA
    libslate_src += src/stubs/cuda_stubs.cc
    libslate_src += src/stubs/cublas_stubs.cc
endif

#-------------------------------------------------------------------------------
# MacOS needs shared library's path set
ifeq ($(macos),1)
    install_name = -install_name @rpath/$(notdir $@)
else
    install_name =
endif

#-------------------------------------------------------------------------------
# Files

# types and classes
libslate_src += \
        src/aux/Debug.cc \
        src/aux/Exception.cc \
        src/core/Memory.cc \
        src/aux/Trace.cc \
        src/core/types.cc \

# internal
libslate_src += \
        src/internal/internal_comm.cc \
        src/internal/internal_copyhb2st.cc \
        src/internal/internal_copytb2bd.cc \
        src/internal/internal_gecopy.cc \
        src/internal/internal_gbnorm.cc \
        src/internal/internal_geadd.cc \
        src/internal/internal_gemm.cc \
        src/internal/internal_gemm_A.cc \
        src/internal/internal_gemm_split.cc \
        src/internal/internal_genorm.cc \
        src/internal/internal_gebr.cc \
        src/internal/internal_geqrf.cc \
        src/internal/internal_geset.cc \
        src/internal/internal_getrf.cc \
        src/internal/internal_hebr.cc \
        src/internal/internal_hemm.cc \
        src/internal/internal_hbnorm.cc \
        src/internal/internal_henorm.cc \
        src/internal/internal_her2k.cc \
        src/internal/internal_herk.cc \
        src/internal/internal_hettmqr.cc \
        src/internal/internal_potrf.cc \
        src/internal/internal_swap.cc \
        src/internal/internal_symm.cc \
        src/internal/internal_synorm.cc \
        src/internal/internal_syr2k.cc \
        src/internal/internal_syrk.cc \
        src/internal/internal_transpose.cc \
        src/internal/internal_trmm.cc \
        src/internal/internal_trnorm.cc \
        src/internal/internal_trsm.cc \
        src/internal/internal_trtri.cc \
        src/internal/internal_trtrm.cc \
        src/internal/internal_ttmqr.cc \
        src/internal/internal_ttmlq.cc \
        src/internal/internal_ttqrt.cc \
        src/internal/internal_ttlqt.cc \
        src/internal/internal_tzcopy.cc \
        src/internal/internal_unmqr.cc \
        src/internal/internal_unmlq.cc \
        src/internal/internal_util.cc \

# device
ifeq ($(cuda),1)
    libslate_src += \
            src/cuda/device_geadd.cu \
            src/cuda/device_gecopy.cu \
            src/cuda/device_genorm.cu \
            src/cuda/device_geset.cu \
            src/cuda/device_henorm.cu \
            src/cuda/device_synorm.cu \
            src/cuda/device_transpose.cu \
            src/cuda/device_trnorm.cu \
            src/cuda/device_tzcopy.cu \

endif

# driver
libslate_src += \
        src/bdsqr.cc \
        src/colNorms.cc \
        src/copy.cc \
        src/gbmm.cc \
        src/gbsv.cc \
        src/gbtrf.cc \
        src/gbtrs.cc \
        src/ge2tb.cc \
        src/geadd.cc \
        src/gels.cc \
        src/gemm.cc \
        src/geqrf.cc \
        src/gelqf.cc \
        src/gesv.cc \
        src/gesvd.cc \
        src/gesvMixed.cc \
        src/getrf.cc \
        src/getri.cc \
        src/getriOOP.cc \
        src/getrs.cc \
        src/hb2st.cc \
        src/he2hb.cc \
        src/heev.cc \
        src/hemm.cc \
        src/hbmm.cc \
        src/her2k.cc \
        src/herk.cc \
        src/hesv.cc \
        src/hetrf.cc \
        src/hetrs.cc \
        src/norm.cc \
        src/pbsv.cc \
        src/pbtrf.cc \
        src/pbtrs.cc \
        src/posv.cc \
        src/posvMixed.cc \
        src/potrf.cc \
        src/potri.cc \
        src/potrs.cc \
        src/set.cc \
        src/sterf.cc \
        src/symm.cc \
        src/syr2k.cc \
        src/syrk.cc \
        src/tb2bd.cc \
        src/tbsm.cc \
        src/tbsmPivots.cc \
        src/trmm.cc \
        src/trsm.cc \
        src/trtri.cc \
        src/trtrm.cc \
        src/unmqr.cc \
        src/unmlq.cc \

# main tester
tester_src += \
        test/test.cc \
        test/test_bdsqr.cc \
        test/test_gbmm.cc \
        test/test_gbnorm.cc \
        test/test_gbsv.cc \
        test/test_ge2tb.cc \
        test/test_gels.cc \
        test/test_gemm.cc \
        test/test_genorm.cc \
        test/test_geqrf.cc \
        test/test_gelqf.cc \
        test/test_gesv.cc \
        test/test_gesvd.cc \
        test/test_getri.cc \
        test/test_he2hb.cc \
        test/test_heev.cc \
        test/test_hemm.cc \
        test/test_hbmm.cc \
        test/test_hbnorm.cc \
        test/test_henorm.cc \
        test/test_her2k.cc \
        test/test_herk.cc \
        test/test_hesv.cc \
        test/test_posv.cc \
        test/test_pbsv.cc \
        test/test_potri.cc \
        test/test_symm.cc \
        test/test_synorm.cc \
        test/test_syr2k.cc \
        test/test_syrk.cc \
        test/test_sterf.cc \
        test/test_tb2bd.cc \
        test/test_tbsm.cc \
        test/test_trmm.cc \
        test/test_trnorm.cc \
        test/test_trsm.cc \
        test/test_trtri.cc \


# Compile fixes for ScaLAPACK routines if Fortran compiler $(FC) exists.
# Note that 'make' sets $(FC) to f77 by default.
FORTRAN = $(shell which $(FC))
ifneq ($(FORTRAN),)
    tester_src += \
        test/pslange.f \
        test/pdlange.f \
        test/pclange.f \
        test/pzlange.f \
        test/pslansy.f \
        test/pdlansy.f \
        test/pclansy.f \
        test/pzlansy.f \
        test/pslantr.f \
        test/pdlantr.f \
        test/pclantr.f \
        test/pzlantr.f \

endif

# unit testers
unit_src = \
        unit_test/test_BandMatrix.cc \
        unit_test/test_HermitianMatrix.cc \
        unit_test/test_Matrix.cc \
        unit_test/test_Memory.cc \
        unit_test/test_SymmetricMatrix.cc \
        unit_test/test_Tile.cc \
        unit_test/test_Tile_kernels.cc \
        unit_test/test_TrapezoidMatrix.cc \
        unit_test/test_TriangularMatrix.cc \
        unit_test/test_lq.cc \
        unit_test/test_norm.cc \
        unit_test/test_qr.cc \

# unit test framework
unit_test_obj = \
        unit_test/unit_test.o

libslate_obj = $(addsuffix .o, $(basename $(libslate_src)))
tester_obj   = $(addsuffix .o, $(basename $(tester_src)))
unit_obj     = $(addsuffix .o, $(basename $(unit_src)))
dep          = $(addsuffix .d, $(basename $(libslate_src) $(tester_src) \
                                          $(unit_src) $(unit_test_obj)))

tester    = test/tester
unit_test = $(basename $(unit_src))

#-------------------------------------------------------------------------------
# SLATE specific flags and libraries
# FLAGS accumulates definitions, include dirs, etc. for both CXX and NVCC.
# FLAGS += -I.
FLAGS += -I./blaspp/include
FLAGS += -I./lapackpp/include
FLAGS += -I./include
FLAGS += -I./src

CXXFLAGS  += $(FLAGS)
NVCCFLAGS += $(FLAGS)

# libraries to create libslate.so
LDFLAGS  += -L./blaspp/lib -Wl,-rpath,$(abspath ./blaspp/lib)
LDFLAGS  += -L./lapackpp/lib -Wl,-rpath,$(abspath ./lapackpp/lib)
LIBS     := -lblaspp -llapackpp $(LIBS)

# additional flags and libraries for testers
$(tester_obj):    CXXFLAGS += -I./testsweeper
$(unit_obj):      CXXFLAGS += -I./testsweeper
$(unit_test_obj): CXXFLAGS += -I./testsweeper

TEST_LDFLAGS += -L./lib -Wl,-rpath,$(abspath ./lib)
TEST_LDFLAGS += -L./testsweeper -Wl,-rpath,$(abspath ./testsweeper)
TEST_LIBS    += -lslate -ltestsweeper $(scalapack)

UNIT_LDFLAGS += -L./lib -Wl,-rpath,$(abspath ./lib)
UNIT_LDFLAGS += -L./testsweeper -Wl,-rpath,$(abspath ./testsweeper)
UNIT_LIBS    += -lslate -ltestsweeper

#-------------------------------------------------------------------------------
# Rules
.DELETE_ON_ERROR:
.SUFFIXES:
.PHONY: all docs lib test tester unit_test clean distclean testsweeper blaspp lapackpp
.DEFAULT_GOAL := all

all: lib tester unit_test scalapack_api lapack_api

docs:
	doxygen docs/doxygen/doxyfile.conf

#-------------------------------------------------------------------------------
# testsweeper library
testsweeper_src = $(wildcard testsweeper/*.hh testsweeper/*.cc)

testsweeper = testsweeper/libtestsweeper.$(lib_ext)

$(testsweeper): $(testsweeper_src)
	cd testsweeper && $(MAKE) lib

testsweeper: $(testsweeper)

#-------------------------------------------------------------------------------
# BLAS++ library
libblaspp_src = $(wildcard blaspp/include/*.h \
                           blaspp/include/*.hh \
                           blaspp/src/*.cc)

libblaspp = blaspp/lib/libblaspp.$(lib_ext)

# dependency on testsweeper serializes compiles
$(libblaspp): $(libblaspp_src) | $(testsweeper)
	cd blaspp && $(MAKE) lib

blaspp: $(libblaspp)

#-------------------------------------------------------------------------------
# LAPACK++ library
liblapackpp_src = $(wildcard lapackpp/include/*.h \
                             lapackpp/include/*.hh \
                             lapackpp/src/*.cc)

liblapackpp = lapackpp/lib/liblapackpp.$(lib_ext)

# dependency on testsweeper, BLAS++ serializes compiles
$(liblapackpp): $(liblapackpp_src) | $(testsweeper) $(libblaspp)
	cd lapackpp && $(MAKE) lib

lapackpp: $(liblapackpp)

#-------------------------------------------------------------------------------
# libslate library
libslate_a  = lib/libslate.a
libslate_so = lib/libslate.so
libslate    = lib/libslate.$(lib_ext)

$(libslate_a): $(libslate_obj) $(libblaspp) $(liblapackpp)
	mkdir -p lib
	-rm $@
	ar cr $@ $(libslate_obj)
	ranlib $@

$(libslate_so): $(libslate_obj) $(libblaspp) $(liblapackpp)
	mkdir -p lib
	$(CXX) $(LDFLAGS) \
		$(libslate_obj) \
		$(LIBS) \
		-shared $(install_name) -o $@

lib: $(libslate)
src: lib

#-------------------------------------------------------------------------------
# headers
# precompile headers to verify self-sufficiency
headers     = $(wildcard include/*/*.hh test/*.hh)
headers_gch = $(addsuffix .gch, $(basename $(headers)))

headers: $(headers_gch)

# sub-directory rules
include: headers

include/clean:
	$(RM) include/*/*.gch test/*.gch

#-------------------------------------------------------------------------------
# main tester
# Note 'test' is sub-directory rule; 'tester' is CMake-compatible rule.
test: $(tester)
tester: $(tester)

test/clean:
	rm -f $(tester) $(tester_obj)

$(tester): $(tester_obj) $(libslate) $(testsweeper)
	$(CXX) $(TEST_LDFLAGS) $(LDFLAGS) $(tester_obj) \
		$(TEST_LIBS) $(LIBS) \
		-o $@

#-------------------------------------------------------------------------------
# unit testers
unit_test: $(unit_test)

unit_test/clean:
	rm -f $(unit_test) $(unit_obj) $(unit_test_obj)

$(unit_test): %: %.o $(unit_test_obj) $(libslate)
	$(CXX) $(UNIT_LDFLAGS) $(LDFLAGS) $< \
		$(unit_test_obj) $(UNIT_LIBS) $(LIBS)  \
		-o $@

#-------------------------------------------------------------------------------
# scalapack_api library
scalapack_api_a  = lib/libslate_scalapack_api.a
scalapack_api_so = lib/libslate_scalapack_api.so
scalapack_api    = lib/libslate_scalapack_api.$(lib_ext)

scalapack_api_src += \
        scalapack_api/scalapack_gels.cc \
        scalapack_api/scalapack_gemm.cc \
        scalapack_api/scalapack_getrf.cc \
        scalapack_api/scalapack_getrs.cc \
        scalapack_api/scalapack_gesv.cc \
        scalapack_api/scalapack_hemm.cc \
        scalapack_api/scalapack_her2k.cc \
        scalapack_api/scalapack_herk.cc \
        scalapack_api/scalapack_lanhe.cc \
        scalapack_api/scalapack_lange.cc \
        scalapack_api/scalapack_lansy.cc \
        scalapack_api/scalapack_lantr.cc \
        scalapack_api/scalapack_potrf.cc \
        scalapack_api/scalapack_potri.cc \
        scalapack_api/scalapack_posv.cc \
        scalapack_api/scalapack_symm.cc \
        scalapack_api/scalapack_syr2k.cc \
        scalapack_api/scalapack_syrk.cc \
        scalapack_api/scalapack_trmm.cc \
        scalapack_api/scalapack_trsm.cc \

scalapack_api_obj = $(addsuffix .o, $(basename $(scalapack_api_src)))

dep += $(addsuffix .d, $(basename $(scalapack_api_src)))

SCALAPACK_API_LDFLAGS += -L./lib -Wl,-rpath,$(abspath ./lib)
SCALAPACK_API_LIBS    += -lslate $(scalapack)

scalapack_api: $(scalapack_api)

scalapack_api/clean:
	rm -f $(scalapack_api) $(scalapack_api_obj)

$(scalapack_api_a): $(scalapack_api_obj) $(libslate)
	-rm $@
	ar cr $@ $(scalapack_api_obj)
	ranlib $@

$(scalapack_api_so): $(scalapack_api_obj) $(libslate)
	$(CXX) $(SCALAPACK_API_LDFLAGS) $(LDFLAGS) $(scalapack_api_obj) \
		$(SCALAPACK_API_LIBS) $(LIBS) -shared $(install_name) -o $@

#-------------------------------------------------------------------------------
# lapack_api library
lapack_api_a  = lib/libslate_lapack_api.a
lapack_api_so = lib/libslate_lapack_api.so
lapack_api    = lib/libslate_lapack_api.$(lib_ext)

lapack_api_src += \
        lapack_api/lapack_gels.cc \
        lapack_api/lapack_gemm.cc \
        lapack_api/lapack_gesv.cc \
        lapack_api/lapack_gesvMixed.cc \
        lapack_api/lapack_getrf.cc \
        lapack_api/lapack_getrs.cc \
        lapack_api/lapack_hemm.cc \
        lapack_api/lapack_her2k.cc \
        lapack_api/lapack_herk.cc \
        lapack_api/lapack_lange.cc \
        lapack_api/lapack_lanhe.cc \
        lapack_api/lapack_lansy.cc \
        lapack_api/lapack_lantr.cc \
        lapack_api/lapack_posv.cc \
        lapack_api/lapack_potrf.cc \
        lapack_api/lapack_potri.cc \
        lapack_api/lapack_symm.cc \
        lapack_api/lapack_syr2k.cc \
        lapack_api/lapack_syrk.cc \
        lapack_api/lapack_trmm.cc \
        lapack_api/lapack_trsm.cc \


lapack_api_obj = $(addsuffix .o, $(basename $(lapack_api_src)))

dep += $(addsuffix .d, $(basename $(lapack_api_src)))

LAPACK_API_LDFLAGS += -L./lib -Wl,-rpath,$(abspath ./lib)
LAPACK_API_LIBS    += -lslate

lapack_api: $(lapack_api)

lapack_api/clean:
	rm -f $(lapack_api) $(lapack_api_obj)

$(lapack_api_a): $(lapack_api_obj) $(libslate)
	-rm $@
	ar cr $@ $(lapack_api_obj)
	ranlib $@

$(lapack_api_so): $(lapack_api_obj) $(libslate)
	$(CXX) $(LAPACK_API_LDFLAGS) $(LDFLAGS) $(lapack_api_obj) \
		$(LAPACK_API_LIBS) $(LIBS) -shared $(install_name) -o $@

#-------------------------------------------------------------------------------
# general rules
clean: test/clean unit_test/clean scalapack_api/clean lapack_api/clean include/clean
	rm -f $(libslate_a) $(libslate_so) $(libslate_obj)
	rm -f trace_*.svg

distclean: clean
	rm -f $(dep)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.f
	$(FC) $(FCFLAGS) -c $< -o $@

%.o: %.cu
	$(NVCC) $(NVCCFLAGS) -c $< -o $@

# preprocess source
# test/%.i depend on testsweeper; for simplicity just add it here.
%.i: %.cc
	$(CXX) $(CXXFLAGS) -I./testsweeper -E $< -o $@

# precompile header to check for errors
# test/%.gch depend on testsweeper; for simplicity just add it here.
%.gch: %.hh
	$(CXX) $(CXXFLAGS) -I./testsweeper -c $< -o $@

-include $(dep)

#-------------------------------------------------------------------------------
# debugging
echo:
	@echo "mpi           = '$(mpi)'"
	@echo "spectrum      = '$(spectrum)'"
	@echo "mkl           = '$(mkl)'"
	@echo "mkl_intel     = '$(mkl_intel)'"
	@echo "mkl_threaded  = '$(mkl_threaded)'"
	@echo "mkl_blacs     = '$(mkl_blacs)'"
	@echo "ilp64         = '$(ilp64)'"
	@echo "essl          = '$(essl)'"
	@echo "openblas      = '$(openblas)'"
	@echo "openmp        = '$(openmp)'"
	@echo "static        = '$(static)'"
	@echo "cuda_arch     = '$(cuda_arch)'"
	@echo "cuda          = '$(cuda)'"
	@echo "macos         = '$(macos)'"
	@echo
	@echo "libblaspp     = $(libblaspp)"
	@echo "liblapackpp   = $(liblapackpp)"
	@echo "testsweeper   = $(testsweeper)"
	@echo
	@echo "libslate_a    = $(libslate_a)"
	@echo "libslate_so   = $(libslate_so)"
	@echo "libslate      = $(libslate)"
	@echo
	@echo "libslate_obj  = $(libslate_obj)"
	@echo
	@echo "tester_src    = $(tester_src)"
	@echo
	@echo "tester_obj    = $(tester_obj)"
	@echo
	@echo "tester        = $(tester)"
	@echo
	@echo "unit_src      = $(unit_src)"
	@echo
	@echo "unit_obj      = $(unit_obj)"
	@echo
	@echo "unit_test_obj = $(unit_test_obj)"
	@echo
	@echo "unit_test     = $(unit_test)"
	@echo
	@echo "dep           = $(dep)"
	@echo
	@echo "CXX           = $(CXX)"
	@echo "CXXFLAGS      = $(CXXFLAGS)"
	@echo
	@echo "NVCC          = $(NVCC)"
	@echo "NVCCFLAGS     = $(NVCCFLAGS)"
	@echo "cuda_arch     = $(cuda_arch)"
	@echo "cuda_arch_    = $(cuda_arch_)"
	@echo "sms           = $(sms)"
	@echo "nv_sm         = $(nv_sm)"
	@echo "nv_compute    = $(nv_compute)"
	@echo "nwords        = $(nwords)"
	@echo "nwords_1      = $(nwords_1)"
	@echo "nv_compute_last = $(nv_compute_last)"
	@echo
	@echo "FC            = $(FC)"
	@echo "FCFLAGS       = $(FCFLAGS)"
	@echo
	@echo "LDFLAGS       = $(LDFLAGS)"
	@echo "LIBS          = $(LIBS)"
	@echo
	@echo "TEST_LDFLAGS  = $(TEST_LDFLAGS)"
	@echo "TEST_LIBS     = $(TEST_LIBS)"
	@echo
	@echo "UNIT_LDFLAGS  = $(UNIT_LDFLAGS)"
	@echo "UNIT_LIBS     = $(UNIT_LIBS)"
