
# Configures LLVM dependency
#
# This function handles everything needed to setup LLVM project.
# By default it downloads and builds LLVM from source.
# In case LLVM_DIR variable is set it tries to use the pointed pre-built
# LLVM package. LLVM_DIR should point LLVM's shared cmake files to be used
# by find_package(... CONFIG) function.
#
# Creates a target representing all required LLVM libraries and include path.
function(configure_llvm_project)
    if (LLVM_DIR)
        find_package(LLVM REQUIRED CONFIG)
        llvm_map_components_to_libnames(LIBS mcjit ipo x86codegen)

        # To create a fake imported library later on we need to know the
        # location of some library
        list(GET LIBS 0 MAIN_LIB)
        get_property(CONFIGS TARGET ${MAIN_LIB} PROPERTY IMPORTED_CONFIGURATIONS)
        list(GET CONFIGS 0 CONFIG)  # Just get the first one. Usually there is only one.
        if (CONFIG)
            get_property(MAIN_LIB TARGET ${MAIN_LIB} PROPERTY IMPORTED_LOCATION_${CONFIG})
        else()
            set(CONFIG Unknown)
            get_property(MAIN_LIB TARGET ${MAIN_LIB} PROPERTY IMPORTED_LOCATION)
        endif()
        message(STATUS "LLVM ${LLVM_VERSION} (${CONFIG}; ${LLVM_ENABLE_ASSERTIONS}; ${LLVM_DIR})")
        if (NOT EXISTS ${MAIN_LIB})
            # Add some diagnostics to detect issues before building.
            message(FATAL_ERROR "LLVM library not found: ${MAIN_LIB}")
        endif()
    else()
        # List of required LLVM libs.
        # Generated with `llvm-config --libs mcjit ipo x86codegen`
        # Only used here locally to setup the "llvm" imported target
        set(LIBS
            LLVMMCJIT
            LLVMX86CodeGen LLVMGlobalISel LLVMX86Desc LLVMX86Info LLVMMCDisassembler
            LLVMX86AsmPrinter LLVMX86Utils LLVMSelectionDAG LLVMAsmPrinter LLVMDebugInfoCodeView
            LLVMDebugInfoMSF LLVMCodeGen LLVMipo LLVMInstrumentation LLVMVectorize LLVMScalarOpts
            LLVMLinker LLVMIRReader LLVMAsmParser LLVMInstCombine LLVMTransformUtils LLVMBitWriter
            LLVMExecutionEngine LLVMTarget LLVMAnalysis LLVMProfileData LLVMRuntimeDyld LLVMObject
            LLVMMCParser LLVMBitReader LLVMMC LLVMCore LLVMBinaryFormat LLVMSupport LLVMDemangle
        )

        # System libs that LLVM depend on.
        # See `llvm-config --system-libs`
        if (APPLE)
            set(SYSTEM_LIBS pthread)
        elseif (UNIX)
            set(SYSTEM_LIBS pthread dl)
        endif()
        
        if (${CMAKE_GENERATOR} STREQUAL "Unix Makefiles")
            set(BUILD_COMMAND $(MAKE))
        else()
            if (LLVM_BUILD_TYPE)
                set(LLVM_CONFIG_TYPE "${LLVM_BUILD_TYPE}")
            else()
                set(LLVM_CONFIG_TYPE "Release")
            endif()
            set(BUILD_COMMAND cmake --build <BINARY_DIR> --config ${LLVM_CONFIG_TYPE})
        endif()
        message("LLVM_CONFIG_TYPE: ${LLVM_CONFIG_TYPE}")
        
        include(ExternalProject)
        ExternalProject_Add(llvm
            PREFIX ${CMAKE_SOURCE_DIR}/deps
            URL http://llvm.org/releases/5.0.0/llvm-5.0.0.src.tar.xz
            URL_HASH SHA256=e35dcbae6084adcf4abb32514127c5eabd7d63b733852ccdb31e06f1373136da
            DOWNLOAD_NO_PROGRESS TRUE
            BINARY_DIR ${CMAKE_SOURCE_DIR}/deps  # Build directly to install dir to avoid copy.
            CMAKE_ARGS -DCMAKE_BUILD_TYPE=${LLVM_CONFIG_TYPE}
                       -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                       -DLLVM_ENABLE_TERMINFO=OFF  # Disable terminal color support
                       -DLLVM_ENABLE_ZLIB=OFF      # Disable compression support -- not needed at all
                       -DLLVM_USE_CRT_DEBUG=MTd
                       -DLLVM_USE_CRT_RELEASE=MT
                       -DLLVM_TARGETS_TO_BUILD=X86
                       -DLLVM_INCLUDE_TOOLS=OFF
                       -DLLVM_INCLUDE_EXAMPLES=OFF
                       -DLLVM_INCLUDE_TESTS=OFF
            LOG_CONFIGURE TRUE
            BUILD_COMMAND   ${BUILD_COMMAND}
            INSTALL_COMMAND cmake --build <BINARY_DIR> --config ${LLVM_CONFIG_TYPE} --target install
            LOG_INSTALL TRUE
            EXCLUDE_FROM_ALL TRUE
        )

        ExternalProject_Get_Property(llvm INSTALL_DIR)
        set(LLVM_LIBRARY_DIRS ${INSTALL_DIR}/lib)
        set(LLVM_INCLUDE_DIRS ${INSTALL_DIR}/include)
        file(MAKE_DIRECTORY ${LLVM_INCLUDE_DIRS})  # Must exists.

        foreach(LIB ${LIBS})
            list(APPEND LIBFILES "${LLVM_LIBRARY_DIRS}/${CMAKE_STATIC_LIBRARY_PREFIX}${LIB}${CMAKE_STATIC_LIBRARY_SUFFIX}")
        endforeach()

        # Pick one of the libraries to be the main one. It does not matter which one
        # but the imported target requires the IMPORTED_LOCATION property.
        list(GET LIBFILES 0 MAIN_LIB)
        list(REMOVE_AT LIBFILES 0)
        set(LIBS ${LIBFILES} ${SYSTEM_LIBS})
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang needs this to build LLVM. Weird that the GCC does not.
        set(DEFINES __STDC_LIMIT_MACROS __STDC_CONSTANT_MACROS)
    endif()

    # Create the target representing
    add_library(LLVM::JIT STATIC IMPORTED)
    set_property(TARGET LLVM::JIT PROPERTY IMPORTED_CONFIGURATIONS Release)
    set_property(TARGET LLVM::JIT PROPERTY IMPORTED_LOCATION_RELEASE ${MAIN_LIB})
    set_property(TARGET LLVM::JIT PROPERTY INTERFACE_COMPILE_DEFINITIONS ${DEFINES})
    set_property(TARGET LLVM::JIT PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LLVM_INCLUDE_DIRS})
    set_property(TARGET LLVM::JIT PROPERTY INTERFACE_LINK_LIBRARIES ${LIBS})
    if (TARGET llvm)
        add_dependencies(LLVM::JIT llvm)
    endif()
endfunction()
