#pragma once

#include <string>
#include <Core/BlockSums.h>

// Forward Declarations
class Config;
class FullBlock;
class BlockHeader;
class Commitment;
class OutputIdentifier;
class IBlockChainServer;
class IBlockDB;
class Transaction;

class ITxHashSet
{
public:
	//
	// Validates all hashes, signatures, etc in the entire TxHashSet.
	// This is typically only used during initial sync.
	//
	virtual std::unique_ptr<BlockSums> ValidateTxHashSet(const BlockHeader& header, const IBlockChainServer& blockChainServer) = 0;

	//
	// Saves the commitments, MMR indices, and block height for all unspent outputs in the block.
	// This is typically only used during initial sync.
	//
	virtual bool SaveOutputPositions(const BlockHeader& blockHeader, const uint64_t firstOutputIndex) = 0;



	//
	// Returns true if the output has not been spent.
	//
	virtual bool IsUnspent(const OutputIdentifier& output) const = 0;

	//
	// Returns true if all inputs in the transaction are valid and unspent. Otherwise, false.
	//
	virtual bool IsValid(const Transaction& transaction) const = 0;

	//
	// Appends all new kernels, outputs, and rangeproofs to the MMRs, and prunes all of the inputs.
	//
	virtual bool ApplyBlock(const FullBlock& block) = 0;

	//
	// Validates that the kernel, output and rangeproof MMR roots match those specified in the given header.
	//
	virtual bool ValidateRoots(const BlockHeader& blockHeader) const = 0;



	//
	// Rewinds the kernel, output, and rangeproof MMRs to the given block.
	//
	virtual bool Rewind(const BlockHeader& header) = 0;

	//
	// Removes all outputs, rangeproofs, and kernels added by the block, and adds back any inputs spent from it.
	// This is the reverse of ApplyBlock, and should only be called for the most recently applied block.
	//
	virtual bool RewindBlock(const FullBlock& block) = 0;

	//
	// Flushes all changes to disk.
	//
	virtual bool Commit() = 0;

	//
	// Discards all changes since the last commit.
	//
	virtual bool Discard() = 0;



	virtual bool Snapshot(const BlockHeader& header) = 0;
	virtual bool Compact() = 0;
};