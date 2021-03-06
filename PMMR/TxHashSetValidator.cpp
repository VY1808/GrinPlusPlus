#include "TxHashSetValidator.h"
#include "TxHashSetImpl.h"
#include "KernelMMR.h"
#include "Common/MMR.h"
#include "Common/MMRUtil.h"
#include "Common/MMRHashUtil.h"

#include <Core/Validation/KernelSignatureValidator.h>
#include <Core/Validation/KernelSumValidator.h>
#include <Consensus/Common.h>
#include <Common/Util/HexUtil.h>
#include <Infrastructure/Logger.h>
#include <BlockChain/BlockChainServer.h>
#include <async++.h>

TxHashSetValidator::TxHashSetValidator(const IBlockChainServer& blockChainServer)
	: m_blockChainServer(blockChainServer)
{

}

// TODO: Where do we validate the data in MMR actually hashes to HashFile's hash?
std::unique_ptr<BlockSums> TxHashSetValidator::Validate(TxHashSet& txHashSet, const BlockHeader& blockHeader) const
{
	const KernelMMR& kernelMMR = *txHashSet.GetKernelMMR();
	const OutputPMMR& outputPMMR = *txHashSet.GetOutputPMMR();
	const RangeProofPMMR& rangeProofPMMR = *txHashSet.GetRangeProofPMMR();

	// Validate size of each MMR matches blockHeader
	if (!ValidateSizes(txHashSet, blockHeader))
	{
		LoggerAPI::LogError("TxHashSetValidator::Validate - Invalid MMR size.");
		return std::unique_ptr<BlockSums>(nullptr);
	}

	// Validate MMR hashes in parallel
	async::task<bool> kernelTask = async::spawn([this, &kernelMMR] { return this->ValidateMMRHashes(kernelMMR); });
	async::task<bool> outputTask = async::spawn([this, &outputPMMR] { return this->ValidateMMRHashes(outputPMMR); });
	async::task<bool> rangeProofTask = async::spawn([this, &rangeProofPMMR] { return this->ValidateMMRHashes(rangeProofPMMR); });

	const bool mmrHashesValidated = async::when_all(kernelTask, outputTask, rangeProofTask).then(
		[](std::tuple<async::task<bool>, async::task<bool>, async::task<bool>> results) -> bool {
		return std::get<0>(results).get() && std::get<1>(results).get() && std::get<2>(results).get();
	}).get();
	
	if (!mmrHashesValidated)
	{
		LoggerAPI::LogError("TxHashSetValidator::Validate - Invalid MMR hashes.");
		return std::unique_ptr<BlockSums>(nullptr);
	}

	// Validate root for each MMR matches blockHeader
	if (!txHashSet.ValidateRoots(blockHeader))
	{
		LoggerAPI::LogError("TxHashSetValidator::Validate - Invalid MMR roots.");
		return std::unique_ptr<BlockSums>(nullptr);
	}

	// Validate the full kernel history (kernel MMR root for every block header).
	if (!ValidateKernelHistory(*txHashSet.GetKernelMMR(), blockHeader))
	{
		LoggerAPI::LogError("TxHashSetValidator::Validate - Invalid kernel history.");
		return std::unique_ptr<BlockSums>(nullptr);
	}

	// Validate kernel sums
	std::unique_ptr<BlockSums> pBlockSums = ValidateKernelSums(txHashSet, blockHeader);
	if (pBlockSums == nullptr)
	{
		LoggerAPI::LogError("TxHashSetValidator::Validate - Invalid kernel sums.");
		return std::unique_ptr<BlockSums>(nullptr);
	}

	// Validate the rangeproof associated with each unspent output.
	if (!ValidateRangeProofs(txHashSet, blockHeader))
	{
		LoggerAPI::LogError("TxHashSetValidator::ValidateRangeProofs - Failed to verify rangeproofs.");
		return std::unique_ptr<BlockSums>(nullptr);
	}

	// Validate kernel signatures
	if (!ValidateKernelSignatures(*txHashSet.GetKernelMMR()))
	{
		LoggerAPI::LogError("TxHashSetValidator::ValidateKernelSignatures - Failed to verify kernel signatures.");
		return std::unique_ptr<BlockSums>(nullptr);
	}

	return pBlockSums;
}

bool TxHashSetValidator::ValidateSizes(TxHashSet& txHashSet, const BlockHeader& blockHeader) const
{
	if (txHashSet.GetKernelMMR()->GetSize() != blockHeader.GetKernelMMRSize())
	{
		LoggerAPI::LogError("TxHashSetValidator::ValidateSizes - Kernel size not matching for header " + HexUtil::ConvertHash(blockHeader.GetHash()));
		return false;
	}

	if (txHashSet.GetOutputPMMR()->GetSize() != blockHeader.GetOutputMMRSize())
	{
		LoggerAPI::LogError("TxHashSetValidator::ValidateSizes - Output size not matching for header " + HexUtil::ConvertHash(blockHeader.GetHash()));
		return false;
	}

	if (txHashSet.GetRangeProofPMMR()->GetSize() != blockHeader.GetOutputMMRSize())
	{
		LoggerAPI::LogError("TxHashSetValidator::ValidateSizes - RangeProof size not matching for header " + HexUtil::ConvertHash(blockHeader.GetHash()));
		return false;
	}

	return true;
}

// TODO: This probably belongs in MMRHashUtil.
bool TxHashSetValidator::ValidateMMRHashes(const MMR& mmr) const
{
	const uint64_t size = mmr.GetSize();
	for (uint64_t i = 0; i < size; i++)
	{
		const uint64_t height = MMRUtil::GetHeight(i);
		if (height > 0)
		{
			const std::unique_ptr<Hash> pParentHash = mmr.GetHashAt(i);
			if (pParentHash != nullptr)
			{
				const uint64_t leftIndex = MMRUtil::GetLeftChildIndex(i, height);
				const std::unique_ptr<Hash> pLeftHash = mmr.GetHashAt(leftIndex);

				const uint64_t rightIndex = MMRUtil::GetRightChildIndex(i);
				const std::unique_ptr<Hash> pRightHash = mmr.GetHashAt(rightIndex);

				if (pLeftHash != nullptr && pRightHash != nullptr)
				{
					const Hash expectedHash = MMRHashUtil::HashParentWithIndex(*pLeftHash, *pRightHash, i);
					if (*pParentHash != expectedHash)
					{
						LoggerAPI::LogError("TxHashSetValidator::ValidateMMRHashes - Invalid parent hash at index " + std::to_string(i));
						return false;
					}
				}
			}
		}
	}

	return true;
}

bool TxHashSetValidator::ValidateKernelHistory(const KernelMMR& kernelMMR, const BlockHeader& blockHeader) const
{
	for (uint64_t height = 0; height <= blockHeader.GetHeight(); height++)
	{
		std::unique_ptr<BlockHeader> pHeader = m_blockChainServer.GetBlockHeaderByHeight(height, EChainType::CANDIDATE);
		if (pHeader == nullptr)
		{
			LoggerAPI::LogError("TxHashSetValidator::ValidateKernelHistory - No header found at height " + std::to_string(height));
			return false;
		}
		
		if (kernelMMR.Root(pHeader->GetKernelMMRSize()) != pHeader->GetKernelRoot())
		{
			LoggerAPI::LogError("TxHashSetValidator::ValidateKernelHistory - Kernel root not matching for header at height " + std::to_string(height));
			return false;
		}
	}

	return true;
}

std::unique_ptr<BlockSums> TxHashSetValidator::ValidateKernelSums(TxHashSet& txHashSet, const BlockHeader& blockHeader) const
{
	// Calculate overage
	const int64_t overage = 0 - (Consensus::REWARD * (1 + blockHeader.GetHeight()));

	// Determine output commitments
	OutputPMMR* pOutputPMMR = txHashSet.GetOutputPMMR();
	std::vector<Commitment> outputCommitments;
	for (uint64_t i = 0; i < blockHeader.GetOutputMMRSize(); i++)
	{
		std::unique_ptr<OutputIdentifier> pOutput = pOutputPMMR->GetOutputAt(i);
		if (pOutput != nullptr)
		{
			outputCommitments.push_back(pOutput->GetCommitment());
		}
	}

	// Determine kernel excess commitments
	KernelMMR* pKernelMMR = txHashSet.GetKernelMMR();
	std::vector<Commitment> excessCommitments;
	for (uint64_t i = 0; i < blockHeader.GetKernelMMRSize(); i++)
	{
		std::unique_ptr<TransactionKernel> pKernel = pKernelMMR->GetKernelAt(i);
		if (pKernel != nullptr)
		{
			excessCommitments.push_back(pKernel->GetExcessCommitment());
		}
	}

	return KernelSumValidator::ValidateKernelSums(std::vector<Commitment>(), outputCommitments, excessCommitments, overage, blockHeader.GetTotalKernelOffset(), std::nullopt);
}

bool TxHashSetValidator::ValidateRangeProofs(TxHashSet& txHashSet, const BlockHeader& blockHeader) const
{
	std::vector<std::pair<Commitment, RangeProof>> rangeProofs;

	for (uint64_t mmrIndex = 0; mmrIndex < txHashSet.GetOutputPMMR()->GetSize(); mmrIndex++)
	{
		std::unique_ptr<OutputIdentifier> pOutput = txHashSet.GetOutputPMMR()->GetOutputAt(mmrIndex);
		if (pOutput != nullptr)
		{
			std::unique_ptr<RangeProof> pRangeProof = txHashSet.GetRangeProofPMMR()->GetRangeProofAt(mmrIndex);
			if (pRangeProof == nullptr)
			{
				LoggerAPI::LogError("TxHashSetValidator::ValidateRangeProofs - No rangeproof found at mmr index " + std::to_string(mmrIndex));
				return false;
			}

			rangeProofs.emplace_back(std::make_pair<Commitment, RangeProof>(Commitment(pOutput->GetCommitment()), RangeProof(*pRangeProof)));

			if (rangeProofs.size() >= 1000)
			{
				if (!Crypto::VerifyRangeProofs(rangeProofs))
				{
					return false;
				}

				rangeProofs.clear();
			}
		}
	}

	if (!rangeProofs.empty())
	{
		if (!Crypto::VerifyRangeProofs(rangeProofs))
		{
			return false;
		}
	}
	
	return true;
}

bool TxHashSetValidator::ValidateKernelSignatures(const KernelMMR& kernelMMR) const
{
	std::vector<TransactionKernel> kernels;

	const uint64_t mmrSize = kernelMMR.GetSize();
	const uint64_t numKernels = MMRUtil::GetNumLeaves(mmrSize);
	for (uint64_t i = 0; i < mmrSize; i++)
	{
		std::unique_ptr<TransactionKernel> pKernel = kernelMMR.GetKernelAt(i);
		if (pKernel != nullptr)
		{
			kernels.push_back(*pKernel);

			if (kernels.size() >= 2000)
			{
				if (!KernelSignatureValidator::VerifyKernelSignatures(kernels))
				{
					return false;
				}

				kernels.clear();
			}
		}
	}

	if (!kernels.empty())
	{
		if (!KernelSignatureValidator::VerifyKernelSignatures(kernels))
		{
			return false;
		}
	}

	return true;
}