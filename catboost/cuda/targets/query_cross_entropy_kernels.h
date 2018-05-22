#pragma once

#include <catboost/cuda/cuda_lib/cuda_kernel_buffer.h>
#include <catboost/cuda/cuda_lib/cuda_buffer.h>
#include <catboost/cuda/cuda_lib/kernel.h>
#include <catboost/cuda/cuda_util/kernel/fill.cuh>
#include <catboost/cuda/cuda_util/kernel/transform.cuh>
#include <catboost/cuda/targets/kernel/query_cross_entropy.cuh>
#include <catboost/cuda/gpu_data/kernel/query_helper.cuh>

namespace NKernelHost {

    class TQueryCrossEntropyKernel: public  TKernelBase<NKernel::TQueryLogitContext, false> {
    private:
        double Alpha;
        TCudaBufferPtr<const float> Targets;
        TCudaBufferPtr<const float> Weights;
        TCudaBufferPtr<const float> Values;
        TCudaBufferPtr<const ui32> LoadIndices;
        TCudaBufferPtr<const ui32> Qids;
        TCudaBufferPtr<const bool> IsSingleClassQueries;
        TCudaBufferPtr<const ui32> QueryOffsets;
        TCudaBufferPtr<float> FunctionValue;
        TCudaBufferPtr<float> Ders;
        TCudaBufferPtr<float> Ders2llp;
        TCudaBufferPtr<float> Ders2llmax;
        TCudaBufferPtr<float> GroupDers2;

    public:
        using TKernelContext = NKernel::TQueryLogitContext;

        THolder<TKernelContext> PrepareContext(IMemoryManager& memoryManager) const {
            auto context = MakeHolder<TKernelContext>();
            context->QidCursor = memoryManager.Allocate<int, NCudaLib::EPtrType::CudaDevice>(1).Get();
            return context;
        }

        TQueryCrossEntropyKernel() = default;

        TQueryCrossEntropyKernel(double alpha,
                                 TCudaBufferPtr<const float> targets,
                                 TCudaBufferPtr<const float> weights,
                                 TCudaBufferPtr<const float> values,
                                 TCudaBufferPtr<const ui32> loadIndices,
                                 TCudaBufferPtr<const ui32> qids,
                                 TCudaBufferPtr<const bool> isSingleClassQueries,
                                 TCudaBufferPtr<const ui32> qOffsets,
                                 TCudaBufferPtr<float> functionValue,
                                 TCudaBufferPtr<float> ders,
                                 TCudaBufferPtr<float> ders2llp,
                                 TCudaBufferPtr<float> ders2llmax,
                                 TCudaBufferPtr<float> groupDers2)
                :  Alpha(alpha)
                  , Targets(targets)
                  , Weights(weights)
                  , Values(values)
                  , LoadIndices(loadIndices)
                  , Qids(qids)
                  , IsSingleClassQueries(isSingleClassQueries)
                  , QueryOffsets(qOffsets)
                  , FunctionValue(functionValue)
                  , Ders(ders)
                  , Ders2llp(ders2llp)
                  , Ders2llmax(ders2llmax)
                  , GroupDers2(groupDers2) {
        }

        Y_SAVELOAD_DEFINE(Alpha, Targets, Weights, Values, LoadIndices, Qids, IsSingleClassQueries,
                          QueryOffsets, FunctionValue, Ders, Ders2llp, Ders2llmax, GroupDers2);

        void Run(const TCudaStream& stream,
                 TKernelContext& context) const {

            Y_VERIFY(QueryOffsets.Size() > 0);
            const ui32 queryCount = QueryOffsets.Size() - 1;

            NKernel::QueryCrossEntropy(context.QidCursor,
                                       queryCount,
                                       Alpha,
                                       Targets.Get(),
                                       Weights.Get(),
                                       Values.Get(),
                                       reinterpret_cast<const int*>(LoadIndices.Get()),
                                       Qids.Get(),
                                       IsSingleClassQueries.Get(),
                                       QueryOffsets.Get(),
                                       static_cast<int>(Targets.Size()),
                                       FunctionValue.Get(),
                                       Ders.Get(),
                                       Ders2llp.Get(),
                                       Ders2llmax.Get(),
                                       GroupDers2.Get(),
                                       stream.GetStream());
        }
    };

    class TComputeQueryLogitMatrixSizesKernel: public TStatelessKernel {
    private:
        TCudaBufferPtr<const ui32> QueryOffsets;
        TCudaBufferPtr<const bool> IsSingleQueryFlags;
        TCudaBufferPtr<ui32> MatrixSize;

    public:
        TComputeQueryLogitMatrixSizesKernel() = default;

        TComputeQueryLogitMatrixSizesKernel(TCudaBufferPtr<const ui32> queryOffsets,
                                            TCudaBufferPtr<const bool> isSingleQueryFlag,
                                            TCudaBufferPtr<ui32> matrixSize)
                : QueryOffsets(queryOffsets)
                , IsSingleQueryFlags(isSingleQueryFlag)
                  , MatrixSize(matrixSize) {
        }

        Y_SAVELOAD_DEFINE(QueryOffsets, IsSingleQueryFlags, MatrixSize);

        void Run(const TCudaStream& stream) const {
            Y_VERIFY(QueryOffsets.Size() > 0);
            const ui32 queryCount = QueryOffsets.Size() - 1;

            NKernel::ComputeQueryLogitMatrixSizes(QueryOffsets.Get() ,IsSingleQueryFlags.Get(), queryCount, MatrixSize.Get(), stream.GetStream());
        }
    };

    class TMakeQueryLogitPairsKernel: public TStatelessKernel {
    private:
        TCudaBufferPtr<const ui32> QueryOffsets;
        TCudaBufferPtr<const ui32> MatrixOffset;
        TCudaBufferPtr<const bool> IsSingleQueryFlags;
        double MeanQuerySize;
        TCudaBufferPtr<uint2> Pairs;

    public:
        TMakeQueryLogitPairsKernel() = default;

        TMakeQueryLogitPairsKernel(TCudaBufferPtr<const ui32> qOffsets,
                                   TCudaBufferPtr<const ui32> matrixOffset,
                                   TCudaBufferPtr<const bool> isSingleQueryFlags,
                                   double meanQuerySize,
                                   TCudaBufferPtr<uint2> pairs)
                : QueryOffsets(qOffsets)
                , MatrixOffset(matrixOffset)
                ,  IsSingleQueryFlags(isSingleQueryFlags)
                  , MeanQuerySize(meanQuerySize)
                  , Pairs(pairs) {
        }

        Y_SAVELOAD_DEFINE(QueryOffsets, MatrixOffset, IsSingleQueryFlags, MeanQuerySize, Pairs);

        void Run(const TCudaStream& stream) const {
            Y_VERIFY(QueryOffsets.Size() > 0);
            const ui32 queryCount = QueryOffsets.Size() - 1;

            NKernel::MakeQueryLogitPairs(QueryOffsets.Get(),
                                         MatrixOffset.Get(),
                                         IsSingleQueryFlags.Get(),
                                         MeanQuerySize,
                                         queryCount,
                                         Pairs.Get(),
                                         stream.GetStream());
        }
    };

    class TMakeIsSingleClassFlagsKernel: public TStatelessKernel {
    private:
        TCudaBufferPtr<const float> Targets;
        TCudaBufferPtr<const ui32> LoadIndices;
        TCudaBufferPtr<const ui32> QueryOffsets;
        double MeanQuerySize;
        TCudaBufferPtr<bool> IsSingleClassQuery;

    public:
        TMakeIsSingleClassFlagsKernel() = default;

        TMakeIsSingleClassFlagsKernel(TCudaBufferPtr<const float> targets,
                                      TCudaBufferPtr<const ui32> loadIndices,
                                      TCudaBufferPtr<const ui32> queryOffsets,
                                      double meanQuerySize,
                                      TCudaBufferPtr<bool> isSingleClassQuery)
                : Targets(targets)
                , LoadIndices(loadIndices)
                  , QueryOffsets(queryOffsets)
                  , MeanQuerySize(meanQuerySize)
                  , IsSingleClassQuery(isSingleClassQuery) {
        }

        Y_SAVELOAD_DEFINE(Targets, QueryOffsets,  MeanQuerySize, LoadIndices, IsSingleClassQuery);

        void Run(const TCudaStream& stream) const {
            Y_VERIFY(QueryOffsets.Size() > 0);
            Y_VERIFY(LoadIndices.Size() == IsSingleClassQuery.Size());

            const ui32 queryCount = QueryOffsets.Size() - 1;
            NKernel::MakeIsSingleClassFlags(Targets.Get(), LoadIndices.Get(),
                                            QueryOffsets.Get(),
                                            queryCount,
                                            MeanQuerySize,
                                            IsSingleClassQuery.Get(), stream.GetStream());
        }
    };

    class TFillPairDer2OnlyKernel: public TStatelessKernel {
    private:
        TCudaBufferPtr<const float> Ders2;
        TCudaBufferPtr<const float> GroupDers2;
        TCudaBufferPtr<const ui32> Qids;
        TCudaBufferPtr<const uint2> Pairs;
        TCudaBufferPtr<float> PairDer2;

    public:
        TFillPairDer2OnlyKernel() = default;

        TFillPairDer2OnlyKernel(TCudaBufferPtr<const float> ders2,
                                TCudaBufferPtr<const float> groupDers2,
                                TCudaBufferPtr<const ui32> qids,
                                TCudaBufferPtr<const uint2> pairs,
                                TCudaBufferPtr<float> pairDer2)
                : Ders2(ders2)
                , GroupDers2(groupDers2)
                , Qids(qids)
                , Pairs(pairs)
                , PairDer2(pairDer2) {
        }

        Y_SAVELOAD_DEFINE(Ders2, GroupDers2, Qids, Pairs, PairDer2);

        void Run(const TCudaStream& stream) const {
            NKernel::FillPairDer2Only(Ders2.Get(),
                                      GroupDers2.Get(),
                                      Qids.Get(),
                                      Pairs.Get(),
                                      static_cast<ui32>(Pairs.Size()),
                                      PairDer2.Get(),
                                      stream.GetStream());
        }
    };


    class TFillPairDer2AndRemapPairDocumentsKernel: public TStatelessKernel {
    private:
        TCudaBufferPtr<const float> Ders2;
        TCudaBufferPtr<const float> GroupDers2;
        TCudaBufferPtr<const ui32> DocIds;
        TCudaBufferPtr<const ui32> Qids;
        TCudaBufferPtr<float> PairDer2;
        TCudaBufferPtr<uint2> Pairs;

    public:
        TFillPairDer2AndRemapPairDocumentsKernel() = default;

        TFillPairDer2AndRemapPairDocumentsKernel(TCudaBufferPtr<const float> ders2,
                                                 TCudaBufferPtr<const float> groupDers2,
                                                 TCudaBufferPtr<const ui32> docIds,
                                                 TCudaBufferPtr<const ui32> qids,
                                                 TCudaBufferPtr<float> pairDer2,
                                                 TCudaBufferPtr<uint2> pairs)
                : Ders2(ders2)
                  , GroupDers2(groupDers2)
                  , DocIds(docIds)
                  , Qids(qids)
                  , PairDer2(pairDer2)
                  , Pairs(pairs)
        {
        }

        Y_SAVELOAD_DEFINE(Ders2, GroupDers2, DocIds, Qids,  PairDer2, Pairs);

        void Run(const TCudaStream& stream) const {
            CB_ENSURE(PairDer2.Size() == Pairs.Size());
            NKernel::FillPairDer2AndRemapPairDocuments(Ders2.Get(), GroupDers2.Get(), DocIds.Get(), Qids.Get(), PairDer2.Size(), PairDer2.Get(), Pairs.Get(), stream.GetStream());
        }
    };
}



template <class TMapping>
inline void QueryCrossEntropy(double alpha,
                              const TCudaBuffer<const float, TMapping>& target,
                              const TCudaBuffer<const float, TMapping>& weights,
                              const TCudaBuffer<const float, TMapping>& point,
                              const TCudaBuffer<ui32, TMapping>& loadIndices,
                              const TCudaBuffer<ui32, TMapping>& qids,
                              const TCudaBuffer<bool, TMapping>& isSingleQueryFlags,
                              const TCudaBuffer<ui32, TMapping>& queryOffsets,
                              TCudaBuffer<float, TMapping>* score,
                              TCudaBuffer<float, TMapping>* weightedFullDer,
                              TCudaBuffer<float, TMapping>* weightedDer2NonShifted,
                              TCudaBuffer<float, TMapping>* weightedDer2Shifted,
                              TCudaBuffer<float, TMapping>* weightedGroupDer2,
                              ui32 stream = 0) {
    using TKernel = NKernelHost::TQueryCrossEntropyKernel;
    LaunchKernels<TKernel>(target.NonEmptyDevices(),
                           stream,
                           alpha,
                           target,
                           weights,
                           point,
                           loadIndices,
                           qids,
                           isSingleQueryFlags,
                           queryOffsets,
                           score,
                           weightedFullDer,
                           weightedDer2NonShifted,
                           weightedDer2Shifted,
                           weightedGroupDer2);
}


template <class TMapping>
inline void ComputeQueryLogitMatrixSizes(const TCudaBuffer<ui32, TMapping>& sampledQidOffsets,
                                         const TCudaBuffer<bool, TMapping>& sampledFlags,
                                         TCudaBuffer<ui32, TMapping>* matrixSizes,
                                         ui32 stream = 0) {
    using TKernel = NKernelHost::TComputeQueryLogitMatrixSizesKernel;
    LaunchKernels<TKernel>(sampledQidOffsets.NonEmptyDevices(),
                           stream,
                           sampledQidOffsets,
                           sampledFlags,
                           matrixSizes);
}



template <class TMapping>
inline void FillPairDer2AndRemapPairDocuments(const TCudaBuffer<float, TMapping>& ders2,
                                              const TCudaBuffer<float, TMapping>& queryDers2,
                                              const TCudaBuffer<ui32, TMapping>& docIds,
                                              const TCudaBuffer<ui32, TMapping>& qids,
                                              TCudaBuffer<float, TMapping>* pairDer2,
                                              TCudaBuffer<uint2, TMapping>* pairs,
                                              ui32 stream = 0) {
    using TKernel = NKernelHost::TFillPairDer2AndRemapPairDocumentsKernel;
    LaunchKernels<TKernel>(ders2.NonEmptyDevices(),
                           stream,
                           ders2,
                           queryDers2,
                           docIds,
                           qids,
                           pairDer2,
                           pairs);
}




template <class TMapping>
inline void MakePairsQueryLogit(const TCudaBuffer<ui32, TMapping>& sampledQidOffsets,
                                const TCudaBuffer<ui32, TMapping>& matrixOffsets,
                                const TCudaBuffer<bool, TMapping>& sampledFlags,
                                double meanQuerySize,
                                TCudaBuffer<uint2, TMapping>* pairs,
                                ui32 stream = 0) {
    using TKernel = NKernelHost::TMakeQueryLogitPairsKernel;
    LaunchKernels<TKernel>(sampledQidOffsets.NonEmptyDevices(),
                           stream,
                           sampledQidOffsets,
                           matrixOffsets,
                           sampledFlags,
                           meanQuerySize,
                           pairs);
}



template <class TMapping>
inline void MakeIsSingleClassQueryFlags(const TCudaBuffer<const float, TMapping>& targets,
                                        const TCudaBuffer<const ui32, TMapping>& order,
                                        const TCudaBuffer<const ui32, TMapping>& queryOffsets,
                                        double meanQuerySize,
                                        TCudaBuffer<bool, TMapping>* flags,
                                        ui32 stream = 0) {
    using TKernel = NKernelHost::TMakeIsSingleClassFlagsKernel;
    LaunchKernels<TKernel>(targets.NonEmptyDevices(),
                           stream,
                           targets,
                           order,
                           queryOffsets,
                           meanQuerySize,
                           flags);
}
