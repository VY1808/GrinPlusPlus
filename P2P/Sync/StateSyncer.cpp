#include "StateSyncer.h"
#include "../BlockLocator.h"
#include "../ConnectionManager.h"
#include "../Messages/TxHashSetRequestMessage.h"

#include <BlockChain/BlockChainServer.h>
#include <Consensus/BlockTime.h>

StateSyncer::StateSyncer(ConnectionManager& connectionManager, IBlockChainServer& blockChainServer)
	: m_connectionManager(connectionManager), m_blockChainServer(blockChainServer)
{
	m_timeRequested = std::chrono::system_clock::now();
	m_requestedHeight = 0;
	m_connectionId = 0;
}

bool StateSyncer::SyncState(const SyncStatus& syncStatus)
{
	if (IsStateSyncDue(syncStatus))
	{
		RequestState(syncStatus);

		return true;
	}

	// If state sync is still in progress, return true to delay block sync.
	if (syncStatus.GetBlockHeight() < m_requestedHeight)
	{
		return true;
	}

	return false;
}

// NOTE: This doesn't handle re-orgs beyond the horizon.
bool StateSyncer::IsStateSyncDue(const SyncStatus& syncStatus) const
{
	const uint64_t headerHeight = syncStatus.GetHeaderHeight();
	const uint64_t blockHeight = syncStatus.GetBlockHeight();

	const ESyncStatus status = syncStatus.GetStatus();
	if (status == ESyncStatus::PROCESSING_TXHASHSET)
	{
		return false;
	}

	if (status == ESyncStatus::TXHASHSET_SYNC_FAILED)
	{
		return true;
	}

	// For the first week, there's no reason to request TxHashSet, since we can just download full blocks.
	if (headerHeight < Consensus::CUT_THROUGH_HORIZON)
	{
		return false;
	}

	// If block height is within threshold, just rely on block sync.
	if (blockHeight > (headerHeight - Consensus::CUT_THROUGH_HORIZON))
	{
		return false;
	}

	if (m_requestedHeight == 0)
	{
		return true;
	}

	// If state sync has already occurred, just rely on block sync.
	if (blockHeight >= m_requestedHeight)
	{
		return false;
	}

	// If TxHashSet download timed out, request it from another peer.
	if ((m_timeRequested + std::chrono::minutes(10)) < std::chrono::system_clock::now())
	{
		return true;
	}

	// If 30 seconds elapsed with no progress, try another peer.
	if ((m_timeRequested + std::chrono::seconds(30)) < std::chrono::system_clock::now())
	{
		const uint64_t downloaded = syncStatus.GetDownloaded();
		if (downloaded == 0)
		{
			return true;
		}
	}

	return false;
}

bool StateSyncer::RequestState(const SyncStatus& syncStatus)
{
	const uint64_t headerHeight = syncStatus.GetHeaderHeight();
	const uint64_t requestedHeight = headerHeight - Consensus::STATE_SYNC_THRESHOLD;
	Hash hash = m_blockChainServer.GetBlockHeaderByHeight(requestedHeight, EChainType::CANDIDATE)->GetHash();
	if (m_connectionId > 0)
	{
		m_connectionManager.BanConnection(m_connectionId, EBanReason::FraudHeight);
	}

	const TxHashSetRequestMessage txHashSetRequestMessage(std::move(hash), requestedHeight);
	m_connectionId = m_connectionManager.SendMessageToMostWorkPeer(txHashSetRequestMessage);

	if (m_connectionId > 0)
	{
		m_timeRequested = std::chrono::system_clock::now();
		m_requestedHeight = requestedHeight;
	}

	return m_connectionId > 0;
}