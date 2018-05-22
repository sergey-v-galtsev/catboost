UNITTEST(targets_tests)



IF (NOT AUTOCHECK)
SRCS(
    test_query_cross_entropy.cpp
)
ENDIF()


PEERDIR(
    catboost/cuda/targets
)

CUDA_NVCC_FLAGS(
     -gencode arch=compute_30,code=compute_30
       -gencode arch=compute_35,code=sm_35
       -gencode arch=compute_50,code=compute_50
       -gencode arch=compute_52,code=sm_52
       -gencode arch=compute_60,code=compute_60
       -gencode arch=compute_61,code=compute_61
       -gencode arch=compute_61,code=sm_61
       -gencode arch=compute_70,code=sm_70
       -gencode arch=compute_70,code=compute_70
)

ALLOCATOR(LF)

END()
